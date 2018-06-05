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

#ifndef OSGTRANSPARENCY_BASERENDERBIN_H
#define OSGTRANSPARENCY_BASERENDERBIN_H

#include <osgTransparency/api.h>
#include <osgTransparency/types.h>

#include <osg/Matrix>
#include <osgUtil/RenderBin>

#include <boost/shared_ptr.hpp>

namespace bbp
{
namespace osgTransparency
{
class OcclusionQueryGroup;

/**
   Base class for all transparent rendering render bins.
 */
class BaseRenderBin : public osgUtil::RenderBin
{
public:
    /*--- Public declarations ---*/

    class Parameters;

    /*--- Public member functions ---*/

    /**
       Adds a collection of shaders to use for objects with a given state set

       The algorithms implemented in derived classes use their own shaders
       internally. This implies that the objects to render cannot include
       a main entry point for vertex and fragment shaders as part of their
       state set (i.e. a fully linked osg::Program). At the same time, the
       internal shaders (vertex and fragment) are not complete, and
       they require some extra functions to make a program.

       These functions can be provided in a StateSet basis as an
       osg::Program with the required shaders attached.

       For vertex programs the functions to provide in are
       @code
       void shaderVertex(); // Write to gl_Position and other outputs
       void trivialShaderVertex(); // Simplified form just to compute the depth
       @endcode
       For fragment programs:
       @code
       vec4 shadeFragment(); // Return the final fragment color
       float fragmentAlpha();
       float fragmentDepth();
       @endcode
       code for vertices and fragments

       The state set configured this way must not have any shading program
       assigned through setAttributeAndModes, otherwise the behaviour is
       undefined.

       If the program lacks the vertex or fragment shader a default one
       will be provided. The default vertex shader has the following code:
       @code
       #version 130
       out vec3 normal;
       out vec4 color;
       void shadeVertex()
       {
            gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;
            normal = gl_NormalMatrix * gl_Normal;
            color = gl_Color;
       }
       void trivialShadeVertex()
       {
            shadeVertex();
       }
       @endcode
       The default fragment shader has the following code:
       @code
       #version 130
       in vec4 color;
       in vec3 normal;
       vec4 shadeFragment()
       {
           vec3 n = normalize(normal);
           const vec3 l = vec3(0.0, 0.0, 1.0);
           float lambertTerm = dot(n, l);
           if (lambertTerm < 0.0)
               lambertTerm = -lambertTerm;
           vec3 shadedColor = color.rgb * lambertTerm;
           return vec4(shadedColor, color.a);
       }
       float fragmentAlpha() { return color.a; }
       float fragmentDepth() { return gl_FragCoord.z; }
       @endcode

       @param stateSet The state set to configure
       @param extra The shaders to associate with the state set. If extra == 0
       the default shaders described above are assigned to the state set.

       This function is not thread safe.
     */
    void addExtraShadersForState(const osg::StateSet* stateSet,
                                 osg::Program* extra = 0);

    Parameters& getParameters() { return *_parameters; }
    const Parameters& getParameters() const { return *_parameters; }
    /** @internal */
    const ProgramMap& getExtraShaders() const { return *_extraShaders; }
protected:
    /*--- Protected member attributes ---*/

    boost::shared_ptr<Parameters> _parameters;
    boost::shared_ptr<ProgramMap> _extraShaders;

    /*--- Protected constructors destructor ---*/

    BaseRenderBin(const boost::shared_ptr<Parameters>& parameters =
                      boost::shared_ptr<Parameters>());

    BaseRenderBin(const BaseRenderBin& renderBin, const osg::CopyOp& copyop);

    /*--- Protected member functions ---*/

    virtual void sortImplementation();

    virtual void drawImplementation(osg::RenderInfo& renderInfo,
                                    osgUtil::RenderLeaf*& previous) = 0;

    void render(osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous,
                osg::StateSet* baseStateSet, ProgramMap& programs,
                osg::RefMatrix* projection = 0,
                OcclusionQueryGroup* queryGroup = 0);

public:
    /**
       Renders the bounds of the drawables from the RenderLeafs.
       The shapes map will be filled internally and can be reused for following
       frames but mustn't be shared between contexts. Dynamic resizing of
       bounding shapes is neither supported.
     */
    void renderBounds(osg::RenderInfo& renderInfo, osg::StateSet* baseStateSet,
                      ProgramMap& programs, BoundShapesStorage& shapes,
                      osg::RefMatrix* projection = 0);

private:
    /*--- Private member functions ---*/

    ProgramMap::iterator _findExtraShaders(const osgUtil::StateGraph* graph,
                                           ProgramMap& programs);
};
}
}
#endif
