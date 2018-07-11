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

#define GL_GLEXT_PROTOTYPES
#include <osg/Config>
#include <osg/GL>

#include <GL/glext.h>

#include "Canvas.h"
#include "Context.h"
#ifdef OSG_GL3_AVAILABLE
#include "GL3IterativeDepthPartitioner.h"
#else
#include "IterativeDepthPartitioner.h"
#endif
#include "../util/TextureDebugger.h"
#include "../util/extensions.h"
#include "../util/glerrors.h"
#include "../util/strings_array.h"
#include "../util/trace.h"

#include "osgTransparency/MultiLayerParameters.h"
#include "osgTransparency/OcclusionQueryGroup.h"

#include <osg/BlendFunc>
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
#  include <osg/BindImageTexture>
#endif
#include <osg/Geometry>
#include <osg/Texture2DArray>
#include <osg/ValueObject>
#include <osg/Version>
#include <osgViewer/Renderer>

#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>

#include <iomanip>
#include <iostream>

//#define SHOW_TEXTURES

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
namespace
{
const unsigned int MAX_NUM_RETRIES = 5;
const bool QUERY_LATENCY =
    ::getenv("OSGTRANSPARENCY_OCCLUSION_QUERY_LATENCY") != 0 &&
            strtol(getenv("OSGTRANSPARENCY_OCCLUSION_QUERY_LATENCY"), 0, 10) >=
                0
        ? strtol(getenv("OSGTRANSPARENCY_OCCLUSION_QUERY_LATENCY"), 0, 10)
        : 1;
const bool USE_GL_ANY_SAMPLES =
    ::getenv("OSGTRANSPARENCY_USE_GL_ANY_SAMPLES") != 0;
#ifdef OSG_GL3_AVAILABLE
const GLenum COLOR_BUFFER_FORMAT = GL_RGBA16F;
#endif
}

using boost::format;
using boost::str;

/*
  Constructor
*/
Canvas::Canvas(osg::RenderInfo& renderInfo, Context* context)
    : _context(context)
    , _camera(renderInfo.getCurrentCamera())
    , _pass(0)
    , _index(0)
    , _lastSamplesPassed(0)
    , _timesSamplesRepeated(0)
#ifdef OSG_GL3_AVAILABLE
    , _depthPartitioner(
          new GL3IterativeDepthPartitioner(context->getParameters()))
#else
    , _depthPartitioner(new IterativeDepthPartitioner(context->getParameters()))
#endif

{
    _camera->addObserver(this);

    float width = _camera->getViewport()->width();
    float height = _camera->getViewport()->height();

    _viewport = new osg::Viewport(0, 0, width, height);
    _maxWidth = width;
    _maxHeight = height;
    osg::Vec2d maxViewport;
    if (_camera->getUserValue("max_viewport_hint", maxViewport))
    {
        _maxWidth = maxViewport.x();
        _maxHeight = maxViewport.y();
    }

    /* Updating the uniform variables with the values of the projection matrix
       required for project and unproject points */
    _projection_33 = new osg::Uniform(osg::Uniform::FLOAT, "proj33");
    _projection_34 = new osg::Uniform(osg::Uniform::FLOAT, "proj34");

    _queryGroup = new OcclusionQueryGroup(renderInfo);
}

/*
  Destructor
*/
Canvas::~Canvas()
{
    if (_camera)
        _camera->removeObserver(this);
}

/*
  Member functions
*/
bool Canvas::valid(const osg::Camera* camera)
{
    if (!_camera)
        return false;

    const osg::Viewport* viewport = _camera->getViewport();
    const double width = viewport->width();
    const double height = viewport->height();
    osg::Vec2d maxViewport;
    if (_camera->getUserValue("max_viewport_hint", maxViewport))
    {
        std::max(width, maxViewport.x());
        std::max(height, maxViewport.y());
    }

    /* Testing if camera size has changed and if previous buffers can be
       reused. */
    return _camera == camera && width <= _maxWidth && height <= _maxHeight;
}

bool Canvas::checkFinished()
{
    if (!_pass)
        return false;

    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* Checking the result of the queries from last peel pass (or the first
       pass). */

    unsigned int samplesPassed = 0;
    unsigned int latestPass;
    const bool samplesAvailable =
        _queryGroup->checkQueries(latestPass, samplesPassed, QUERY_LATENCY);

    const Parameters& parameters = _context->getParameters();
    bool finished =
        ((samplesAvailable && samplesPassed <= parameters.samplesCutoff) ||
         (parameters.maximumPasses != 0 && _pass >= parameters.maximumPasses));

#ifndef NDEBUG
    if (DepthPeelingBin::PROFILE_DEPTH_PARTITION)
        std::cout << "Pass " << _pass << " samples: " << samplesPassed
                  << std::endl;
#endif

    if (!finished && samplesAvailable && !USE_GL_ANY_SAMPLES)
    {
        if (_lastSamplesPassed == samplesPassed)
        {
            if (++_timesSamplesRepeated == MAX_NUM_RETRIES)
            {
                std::cerr << "Possible infinite loop peeling, finishing"
                          << std::endl;
                finished = true;
#ifndef NDEBUG
                abort();
#endif
            }
        }
        else
        {
            _lastSamplesPassed = samplesPassed;
        }
    }

#ifndef NDEBUG
    if ((DepthPeelingBin::DEBUG_PARTITION.debugPixel() ||
         DepthPeelingBin::PROFILE_DEPTH_PARTITION) &&
        finished)
        std::cout << "Passes " << _pass << std::endl;
#endif

    return finished;
}

void Canvas::startFrame(MultiLayerDepthPeelingBin* bin,
                        osg::RenderInfo& renderInfo,
                        osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();

    if (!_peelFBO.valid())
    {
        _createBuffersAndTextures();
        _createStateSets();
        _quad = createQuad();
    }

    /* Reseting state */
    _index = 0;
    _timesSamplesRepeated = 0;
    _lastSamplesPassed = 0;
    _pass = 0;
    _queryGroup->reset();

    _updateShaderPrograms(*bin->_extraShaders);
    _updateProjectionMatrixUniforms();

    if (DepthPeelingBin::COMPUTE_MAX_DEPTH_COMPLEXITY)
    {
        assert(_depthPartitioner.get());
        const int depth =
            _depthPartitioner->computeMaxDepthComplexity(bin, renderInfo,
                                                         previous);
        std::cout << "Max_depth_complexity " << depth << std::endl;
    }

    if (slices > 1)
    {
        /* Split point calculations */
        _depthPartitioner->computeDepthPartition(bin, renderInfo, previous);
        if (DepthPeelingBin::PROFILE_DEPTH_PARTITION)
            _depthPartitioner->profileDepthPartition(bin, renderInfo, previous);
    }

    /* This seems to fix the problem with state management when more than
       1 slice is used in applications that have more complex scenegraphs
       than the cube grid example. */
    previous = 0;

    /* The scissor setup is needed for OSG cameras whose viewport is not
       at (0, 0) */
    const unsigned int width = getWidth();
    const unsigned int height = getHeight();
    glScissor(0, 0, width, height);
    _viewport->width() = width;
    _viewport->height() = height;

#ifdef OSG_GL3_AVAILABLE
    /* This initial clear texture is indirectly responsible of allocating
       the GPU memory of the textures. */
    clearTexture(state, _auxiliaryBuffer, _frontColors, osg::Vec4(0, 0, 0, 0),
                 false);
    clearTexture(state, _auxiliaryBuffer, _backColors, osg::Vec4(0, 0, 0, 0),
                 false);
#else
    /* Clearing blend buffers */
    glClearColor(0, 0, 0, 0);
    if (slices < 5)
    {
        for (unsigned int i = 0; i < slices * 2; ++i)
        {
            osg::FrameBufferAttachment buffer(
                _targetBlendColorTextures[i].get());
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[i], buffer);
        }
        _auxiliaryBuffer->apply(state);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        /* There are too many buffers to clear all them at once. */
        for (unsigned int i = 0; i < 2; ++i)
        {
            for (unsigned int j = 0; j < slices; ++j)
            {
                osg::FrameBufferAttachment buffer(
                    _targetBlendColorTextures[j * 2 + i].get());
                _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[j], buffer);
            }
            _auxiliaryBuffer->apply(state);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
#endif
}

void Canvas::peel(MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    _preparePeelFBOAndTextures(renderInfo);

    osgUtil::RenderLeaf* previous = _context->oldPrevious;

    checkGLErrors("before peel pass");
    _queryGroup->beginPass();
    if (!_pass)
    {
        bin->render(renderInfo, previous, _firstPassStateSet.get(),
                    _firstPassPrograms, 0, _queryGroup.get());

        checkGLErrors("after first pass");
    }
    else
    {
        bin->render(renderInfo, previous, _peelStateSet.get(),
                    _peelPassPrograms, 0, _queryGroup.get());
        checkGLErrors("after peel pass");
    }
#if !defined NDEBUG && defined SHOW_TEXTURES
    osg::State& state = *renderInfo.getState();
    _showPeelPassTextures(state);
    _debugAlphaAccumulationAtPeelPass();
#endif

    /* Swapping buffer indexing for next pass */
    _index = 1 - _index;
    ++_pass;
}

void Canvas::blend(osg::RenderInfo& renderInfo)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* Nothing to blend in the first pass. */
    if (_pass == 1)
        return;

#ifdef OSG_GL3_AVAILABLE
    /* Barrier to finalize all the color writes of the previous peel pass. */
    const osg::GLExtensions* extensions =
        renderInfo.getState()->get<osg::GLExtensions>();
    extensions->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT_EXT);
#else
    _blendSlices(renderInfo, false);
    _blendSlices(renderInfo, true);
#ifndef NDEBUG
    _debugAlphaAccumulationAtBlendPass(*renderInfo.getState());
#endif
#endif

    checkGLErrors("after blend pass");
}

void Canvas::_createBuffersAndTextures()
{
    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();
    /* Number of simultaneous depth buffers per peel pass. */
    if (slices > 1)
        _depthPartitioner->createBuffersAndTextures(_maxWidth, _maxHeight);

    /* Creating FBOs */
    _peelFBO = new osg::FrameBufferObject();
    _blendBuffers[0] = new osg::FrameBufferObject();
    _blendBuffers[1] = new osg::FrameBufferObject();
    _auxiliaryBuffer = new osg::FrameBufferObject();

    /* Creating ping-pong depth textures */
    for (unsigned int i = 0; i < (slices + 1) / 2; ++i)
    {
        _depthTextures[i][0] =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_RGBA32F_ARB);
        _depthTextures[i][1] =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_RGBA32F_ARB);
    }

#ifdef OSG_GL3_AVAILABLE
    /* Creating one texture array for the front layers of each slice
       and another one for the back layers. */
    for (unsigned int i = 0; i < 2; ++i)
    {
        osg::Texture2DArray* colors = new osg::Texture2DArray();
        colors->setTextureSize(_maxWidth, _maxHeight, slices);
        colors->setInternalFormat(COLOR_BUFFER_FORMAT);
        colors->setSourceFormat(GL_RGBA);
        if (i == 0)
            _frontColors = colors;
        else
            _backColors = colors;
    }
#else
    const unsigned int numColorTextures = ((slices + 3) / 4) * 2;
    const unsigned int numDepthBuffers = (slices + 1) / 2;
    for (unsigned int i = 0; i < numColorTextures; ++i)
    {
        osg::TextureRectangle* color =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_RGBA32F_ARB);
        _colorTextures[i] = color;
        osg::FrameBufferAttachment buffer(color);
        _peelFBO->setAttachment(COLOR_BUFFERS[i + numDepthBuffers], buffer);
    }

    /* Creating color textures for blending of each layer */
    bool front = true;
    for (unsigned int i = 0; i < slices * 2; ++i, front = !front)
    {
        _targetBlendColorTextures[i] =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_RGBA16F);
        osg::FrameBufferAttachment buffer(_targetBlendColorTextures[i].get());
        _blendBuffers[front ? 0 : 1]->setAttachment(COLOR_BUFFERS[i / 2],
                                                    buffer);
    }
#endif
}

void Canvas::finishFrame(osg::RenderInfo& renderInfo)
{
    osg::State& state = *renderInfo.getState();

#if !defined OSG_GL3_AVAILABLE && !defined NDEBUG
    if (::getenv("OSGTRANSPARENCY_SHOW_BLEND_TEXTURE"))
    {
        static TextureDebugger depth;
        const char* idx = ::getenv("OSGTRANSPARENCY_SHOW_BLEND_TEXTURE");
        depth.setWindowName(std::string("target blend texture ") + idx);
        depth.renderTexture(
            state.getGraphicsContext(),
            _targetBlendColorTextures[strtol(idx, 0, 10)].get(),
            TextureDebugger::GLSL(
                "vec4 transform(vec4 input) { return input; }\n"));
    }
#endif

    /* Note that this viewport is not the same as the offscreen one. */
    osg::Viewport* viewport = _camera->getViewport();
    _lowerLeftCorner->set(osg::Vec2(viewport->x(), viewport->y()));
    state.apply(_finalStateSet.get());
    state.applyProjectionMatrix(0);
    state.applyModelViewMatrix(0);
    viewport->apply(state);

    /* The destitation buffer is already bound */
    _quad->draw(renderInfo);
}

void Canvas::objectDeleted(void* object)
{
    if (_camera == object)
        _camera = 0;
}

void Canvas::_createStateSets()
{
    /* The rendering code may performed before any other object is drawn, that
       makes possible that the correct viewport hasn't been applied yet.
       The reason is really wierd, it seems that RenderStage's default
       viewport is always 800x600 regarless of window resizing or
       fullscreen mode.
       To avoid problems, we will add a viewport state attribute to all the
       steps of the rendering. */
    /** \bug Potential bugs here:
      - This doesn't work with all the stereo modes. Only LEFT_EYE and
        RIGHT_EYE have been checked to work correctly.
      - will it be easy to set the camera viewport in Equalizer.
      - RTT cameras might not work. */

    if (_context->getParameters().getNumSlices() > 1)
        _depthPartitioner->createStateSets();

    _createFirstPassStateSet();
    _createPeelStateSet();
    _createBlendStateSet();
    _createFinalCopyStateSet();
}

void Canvas::_createFirstPassStateSet()
{
    using namespace keywords;
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    const Parameters& parameters = _context->getParameters();

    _firstPassStateSet = new osg::StateSet;
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    attributes[new osg::BlendEquation(RGBA_MAX)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    setupStateSet(_firstPassStateSet.get(), modes, attributes, uniforms);
    /* Reserving the 4 first texture numbers for textures units used in the
       vertex shading */
    _depthPartitioner->addDepthPartitionExtraState(
        _firstPassStateSet.get(), parameters.reservedTextureUnits);
}

void Canvas::_createPeelStateSet()
{
    using namespace keywords;
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();
    const unsigned int numDepthBuffers = (slices + 1) / 2;

    _peelStateSet = new osg::StateSet;
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE | osg::StateAttribute::PROTECTED;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    attributes[new osg::BlendEquation(RGBA_MAX)] = ON_OVERRIDE;
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    /* Reserving texture numbers for textures units used in the user given
       vertex and fragment shading. This must be integer (otherwise glUniform
       will produce an invalid operation error). */
    int nextIndex = parameters.reservedTextureUnits;

    /* Uniforms for depth buffer textures. */
    insertTextureArrayUniform(uniforms, "depthBuffers", nextIndex,
                              numDepthBuffers);
    nextIndex += numDepthBuffers;
#ifdef OSG_GL3_AVAILABLE
    /* Output color uniforms for image units */

    /* Uniforms for image and texture units with the slice color buffers. */
    _peelStateSet->setTextureAttribute(nextIndex, _frontColors);
    uniforms.insert(new osg::Uniform("frontInColor", nextIndex++));
    _peelStateSet->setTextureAttribute(nextIndex, _backColors);
    uniforms.insert(new osg::Uniform("backInColor", nextIndex++));
    uniforms.insert(new osg::Uniform("frontOutColor", 0));
    uniforms.insert(new osg::Uniform("backOutColor", 1));
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
    attributes[new osg::BindImageTexture(0, _frontColors,
                                         osg::BindImageTexture::READ_WRITE,
                                         GL_RGBA32F_ARB)];
    attributes[new osg::BindImageTexture(1, _backColors,
                                         osg::BindImageTexture::READ_WRITE,
                                         GL_RGBA32F_ARB)];
#endif
#else
    /* Textures where front and back layers of each slices are blended */
    osg::ref_ptr<osg::TextureRectangle> frontBlendedTextures[8];
    for (unsigned int i = 0; i < slices; ++i)
        frontBlendedTextures[i] = _targetBlendColorTextures[i * 2];
    setupTextureArray("frontBlendedBuffers", nextIndex, *_peelStateSet, slices,
                      frontBlendedTextures);
    nextIndex += slices;
    osg::ref_ptr<osg::TextureRectangle> backBlendedTextures[8];
    for (unsigned int i = 0; i < slices; ++i)
        backBlendedTextures[i] = _targetBlendColorTextures[i * 2 + 1];
    setupTextureArray("backBlendedBuffers", nextIndex, *_peelStateSet, slices,
                      backBlendedTextures);
    nextIndex += slices;
#endif
    /* Setting up the depth partition extra stuff. */
    _depthPartitioner->addDepthPartitionExtraState(_peelStateSet.get(),
                                                   nextIndex);

    /* Eventual state initialization */
    setupStateSet(_peelStateSet.get(), modes, attributes, uniforms);
}

void Canvas::_createBlendStateSet()
{
#ifdef OSG_GL3_AVAILABLE
    /* This is not needed in GL3, the blending is done directly in the peel
       shader. */
    return;
#else
    using namespace keywords;
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    std::map<std::string, std::string> vars;

    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();

    osg::ref_ptr<osg::StateSet> baseBlendStateSet(new osg::StateSet);
    vars["DEFINES"] = str(format("#define SLICES %1%\n") % slices);
    const std::string code =
        "//blend.frag\n" +
        readSourceAndReplaceVariables("multilayer/blend.frag", vars);
    addProgram(baseBlendStateSet, _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE; /* Just in case some cull mode is
                                           inherited */
    modes[GL_BLEND] = ON;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;

    /* Texture units for the color buffers. */
    const size_t numColorBuffers = (slices + 3) / 4;
    insertTextureArrayUniform(uniforms, "colorTextures", 0, numColorBuffers);
    setupStateSet(baseBlendStateSet.get(), modes, attributes, uniforms);

    for (unsigned int i = 0; i != 2; ++i)
        _blendStateSets[i] = new osg::StateSet(*baseBlendStateSet);

    /* Front to back compositing */
    _blendStateSets[0]->setAttributeAndModes(
        new osg::BlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE), ON_PROTECTED);
    /* Back to front compositing */
    _blendStateSets[1]->setAttributeAndModes(
        new osg::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA), ON_PROTECTED);
#endif
}

void Canvas::_createFinalCopyStateSet()
{
    using namespace keywords;
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    std::map<std::string, std::string> vars;

    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();

    _finalStateSet = new osg::StateSet();
    vars["DEFINES"] = str(format("#define SLICES %1%\n") % slices);
    const std::string code =
        "//final_pass.frag\n" +
        readSourceAndReplaceVariables("multilayer/final_pass.frag", vars);
    addProgram(_finalStateSet.get(),
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
    _lowerLeftCorner = new osg::Uniform("corner", osg::Vec2(0, 0));
    uniforms.insert(_lowerLeftCorner);

    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE; /* Just in case some cull mode is
                                           inherited */
    modes[GL_BLEND] = ON;
    ;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)] =
        ON_PROTECTED;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_PROTECTED;

#ifdef OSG_GL3_AVAILABLE
    /* Image unit for the color buffers. */
    uniforms.insert(new osg::Uniform("frontColors", 0));
    uniforms.insert(new osg::Uniform("backColors", 1));
    _finalStateSet->setTextureAttributeAndModes(0, _frontColors);
    _finalStateSet->setTextureAttributeAndModes(1, _backColors);
#else
    setupTextureArray("blendBuffers", 1, *_finalStateSet, slices * 2,
                      _targetBlendColorTextures);
#endif
    setupStateSet(_finalStateSet.get(), modes, attributes, uniforms);
}

void Canvas::_updateProjectionMatrixUniforms()
{
    const osg::Matrix& projection = _camera->getProjectionMatrix();
    _projection_33->set((float)projection(2, 2));
    _projection_34->set((float)projection(3, 2));
    if (_context->getParameters().getNumSlices() > 1)
        _depthPartitioner->updateProjectionUniforms(projection);
}

void Canvas::_preparePeelFBOAndTextures(osg::RenderInfo& renderInfo)
{
    osg::State& state = *renderInfo.getState();
    GL2Extensions* ext = getGL2Extensions(state.getContextID());

    const Parameters& parameters = _context->getParameters();
    const unsigned int numSlices = parameters.getNumSlices();
    const unsigned int numDepthBuffers = (numSlices + 1) / 2;

    /* Setting up the peel FBO attachments and texture units for this pass. */
    for (unsigned int i = 0; i < numDepthBuffers; ++i)
    {
        osg::FrameBufferAttachment depth(_depthTextures[i][_index].get());
        _peelFBO->setAttachment(COLOR_BUFFERS[i], depth);
        if (_pass != 0)
        {
            _peelStateSet->setTextureAttributeAndModes(
                parameters.reservedTextureUnits + i,
                _depthTextures[i][1 - _index].get());
        }
    }
    _peelFBO->apply(state);

#if defined OSG_GL3_AVAILABLE && not OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
    /* For some unknown reason this is needed every frame. */
    _frontColors->bindToImageUnit(0, osg::Texture::WRITE_ONLY,
                                  COLOR_BUFFER_FORMAT, 0, true);
    _backColors->bindToImageUnit(1, osg::Texture::WRITE_ONLY,
                                 COLOR_BUFFER_FORMAT, 0, true);
#endif

    ext->glDrawBuffers(numDepthBuffers, &GL_BUFFER_NAMES[0]);
    glClearColor(-1.0, 0.0, -1.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (_pass != 0)
    {
#ifdef OSG_GL3_AVAILABLE
        /* For the following pass we just want to finish all previous
           operations before proceeding. */
        ext->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT_EXT);
#else
        const unsigned int numColorBuffers = (numSlices + 3) / 4 * 2;
        ext->glDrawBuffers(numColorBuffers, &GL_BUFFER_NAMES[numDepthBuffers]);
        union {
            float float_;
            unsigned int int_;
        } minusInf;
        minusInf.int_ = 0xFF800000;
        glClearColor(minusInf.float_, minusInf.float_, minusInf.float_,
                     minusInf.float_);
        glClear(GL_COLOR_BUFFER_BIT);
        ext->glDrawBuffers(numColorBuffers + numDepthBuffers,
                           &GL_BUFFER_NAMES[0]);
#endif
    }
}

#ifndef OSG_GL3_AVAILABLE
void Canvas::_blendSlices(osg::RenderInfo& renderInfo, const bool back)
{
    osg::State& state = *renderInfo.getState();
    unsigned int index = back;

    const Parameters& parameters = _context->getParameters();
    const unsigned int colorBuffers = (parameters.getNumSlices() + 3) / 4;
    for (unsigned int k = 0; k < colorBuffers; ++k)
    {
        _blendStateSets[index]->setTextureAttributeAndModes(
            k, _colorTextures[k * 2 + index].get());
    }

    _blendBuffers[index]->apply(state);
    state.pushStateSet(_blendStateSets[index].get());
    state.apply();
    _quad->draw(renderInfo);
    state.popStateSet();
}
#endif

void Canvas::_updateShaderPrograms(const ProgramMap& extraShaders)
{
    using namespace keywords;
    ProgramMap newShaders;
    std::map<std::string, std::string> vars;

    updateProgramMap(extraShaders, _extraShaders, newShaders);
    if (newShaders.empty())
        return;

    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();

    /* Updating the shaders internal to the depth partitioning algorithm. */
    if (parameters.getNumSlices() > 1)
        _depthPartitioner->updateShaderPrograms(newShaders);

    /* Updating first pass program map */
    vars.clear();
    vars["SLICES"] = str(format("%1%") % slices);
    if (parameters.unprojectDepths)
        vars["DEFINES"] = "#define UNPROJECT_DEPTH\n";
    std::string code =
        "//first_pass.frag\n" +
        readSourceAndReplaceVariables("multilayer/first_pass.frag", vars);
    addPrograms(newShaders, &_firstPassPrograms,
                _vertex_shaders = strings(sm("trivialShadeVertex();")),
                _fragment_shaders = strings(code));

    /* Picking the programs for the new state sets to update then with the
       shaders needed to use the depth partition */
    ProgramMap newPrograms;
    for (ProgramMap::const_iterator i = newShaders.begin();
         i != newShaders.end(); ++i)
        newPrograms[i->first] = _firstPassPrograms[i->first];
    _depthPartitioner->addDepthPartitionExtraShaders(newPrograms);

    /* Updating peel pass program map */
    vars.clear();
    vars["DEFINES"] =
        str(format("#define SLICES %1%\n") % slices) +
        (parameters.unprojectDepths ? "#define UNPROJECT_DEPTH\n" : "");
    if (parameters.opacityThreshold < 1.0)
        vars["DEFINES"] += str(format("#define OPACITY_THRESHOLD %1%\n") %
                               parameters.opacityThreshold);

    code = "//peel.frag\n" +
           readSourceAndReplaceVariables("multilayer/peel.frag", vars);

    addPrograms(newShaders, &_peelPassPrograms,
                _vertex_shaders = strings(sm("shadeVertex();")),
                _fragment_shaders = strings(code));

    /* Adding depth partition shaders */
    newPrograms.clear();
    for (ProgramMap::const_iterator i = newShaders.begin();
         i != newShaders.end(); ++i)
        newPrograms[i->first] = _peelPassPrograms[i->first];
    _depthPartitioner->addDepthPartitionExtraShaders(newPrograms);
}

#if defined NDEBUG || !defined SHOW_TEXTURES
void Canvas::_showPeelPassTextures(osg::State&)
{
}
#else
void Canvas::_showPeelPassTextures(osg::State& state)
{
    const Parameters& parameters = _context->getParameters();
    const unsigned int slices = parameters.getNumSlices();

    const unsigned int maxPasses = 1;
    const unsigned int maxLayers = 2;

    if (_pass == 0 && slices > 1)
    {
        static TextureDebugger depth;
        depth.setWindowName(std::string("split points "));
        depth.renderTexture(
            state.getGraphicsContext(),
            _depthPartitioner->getDepthPartitionTextureArray()[0].get(),
            TextureDebugger::GLSL(
                "vec4 transform(vec4 c) { return vec4(c.rgb, 1); }\n"));
    }

    std::stringstream wname;

    if (_pass < maxPasses)
    {
        static TextureDebugger depth[32];
        for (unsigned int i = 0; i < slices; ++i)
        {
            wname.str("");
            wname << "output depth texture, pass " << _pass << ", slice" << i;
            depth[_pass * slices + i].setWindowName(wname.str());
            depth[_pass * slices + i].renderTexture(
                state.getGraphicsContext(), _depthTextures[0][_index].get(),
                TextureDebugger::GLSL(i % 2 ? "vec4 transform(vec4 x) {return "
                                              "vec4(-x.r, x.g, 0, 1);}\n"
                                            : "vec4 transform(vec4 x) {return "
                                              "vec4(-x.b, x.a, 0, 1);}\n"));
        }
    }

    if (_pass > 0 && _pass < maxPasses)
    {
        static TextureDebugger tdebugs[32];

        for (unsigned int i = 0; i < maxLayers; ++i)
        {
            TextureDebugger& tdebug = tdebugs[_pass * 4 + i];
            wname.str("");
            wname << "pass " << _pass << " layer " << i;
            tdebug.setWindowName(wname.str());
#ifdef OSG_GL3_AVAILABLE
            glFinish();
            tdebug.renderTexture(state.getGraphicsContext(),
                                 i % 2 == 0 ? _frontColors : _backColors,
                                 i / 2);
#else
            const char* channels[] = {"r", "g", "b", "a"};
            const char* channel = channels[(i / 2) % 4];
            tdebug.renderTexture(
                 state.getGraphicsContext(),
                 _colorTextures[(i / 8) * 2 + i % 2].get(),
                 tdebug.GLSL(std::string(R"(
                 vec4 transform(vec4 x)
                 {
                     int c = int(floatBitsToInt(x.))" + channel + R"());
                     if (c != int(0xFF800000))
                     {
                         unsigned int alpha = unsigned int(c) >> 24;
                         if (alpha >= 129u)
                              alpha -= 2;
                         vec4 rgba = vec4(float((c >> 16) & 255) / 255.0,
                                          float((c >> 8) & 255) / 255.0,
                                          float(c & 255) / 255.0,
                                          float(alpha) / 253.0);
                         return vec4(rgba.rgb * rgba.a, rgba.a);
                     }
                     else
                     {
                         return vec4(1, 0, 1, 1);
                     }
                 })", false, "#extension GL_EXT_gpu_shader4 : enable\n"));
#endif
        }
    }

    checkGLErrors("after showing textures");
}
#endif

void Canvas::_debugAlphaAccumulationAtPeelPass()
{
    const DepthPeelingBin::DebugPartition& debugPartition =
        DepthPeelingBin::DEBUG_PARTITION;

    /* To be called from peel */
    if (!debugPartition.debugAlphaAccumulation())
        return;

    const Parameters& parameters = _context->getParameters();
    const unsigned int numDepthBuffers = (parameters.getNumSlices() + 1) / 2;

    osg::ref_ptr<osg::Image> image(new osg::Image());
    int layers = parameters.getNumSlices() * 2;

    for (size_t b = 0; b < numDepthBuffers; ++b)
    {
        glReadBuffer(GL_BUFFER_NAMES[b]);
        image->readPixels(0, 0, getWidth(), getHeight(), GL_RGBA, GL_FLOAT);
        if (debugPartition.debugPixel())
        {
            int col = debugPartition.column;
            int row = debugPartition.row;
            float* values = (float*)image->data(col, row);
            for (int i = 0; i < 4 && layers != 0; ++i, --layers, ++values)
            {
                if (values[i] != -1 && values[i] != 0)
                {
                    std::cout << "Pass " << _pass << std::endl;
                    std::cout << "Processesed pixel " << col << ' ' << row
                              << std::endl;
                    b = numDepthBuffers;
                    break;
                }
            }
        }
        else
        {
            const float* data = (float*)image->data(0, 0);
            for (int i = 0; i < image->s() * image->t() * 4; ++i)
            {
                if (data[i] != -1.0 && data[i] != 0.0)
                {
                    std::cout << "Pass " << _pass << std::endl;
                    std::cout << "Processesed pixel " << (i / 4 % image->s())
                              << ' ' << (i / 4 / image->s()) << " layer "
                              << (i % 4 + b * 4) << std::endl;
                    b = numDepthBuffers;
                    break;
                }
            }
        }
    }
}

void Canvas::_debugAlphaAccumulationAtBlendPass(osg::State& state)
{
    const DepthPeelingBin::DebugPartition& debugPartition =
        DepthPeelingBin::DEBUG_PARTITION;

    if (debugPartition.debugPixel() && debugPartition.debugAlphaAccumulation())
    {
        const int col = debugPartition.column;
        const int row = debugPartition.row;

        const Parameters& parameters = _context->getParameters();
        std::vector<float> alphas(parameters.getNumSlices() * 2);
        for (int j = 0; j < 2; ++j)
        {
            if (j == 0)
            {
                _blendBuffers[0]->apply(state);
                std::cout << "Front to back layers";
            }
            else
            {
                _blendBuffers[1]->apply(state);
                std::cout << "Back to front layers";
            }
            osg::ref_ptr<osg::Image> image(new osg::Image());
            for (size_t i = 0; i < parameters.getNumSlices(); ++i)
            {
                glReadBuffer(GL_BUFFER_NAMES[i]);
                image->readPixels(0, 0, getWidth(), getHeight(), GL_RGBA,
                                  GL_FLOAT);
                float value = ((float*)image->data(col, row))[3];
                std::cout << ' ' << std::setprecision(4) << value;
                alphas[i * 2 + j] = value;
            }
            std::cout << std::endl;
        }
        float alpha = 0;
        std::cout << "cumulative alpha";
        for (unsigned int i = 0; i < parameters.getNumSlices() * 2 - 1; ++i)
        {
            alpha += (1 - alpha) * alphas[i];
            std::cout << ' ' << alpha;
        }
        std::cout << std::endl;
    }
}
}
}
}
