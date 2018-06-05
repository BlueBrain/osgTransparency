/* Copyright (c) 2006-2018, École Polytechnique Fédérale de Lausanne (EPFL) /
 *                           Blue Brain Project and
 *                          Universidad Politécnica de Madrid (UPM)
 *                          Juan Hernando <juan.hernando@epfl.ch>
 *
 * This file is part of osgTransparency
 * <https://github.com/BlueBrain/osgTransparency>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "BaseRenderBin.h"
#include "BaseParameters.h"
#include "OcclusionQueryGroup.h"
#include "util/ShapeData.h"
#include "util/constants.h"
#include "util/trace.h"

#include <osg/Config>
#include <osg/GL>
#include <osg/ShapeDrawable>
#include <osg/Version>

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
/*
  Constructors/destructor
*/
BaseRenderBin::BaseRenderBin(const boost::shared_ptr<Parameters>& parameters)
    : osgUtil::RenderBin(SORT_BY_STATE)
    , _extraShaders(new ProgramMap())
{
    if (!parameters)
        _parameters.reset(new Parameters());
    else
        _parameters = parameters;
}

BaseRenderBin::BaseRenderBin(const BaseRenderBin& renderBin,
                             const osg::CopyOp& copyop)
    : osgUtil::RenderBin(renderBin, copyop)
    , _parameters(renderBin._parameters)
    , _extraShaders(renderBin._extraShaders)
{
}

/*
  Member functions
*/
void BaseRenderBin::addExtraShadersForState(const osg::StateSet* stateSet,
                                            osg::Program* extra)
{
    static osg::ref_ptr<osg::Shader> defaultVert;
    if (!defaultVert.valid())
    {
        defaultVert = new osg::Shader(osg::Shader::VERTEX);
        defaultVert->setShaderSource(
            "\
        #version 130\n\
        out vec3 normal;\n\
        out vec4 color;\n\
        void shadeVertex()\n\
        {\n\
            gl_Position = gl_ProjectionMatrix * \n\
                          gl_ModelViewMatrix * gl_Vertex;\n\
            normal = gl_NormalMatrix * gl_Normal;\n\
            color = gl_Color;\n\
        }\n\
        void trivialShadeVertex() \n\
        {\n\
            shadeVertex(); \n\
        }\n");
    }
    static osg::ref_ptr<osg::Shader> defaultFrag;
    if (!defaultFrag.valid())
    {
        defaultFrag = new osg::Shader(osg::Shader::FRAGMENT);
        defaultFrag->setShaderSource(
            "\
        #version 130\n\
        in vec4 color;\n\
        in vec3 normal;\n\
        vec4 shadeFragment()\n\
        {\n\
            vec3 n = normalize(normal);\n\
            const vec3 l = vec3(0.0, 0.0, 1.0);\n\
            float lambertTerm = dot(n, l);\n\
            if (lambertTerm < 0.0)\n\
                lambertTerm = -lambertTerm;\n\
            vec3 shadedColor = color.rgb * lambertTerm;\n\
            return vec4(shadedColor, color.a);\n\
        }\n\
        float fragmentAlpha() { return color.a; }\n\
        float fragmentDepth() { return gl_FragCoord.z; }\n");
    }

    bool useDefault = extra == 0;
    if (!useDefault)
    {
        bool hasVertexShader = false;
        bool hasFragmentShader = false;
        for (unsigned int i = 0; i != extra->getNumShaders(); ++i)
        {
            osg::Shader* shader = extra->getShader(i);
            /* It's really difficult to correctly determine whether the
               provided shaders are complete and will compile without
               rejecting valid shaders unless the sources are parsed.
               So we only check for existance. */
            if (shader->getType() == osg::Shader::VERTEX)
                hasVertexShader = true;
            if (shader->getType() == osg::Shader::FRAGMENT)
                hasFragmentShader = true;
        }
        if (!hasVertexShader)
        {
#ifndef NDEBUG
            std::cerr << "osgTransparency: no vertex shader provided. "
                         "Adding the default one"
                      << std::endl;
#endif
            extra->addShader(defaultVert);
        }
        if (!hasFragmentShader)
        {
#ifndef NDEBUG
            std::cerr << "osgTransparency: no fragment shader provided. "
                         "Adding the default one"
                      << std::endl;
#endif
            extra->addShader(defaultFrag);
        }
    }
    else
    {
        extra = new osg::Program();
        extra->addShader(defaultFrag);
        extra->addShader(defaultVert);
    }

    (*_extraShaders)[stateSet] = extra;
}

void BaseRenderBin::render(osg::RenderInfo& renderInfo,
                           osgUtil::RenderLeaf*& previous,
                           osg::StateSet* baseStateSet, ProgramMap& programs,
                           osg::RefMatrix* projection,
                           OcclusionQueryGroup* queryGroup)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();
    assert(getSortMode() == SORT_BY_STATE);

    osg::State& state = *renderInfo.getState();

#ifdef DEBUG_STATESETS
    unsigned int stackPosition = state.getStateSetStackSize();
    bool statePushed = false;
#else
    unsigned int numToPop =
        (previous ? osgUtil::StateGraph::numToPop(previous->_parent) : 0);
    if (numToPop > 1)
        --numToPop;
    unsigned int stackPosition = state.getStateSetStackSize() - numToPop;
#endif

    if (queryGroup && _parameters->singleQueryPerPass)
        queryGroup->beginQuery(0);

    unsigned int index = 0;

    assert(_renderLeafList.empty());

    for (StateGraphList::const_iterator i = _stateGraphList.begin();
         i != _stateGraphList.end(); ++i)
    {
        osgUtil::StateGraph* graph = *i;

        ProgramMap::iterator program = _findExtraShaders(graph, programs);
        assert(program != programs.end());
#ifndef DEBUG_STATESETS
        baseStateSet->setAttributeAndModes(program->second.get());
        state.insertStateSet(stackPosition, baseStateSet);
        /* We need to apply the current state because the cull traversal
           creates a state graph with doesn't include the extra shaders.
           This causes the code in RenderLeaf::render to not properly
           detect when the state needs to be applied between algorithm
           passes. */
        state.apply();
#endif

        for (osgUtil::StateGraph::LeafList::const_iterator
                 l = graph->_leaves.begin();
             l != graph->_leaves.end(); ++l, ++index)
        {
            osgUtil::RenderLeaf* leaf = l->get();

#ifdef DEBUG_STATESETS
            if (statePushed)
                state.popStateSet();
            baseStateSet->setAttributeAndModes(program->second.get());
            state.pushStateSet(baseStateSet);
            state.apply();
            statePushed = true;
#endif

            /* Checking if this render leaf needs to be renderer still. */
            if (queryGroup && !_parameters->singleQueryPerPass)
            {
                unsigned int samples = 0;
                if (queryGroup->lastestSamplesPassed(index, samples) &&
                    samples == 0)
                    continue;
                else
                    queryGroup->beginQuery(index);
            }

            /* Replacing the actual camera projection with the projection for
               the tile being rendered */
            osg::ref_ptr<osg::RefMatrix> oldProjection = leaf->_projection;
            if (projection)
                leaf->_projection = projection;

#ifndef NDEBUG
            if (leaf->_drawable->getUseDisplayList())
            {
                std::cerr << "RTNeuron: Alpha blended render bins don't support"
                          << " display lists" << std::endl;
                leaf->_drawable->dirtyDisplayList();
                leaf->_drawable->setUseDisplayList(false);
            }
#endif
            leaf->render(renderInfo, previous);
            previous = leaf;

            /* Restoring projection matrix */
            if (projection)
                leaf->_projection = oldProjection;

            if (queryGroup && !_parameters->singleQueryPerPass)
                queryGroup->endQuery();
        }

#ifndef DEBUG_STATESETS
        state.removeStateSet(stackPosition);
#endif
    }

#ifdef DEBUG_STATESETS
    state.popStateSetStackToSize(stackPosition);
#endif

    if (queryGroup && _parameters->singleQueryPerPass)
        queryGroup->endQuery();
}

void BaseRenderBin::sortImplementation()
{
    std::cerr << "Unimplemented: " << __FILE__ << ':' << __LINE__ << std::endl;
}

#ifdef OSG_GL3_AVAILABLE
void BaseRenderBin::renderBounds(osg::RenderInfo& /*renderInfo*/,
                                 osg::StateSet* /*baseStateSet*/,
                                 ProgramMap& /*programs*/,
                                 BoundShapesStorage& /*shapes*/,
                                 osg::RefMatrix* /*projection*/)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* To be adapted to GL3 without fixed pipeline */
    std::cerr << "Unimplemented: " << __FILE__ << ':' << __LINE__ << std::endl;
    abort();
}
#else
void BaseRenderBin::renderBounds(osg::RenderInfo& renderInfo,
                                 osg::StateSet* baseStateSet,
                                 ProgramMap& programs,
                                 BoundShapesStorage& shapes,
                                 osg::RefMatrix* projection)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    unsigned int stackPosition = state.getStateSetStackSize();
    osgUtil::RenderLeaf* previous = 0;

    if (projection)
        state.applyProjectionMatrix(projection);

    state.pushStateSet(baseStateSet);
    state.apply();

    for (StateGraphList::const_iterator i = _stateGraphList.begin();
         i != _stateGraphList.end(); ++i)
    {
        osgUtil::StateGraph* graph = *i;

        ProgramMap::iterator program = _findExtraShaders(graph, programs);
        assert(program != programs.end());
        baseStateSet->setAttributeAndModes(program->second.get());
        state.insertStateSet(stackPosition, baseStateSet);

        for (osgUtil::StateGraph::LeafList::const_iterator l =
                 graph->_leaves.begin();
             l != graph->_leaves.end(); ++l)
        {
            osgUtil::RenderLeaf* leaf = l->get();
            const osg::Drawable* drawable = leaf->getDrawable();

            /* Accessing the shape drawable for the bounding box of this render
               leaf */
            ShapeData& data = shapes[drawable];
            if (!data.shape.valid())
            {
#if OSG_VERSION_GREATER_OR_EQUAL(3, 3, 2)
                const osg::BoundingBox& bbox = drawable->getBoundingBox();
#else
                const osg::BoundingBox& bbox = drawable->getBound();
#endif
                osg::Box* box =
                    new osg::Box(bbox.center(), bbox.xMax() - bbox.xMin(),
                                 bbox.yMax() - bbox.yMin(),
                                 bbox.zMax() - bbox.zMin());
                data.shape = new osg::ShapeDrawable(box);
                static osg::ref_ptr<osg::TessellationHints> s_tsHints;
                if (!s_tsHints.valid())
                {
                    s_tsHints = new osg::TessellationHints();
                    s_tsHints->setCreateNormals(false);
                    s_tsHints->setCreateTextureCoords(false);
                }
                data.shape->setTessellationHints(s_tsHints.get());
            }
            /* Rendering the box */
            /* Copied and simplified from RenderLeaf.cpp */
            if (!projection)
                state.applyProjectionMatrix(leaf->_projection);
            state.applyProjectionMatrix(leaf->_projection);
            state.applyModelViewMatrix(leaf->_modelview);
            if (previous)
            {
                osgUtil::StateGraph* prev_rg = previous->_parent;
                osgUtil::StateGraph* prev_rg_parent = prev_rg->_parent;
                osgUtil::StateGraph* rg = leaf->_parent;
                if (prev_rg_parent != rg->_parent)
                {
                    osgUtil::StateGraph::moveStateGraph(state, prev_rg_parent,
                                                        rg->_parent);
                    state.apply(rg->getStateSet());
                }
                else if (rg != prev_rg)
                {
                    state.apply(rg->getStateSet());
                }
            }
            else
            {
                osgUtil::StateGraph::moveStateGraph(state, 0,
                                                    leaf->_parent->_parent);
                state.apply(leaf->_parent->getStateSet());
            }

            /* Generating a display list for this shape if necessary. */
            if (data.list == 0)
            {
                data.list =
                    osg::Drawable::generateDisplayList(state.getContextID());
                if (data.list == 0)
                    abort();
                glNewList(data.list, GL_COMPILE);
                data.shape->drawImplementation(renderInfo);
                glEndList();
            }
            /** \bug Doesn't work with elaborated GLSL programs. The offending
                one are probably those containing geometry shaders. */
            glCallList(data.list);

            previous = leaf;
        }

        state.removeStateSet(stackPosition);
    }
}
#endif

ProgramMap::iterator BaseRenderBin::_findExtraShaders(
    const osgUtil::StateGraph* graph, ProgramMap& programs)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    const osgUtil::StateGraph* node = graph;
    while (node)
    {
        const osg::StateSet* stateSet = node->getStateSet();
        ProgramMap::iterator program = programs.find(stateSet);
        if (program != programs.end())
            return program;
        else
            node = node->_parent;
    }

    std::cerr << "alphablend::RenderBin: " << this << ", " << getName()
              << std::endl
              << "Extra shaders to render leaf not found. "
                 "StateSets searched:";
    for (const osgUtil::StateGraph* g = graph->_parent; g != 0; g = g->_parent)
        std::cerr << ' ' << g->getStateSet();
    std::cerr << std::endl;

    return programs.end();
}
}
}
