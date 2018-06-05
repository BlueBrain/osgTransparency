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

// Including GL headers for various OpenGL tokens
#define GL_GLEXT_PROTOTYPES
#include <osg/GL>

#include "DepthPartitioner.h"

#include "../MultiLayerParameters.h"
#include "../util/constants.h"
#include "../util/helpers.h" // Before any other boost include
#include "../util/loaders.h"
#include "../util/strings_array.h"

#include <osg/BlendFunc>
#include <osg/ColorMask>
#include <osg/FrameBufferObject>
#include <osg/StateSet>
#include <osg/TextureRectangle>
#include <osgDB/WriteFile>

#include <boost/format.hpp>

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
namespace
{
osg::Vec3 _depthComplexityColor(const float a)
{
    static osg::Vec3 blue(0, 0, 1);
    static osg::Vec3 red(1, 0, 0);
    static osg::Vec3 yellow(1, 1, 0);
    static osg::Vec3 white(1, 1, 1);

    osg::Vec3 color;
    if (fabs(a - 0.25) < 0.25)
        color += blue * (1 - fabs(a - 0.25) / 0.25);
    if (fabs(a - 0.5) < 0.25)
        color += red * (1 - fabs(a - 0.5) / 0.25);
    if (fabs(a - 0.75) < 0.25)
        color += yellow * (1 - fabs(a - 0.75) / 0.25);
    if (fabs(a - 1.0) < 0.25)
        color += white * (1 - fabs(a - 1.0) / 0.25);
    return color;
}
}

/*
  Constructor
*/
DepthPartitioner::DepthPartitioner(const Parameters& parameters)
    : _parameters(parameters)
    , _projection_33(new osg::Uniform(osg::Uniform::FLOAT, "proj33"))
    , _projection_34(new osg::Uniform(osg::Uniform::FLOAT, "proj34"))
{
}

/*
  Destructor
*/
DepthPartitioner::~DepthPartitioner()
{
}

/*
  Member functions
*/

void DepthPartitioner::updateProjectionUniforms(const osg::Matrix& projection)
{
    _projection_33->set((float)projection(2, 2));
    _projection_34->set((float)projection(3, 2));
}

void DepthPartitioner::addDepthPartitionExtraState(
    osg::StateSet* stateSet, const unsigned int textureUnit)
{
    unsigned int slices = _parameters.getNumSlices();

    if (slices > 1)
    {
        size_t points = _parameters.getAdjustedNumPoints();
        setupTextureArray("depthPartitionTextures", textureUnit, *stateSet,
                          (points + 3) / 4, getDepthPartitionTextureArray());
    }
}

void DepthPartitioner::addDepthPartitionExtraShaders(ProgramMap& programs)
{
    unsigned int slices = _parameters.getNumSlices();

    using boost::str;
    using boost::format;
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define SLICES %1%\n") % slices);
    if (_parameters.alphaAwarePartition && slices > 1)
        vars["DEFINES"] += "#define ADJUST_QUANTILES_WITH_ALPHA\n";
    std::string code = readSourceAndReplaceVariables(
        "multilayer/depth_partition/check_partition.frag", vars);

    for (ProgramMap::const_iterator i = programs.begin(); i != programs.end();
         ++i)
    {
        osg::Program* program = i->second.get();
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, code));
    }
}

size_t DepthPartitioner::computeMaxDepthComplexity(
    MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
    osgUtil::RenderLeaf*& previous)
{
    osg::State& state = *renderInfo.getState();

    /* Creating count program and state */
    using boost::str;
    using boost::format;
    using namespace keywords;

    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    osg::Viewport* camViewport = renderInfo.getCurrentCamera()->getViewport();
    const unsigned int width = camViewport->width();
    const unsigned int height = camViewport->height();

    osg::ref_ptr<osg::StateSet> stateSet(new osg::StateSet());
    ProgramMap programs;

    std::string code =
        "float fragmentDepth();\n"
        "void main() {\n"
        "    fragmentDepth();\n"
        "    gl_FragColor.r = 1.0;\n"
        "}\n";
    addPrograms(*bin->_extraShaders, &programs,
                _vertex_shaders = strings(sm("shadeVertex();")),
                _fragment_shaders = strings(std::string(code)));
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;

    osg::Viewport* viewport = new osg::Viewport();
    viewport->setViewport(0, 0, width, height);
    attributes[viewport] = ON_OVERRIDE_PROTECTED;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    attributes[new osg::ColorMask(true, false, false, false)] = ON;
    setupStateSet(stateSet.get(), modes, attributes, uniforms);

    /* Preparing buffer and rendering */
    osg::ref_ptr<osg::FrameBufferObject> buffer = new osg::FrameBufferObject();
    osg::ref_ptr<osg::RenderBuffer> colorbuffer =
        new osg::RenderBuffer(width, height, GL_R32F);
    buffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                          osg::FrameBufferAttachment(colorbuffer.get()));
    buffer->apply(state);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    bin->render(renderInfo, previous, stateSet.get(), programs);

    osg::ref_ptr<osg::Image> image(new osg::Image());
    image->readPixels(0, 0, width, height, GL_RED, GL_FLOAT);
    size_t depth = 0;
    if (DepthPeelingBin::DEBUG_PARTITION.debugPixel())
    {
        depth = (size_t) *
                (GLfloat*)image->data(DepthPeelingBin::DEBUG_PARTITION.column,
                                      DepthPeelingBin::DEBUG_PARTITION.row);
    }
    else
    {
        for (int i = 0; i < image->s(); ++i)
        {
            for (int j = 0; j < image->t(); ++j)
            {
                depth =
                    std::max(depth, (size_t)((GLfloat*)image->data(i, j))[0]);
            }
        }
    }

    /* Writing depth partition to file */
    if (::getenv("TRANSPARENCY_SAVE_DEPTH_COMPLEXITY") != 0 &&
        !DepthPeelingBin::DEBUG_PARTITION.debugPixel())
    {
        osg::ref_ptr<osg::Image> map = new osg::Image();
        map->allocateImage(image->s(), image->t(), 1, GL_RGB, GL_UNSIGNED_BYTE);
        for (int i = 0; i < image->s(); ++i)
        {
            for (int j = 0; j < image->t(); ++j)
            {
                GLfloat* source = (GLfloat*)image->data(i, j);
                GLubyte* target = (GLubyte*)map->data(i, j);
                float a = source[0] / (float)depth;
                osg::Vec3 color = _depthComplexityColor(a);
                target[0] = osg::round(color[0] * 255);
                target[1] = osg::round(color[1] * 255);
                target[2] = osg::round(color[2] * 255);
            }
        }
        osgDB::writeImageFile(*map, "depthmap.png");
    }
    return depth;
}

void DepthPartitioner::profileDepthPartition(MultiLayerDepthPeelingBin* bin,
                                             osg::RenderInfo& renderInfo,
                                             osgUtil::RenderLeaf*& previous)
{
    using boost::str;
    using boost::format;
    using namespace keywords;

    /* There seems to be a problem in state management and in some cases
       (e.g. RTNeuron), the textures and uniforms are not correctly applied
       if this pointer is not set to 0. This is more a workaround that a
       real solution. */
    previous = 0;

    /* Rendering the scene using the depth partition profiling shaders.
       Per pixel fragments per slice counts will be output in two target
       textures. */
    osg::State& state = *renderInfo.getState();
    osg::ref_ptr<osg::StateSet> pointcount = new osg::StateSet;
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;

    osg::Viewport* camViewport = renderInfo.getCurrentCamera()->getViewport();
    const unsigned int width = camViewport->width();
    const unsigned int height = camViewport->height();
    osg::Viewport* viewport = new osg::Viewport(0, 0, width, height);
    attributes[viewport] = ON_OVERRIDE_PROTECTED;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    setupStateSet(pointcount.get(), modes, attributes, uniforms);

    glScissor(0, 0, width, height);

    ProgramMap programs;
    std::map<std::string, std::string> vars;
    const size_t slices = _parameters.getNumSlices();
    if (slices == 1)
        return;
    vars["SLICES"] = str(format("%d") % slices);
    if (_parameters.unprojectDepths)
        vars["DEFINES"] = "#define UNPROJECT_DEPTH\n";
    std::string code =
        "//profile.frag\n" +
        readSourceAndReplaceVariables("multilayer/depth_partition/profile.frag",
                                      vars);
    addPrograms(*bin->_extraShaders, &programs,
                _vertex_shaders = strings(sm("shadeVertex();")),
                _fragment_shaders = strings(code));
    addDepthPartitionExtraState(pointcount.get(), 0);
    addDepthPartitionExtraShaders(programs);

    osg::ref_ptr<osg::FrameBufferObject> buffer = new osg::FrameBufferObject();
    buffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                          osg::FrameBufferAttachment(
                              new osg::RenderBuffer(width, height,
                                                    GL_RGBA32F_ARB)));
    buffer->setAttachment(osg::Camera::COLOR_BUFFER1,
                          osg::FrameBufferAttachment(
                              new osg::RenderBuffer(width, height,
                                                    GL_RGBA32F_ARB)));
    buffer->apply(state);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    bin->render(renderInfo, previous, pointcount.get(), programs);

    /* Reading back per pixel data and printing the desired information */

    const DepthPeelingBin::DebugPartition& debugPartition =
        DepthPeelingBin::DEBUG_PARTITION;
    int minRow = 0, minCol = 0;
    int maxRow = height - 1, maxCol = width - 1;
    bool findBadPixel = debugPartition.findBadPixel();
    int offendingSize = 0;
    int offendingSlice = 0;

    if (debugPartition.findBadPixel())
    {
        offendingSlice = debugPartition.badPixelFeatures.first;
        offendingSize = debugPartition.badPixelFeatures.second;
    }
    else if (debugPartition.debugPixel())
    {
        minRow = minCol = maxRow = maxCol = 0;
    }
    osg::ref_ptr<osg::Image> image(new osg::Image());

    std::cout << "Depth_complexity";

    osg::Vec2 pixel;
    std::map<int, int> slice_counts[8];
    for (int r = 0; r < 1 || (r == 1 && slices > 2); ++r)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + r);
        if (debugPartition.debugPixel())
        {
            image->readPixels(debugPartition.column, debugPartition.row, 1, 1,
                              GL_RGBA, GL_FLOAT);
        }
        else
        {
            image->readPixels(0, 0, width, height, GL_RGBA, GL_FLOAT);
        }
        for (int i = minCol; i < maxCol + 1; ++i)
        {
            for (int j = minRow; j < maxRow + 1; ++j)
            {
                for (int k = 0; k < 4; ++k)
                {
                    const float value = ((float*)image->data(i, j))[k];
                    if (k + r * 4 == offendingSlice && value >= offendingSize)
                        pixel = osg::Vec2(i, j);
                    ++slice_counts[k + r * 4][value];
                }
            }
        }
    }
    std::cout << std::endl;
    for (unsigned int i = 0; i < slices; ++i)
    {
        std::cout << "slice " << i << ':';
        int c = 0;
        for (std::map<int, int>::reverse_iterator j = slice_counts[i].rbegin();
             j != slice_counts[i].rend() && c < 10; c += j->second, ++j)
        {
            std::cout << ' ' << j->first << '(' << j->second << ')';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    if (findBadPixel)
        std::cout << "Pixel with offending interval " << pixel[0] << ' '
                  << pixel[1] << std::endl;
}
}
}
}
