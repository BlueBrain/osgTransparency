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

#include "GL3IterativeDepthPartitioner.h"

#include "../MultiLayerParameters.h"
#include "../util/ShapeData.h"
#include "../util/constants.h"
#include "../util/extensions.h"
#include "../util/glerrors.h"
#include "../util/helpers.h"
#include "../util/loaders.h"
#include "../util/strings_array.h"

#include <osg/BlendEquation>
#include <osg/BlendFunc>
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
#  include <osg/BindImageTexture>
#endif
#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/TextureRectangle>
#include <osg/io_utils>

#include <boost/format.hpp>

#include <iostream>

#if defined(__APPLE__)
#ifndef GL_VERSION_3_0
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#endif
#endif

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
using boost::str;
using boost::format;
using namespace keywords;

/*
  Static definitions
*/
namespace
{
const std::string SHADER_PATH = "multilayer/depth_partition/iterative/";
const unsigned int ITERATIONS =
    ::getenv("OSGTRANSPARENCY_DEPTH_PARTITION_ITERATIONS") == 0
        ? 1
        : std::min(1l, strtol(::getenv(
                                  "OSGTRANSPARENCY_DEPTH_PARTITION_ITERATIONS"),
                              0, 10));
const bool ACCURATE_PIXEL_MIN_MAX =
    ::getenv("OSGTRANSPARENCY_NO_ACCURATE_MINMAX") == 0;
const bool HALF_FLOAT_MIN_MAX_TEXTURE = !ACCURATE_PIXEL_MIN_MAX;
}

/*
  Helper functions
*/
#ifndef NDEBUG

static struct GL3IterativeDepthPartitioner::DebugHelpers
{
    struct default_printer
    {
        template <typename T>
        void operator()(const T& x) const
        {
            std::cout << x;
        }
    };

    template <typename Printer>
    void readTexels(const char* message, osg::State& state,
                    osg::ref_ptr<osg::TextureRectangle>* textures, size_t len,
                    int channels, const Printer& printer)
    {
        if (!DepthPeelingBin::DEBUG_PARTITION.debugPixel())
            return;
        state.get<osg::GLExtensions>()->glMemoryBarrier(GL_ALL_BARRIER_BITS);
        const int col = DepthPeelingBin::DEBUG_PARTITION.column;
        const int row = DepthPeelingBin::DEBUG_PARTITION.row;
        const char* channelNames[4] = {"R", "RG", "RGB", "RGBA"};
        std::cout << message << std::endl;
        std::cout << "Reading texels " << col << ", " << row << '['
                  << channelNames[channels - 1] << ']' << std::endl;
        for (size_t i = 0; i < len; ++i)
        {
            std::cout << i << ": ";
            readTexel(state, textures[i], col, row, channels, printer);
            std::cout << std::endl;
        }
    }

    void readTexels(const char* message, osg::State& state,
                    osg::ref_ptr<osg::TextureRectangle>* textures, size_t len,
                    int channels)
    {
        readTexels(message, state, textures, len, channels, default_printer());
    }

    template <typename Printer>
    void readTexel(const char* message, osg::State& state,
                   osg::TextureRectangle* texture, int channels,
                   const Printer& printer)
    {
        state.get<osg::GLExtensions>()->glMemoryBarrier(GL_ALL_BARRIER_BITS);
        if (!DepthPeelingBin::DEBUG_PARTITION.debugPixel())
            return;
        const int col = DepthPeelingBin::DEBUG_PARTITION.column;
        const int row = DepthPeelingBin::DEBUG_PARTITION.row;
        const char* channelNames[4] = {"R", "RG", "RGB", "RGBA"};
        std::cout << message << std::endl;
        std::cout << "Reading texel " << col << ", " << row << '['
                  << channelNames[channels - 1] << ']' << std::endl;
        readTexel(state, texture, col, row, channels, printer);
        std::cout << std::endl;
    }

    void readTexel(const char* message, osg::State& state,
                   osg::TextureRectangle* texture, int channels)
    {
        readTexel(message, state, texture, channels, default_printer());
    }

protected:
    template <typename Printer>
    void readTexel(osg::State& state, osg::TextureRectangle* texture, int x,
                   int y, int channels, const Printer& printer)
    {
        osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();
        fbo->setAttachment(COLOR_BUFFERS[0],
                           osg::FrameBufferAttachment(texture));
        fbo->apply(state);
        osg::ref_ptr<osg::Image> image = new osg::Image();
        GLenum pixelFormat;
        switch (texture->getInternalFormatType())
        {
        case osg::Texture::SIGNED_INTEGER:
            pixelFormat = GL_INT;
            break;
        case osg::Texture::UNSIGNED_INTEGER:
            pixelFormat = GL_UNSIGNED_INT;
            break;
        default:
            pixelFormat = GL_FLOAT;
        };
        image->readPixels(x, y, 1, 1, texture->getSourceFormat(), pixelFormat);
        for (int i = 0; i < channels; ++i)
        {
            void* data = image->data();
            switch (texture->getInternalFormatType())
            {
            case osg::Texture::SIGNED_INTEGER:
                printer(((int*)data)[i]);
                break;
            case osg::Texture::UNSIGNED_INTEGER:
                printer(((unsigned int*)data)[i]);
                break;
            default:
                printer(((float*)data)[i]);
                break;
            }
            if (i != channels - 1)
                std::cout << ' ';
        }
    }

} debug_helpers;

namespace
{
struct uint_tuple_printer
{
    uint_tuple_printer(unsigned int chunks_)
        : chunks(chunks_)
        , count(0)
    {
    }
    void operator()(unsigned int a) const
    {
        /* 32 intervals and 32 bits per texture channel are assumed */
        unsigned int bitsPerChunk = 32 / chunks;
        for (unsigned int i = 0; i < 32; i += bitsPerChunk)
        {
            unsigned int value = a & ((1 << bitsPerChunk) - 1);
            count += value;
            std::cout << value << ' ';
            a >>= bitsPerChunk;
        }
    }
    const unsigned int chunks;
    mutable unsigned int count;
};

struct unpack_interval_printer
{
    void operator()(float x) const
    {
        unsigned int n = (unsigned int)x;
        std::cout << (n & 31) << ',';
        n >>= 5;
        while (n != 0)
        {
            std::cout << (n & 3) << ',';
            n >>= 2;
        }
    }
};
}
#else
namespace
{
struct uint_tuple_printer
{
    uint_tuple_printer(...) {}
};
struct unpack_interval_printer
{
};
}
static struct GL3IterativeDepthPartitioner::DebugHelpers
{
    static void readTexels(const char*, osg::State&, ...){};
    static void readTexel(const char*, osg::State&, ...){};
} debug_helpers;
#endif

/*
  Constructor
*/
GL3IterativeDepthPartitioner::GL3IterativeDepthPartitioner(
    const Parameters& parameters)
    : DepthPartitioner(parameters)
{
}

/*
  Member functions
*/
void GL3IterativeDepthPartitioner::computeDepthPartition(
    MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
    osgUtil::RenderLeaf*& previous)
{
    osg::State& state = *renderInfo.getState();
    osg::GLExtensions* ext = state.get<osg::GLExtensions>();

    if (_parameters.getNumSlices() < 2)
        return;

    /* Updating the quantile uniforms */
    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;
    unsigned int points = quantiles.size();
    for (size_t i = 0; i < quantiles.size(); ++i)
    {
        /* OSG doesn't check if the uniform value has changed before dirtying
           it, so we add that verification here. */
        float quantile;
        _quantiles->getElement(i, quantile);
        if (quantile != quantiles[i])
        {
            _quantiles->setElement(i, quantiles[i]);
            _quantiles2->setElement(i, quantiles[i]);
        }
    }

    /* Getting the min and max depths */
    _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                                    osg::FrameBufferAttachment(
                                        _minMaxTexture.get()));
    _auxiliaryBuffer->apply(state);

    osg::Viewport* viewport = renderInfo.getCurrentCamera()->getViewport();
    _viewport->setViewport(0, 0, viewport->width(), viewport->height());
    glScissor(0, 0, _viewport->width(), _viewport->height());

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glClearColor(-1000000000.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    /* Rendering scene */
    if (!ACCURATE_PIXEL_MIN_MAX)
        std::cerr << "Warning: approximate min/max depth computation"
                     " is not available in GL3"
                  << std::endl;
    render(bin, renderInfo, previous, _minMax.get(), _minMaxPrograms);
    checkGLErrors("after min/max calculation");

    debug_helpers.readTexel("min/max", state, _minMaxTexture.get(), 2);

    /* First histogram pass */
    /* Clearing textures */
    for (size_t i = 0; i < _countTextures.size(); ++i)
    {
        clearTexture(*renderInfo.getState(), _auxiliaryBuffer,
                     _countTextures[i].get());
#if not OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
        _countTextures[i]->bindToImageUnit(i, osg::Texture::READ_WRITE);
#endif
    }

    _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                                    osg::FrameBufferAttachment(
                                        _totalCountsTexture.get()));
    _auxiliaryBuffer->apply(state);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    /* Rendering scene */
    render(bin, renderInfo, previous, _firstCount.get(), _firstCountPrograms);
    uint_tuple_printer p1(32 / _countTextures.size());
    debug_helpers.readTexels("counts (read top-down)", state,
                             &_countTextures[0], _countTextures.size(), 1, p1);

    debug_helpers.readTexel("Real total counts", state,
                            _totalCountsTexture.get(), 1);

    ext->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    checkGLErrors("after first histogram count");

    /* First interval search */
    unsigned int buffersPerType = (points + 3) / 4;
    size_t nextBuffer = 0;
    for (unsigned int i = 0; i < buffersPerType; ++i, ++nextBuffer)
    {
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer],
                                        osg::FrameBufferAttachment(
                                            _codedIntervalsTextures[i].get()));
    }
    for (unsigned int i = 0; i < buffersPerType; ++i, ++nextBuffer)
    {
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer],
                                        osg::FrameBufferAttachment(
                                            _leftAccumTextures[i].get()));
    }
    _auxiliaryBuffer->apply(state);
    ext->glDrawBuffers(buffersPerType * 2, &GL_BUFFER_NAMES[0]);
    state.pushStateSet(_firstFindQuantileIntervals.get());
    state.apply();
    state.applyProjectionMatrix(0);
    state.applyModelViewMatrix(0);
    _quad->draw(renderInfo);
    state.popStateSet();
    checkGLErrors("after first interval search");

    unpack_interval_printer p2;
    debug_helpers.readTexels("first interval search", state,
                             _codedIntervalsTextures, buffersPerType, 4, p2);
    debug_helpers.readTexels("left accumulations", state, _leftAccumTextures,
                             buffersPerType, 4);

    /* Main approximation loop */
    for (unsigned int iteration = 1; iteration <= ITERATIONS; ++iteration)
    {
        _iteration->set((int)iteration);

        /* Fragment count */

        /* Clearing count textures. */
        for (unsigned int i = 0; i < points; ++i)
        {
            clearTexture(*renderInfo.getState(), _auxiliaryBuffer,
                         _countTextures[i].get(), osg::Vec4(0, 0, 0, 0));
#if not OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
            _countTextures[i]->bindToImageUnit(i, osg::Texture::READ_WRITE);
#endif
        }
        /* Rendering scene.
           In this pass nothing is really rendered to the framebuffer. */
        glDrawBuffer(GL_NONE);
        previous = 0;
        render(bin, renderInfo, previous, _countIteration.get(),
               _countIterationPrograms);
        ext->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        checkGLErrors(str(format("after iteration %1% count") % iteration));
        debug_helpers.readTexels(
            str(format("iteration %1%") % iteration).c_str(), state,
            &_countTextures[0], points, 1, p1);

        /* Quantile interval search */
        nextBuffer = 0;
        buffersPerType = (points + 3) / 4;
        for (unsigned int i = 0; i < buffersPerType; ++i, ++nextBuffer)
        {
            _auxiliaryBuffer->setAttachment(
                COLOR_BUFFERS[nextBuffer],
                osg::FrameBufferAttachment(_codedIntervalsTextures[i].get()));
        }
        for (unsigned int i = 0; i < buffersPerType; ++i, ++nextBuffer)
        {
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer],
                                            osg::FrameBufferAttachment(
                                                _leftAccumTextures[i].get()));
        }

        _auxiliaryBuffer->apply(state);
        ext->glDrawBuffers(buffersPerType * 2, &GL_BUFFER_NAMES[0]);
        state.pushStateSet(_findQuantileIntervals.get());
        state.apply();
        state.applyProjectionMatrix(0);
        state.applyModelViewMatrix(0);
        ext->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        _quad->draw(renderInfo);
        state.popStateSet();
        checkGLErrors(
            str(format("after iteration %1% interval search") % iteration));
        debug_helpers.readTexels("interval search", state,
                                 _codedIntervalsTextures, buffersPerType, 4,
                                 p2);
        debug_helpers.readTexels("left accumulations", state,
                                 _leftAccumTextures, buffersPerType, 4);
    }

    /* Projecting final approximate quantiles */
    _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                                    osg::FrameBufferAttachment(
                                        _depthPartitionTexture[0].get()));
    if (_depthPartitionTexture[1].valid())
        _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER1,
                                        osg::FrameBufferAttachment(
                                            _depthPartitionTexture[1].get()));
    _auxiliaryBuffer->apply(state);
    ext->glDrawBuffers(_depthPartitionTexture[1].valid() ? 2 : 1,
                       &GL_BUFFER_NAMES[0]);
    state.pushStateSet(_finalProjection.get());
    state.apply();
    state.applyProjectionMatrix(0);
    state.applyModelViewMatrix(0);
    _quad->draw(renderInfo);
    state.popStateSet();
    checkGLErrors("after final reprojection");

    debug_helpers.readTexels("final projection", state, _depthPartitionTexture,
                             points > 4 ? 2 : 1, 4);
}

void GL3IterativeDepthPartitioner::createBuffersAndTextures(
    const unsigned int width, const unsigned height)
{
    _auxiliaryBuffer = new osg::FrameBufferObject();

    const unsigned int slices = _parameters.getNumSlices();
    const unsigned int points = slices - 1;

    const int formats32[4] = {GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F_ARB};
    const int formats16[4] = {GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F_ARB};
    if (HALF_FLOAT_MIN_MAX_TEXTURE)
        _minMaxTexture =
            createTexture<osg::TextureRectangle>(width, height, formats16[1]);
    else
        _minMaxTexture =
            createTexture<osg::TextureRectangle>(width, height, formats32[1]);

    /* 8 histogram textures with four intervals per texture
       (8 bits per interval counter) yield 32 intervals.
       It seems that the bottleneck is in the atomic operation units so
       decreasing the number of count textures doesn't improve
       performance. */
    const int countTexturesCount = 8;
    _countTextures.resize(countTexturesCount);
    for (int i = 0; i < countTexturesCount; ++i)
        _countTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, GL_R32UI,
                                                 GL_RED_INTEGER);

    /* Total counts texture */
    _totalCountsTexture =
        createTexture<osg::TextureRectangle>(width, height, formats16[1]);

    /* Quantile search textures */
    for (unsigned int i = 0; i < (points + 3) / 4; ++i)
    {
        int codedFormat = ITERATIONS > 3 ? formats32[3] : formats16[3];
        _codedIntervalsTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, codedFormat);
        _leftAccumTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, formats16[3]);
    }

    int finalPoints = points;

    _depthPartitionTexture[0] = createTexture<osg::TextureRectangle>(
        width, height, formats32[finalPoints > 4 ? 3 : finalPoints - 1]);
    if (finalPoints > 4)
        _depthPartitionTexture[1] = createTexture<osg::TextureRectangle>(
            width, height, formats32[(finalPoints - 1) % 4]);
}

void GL3IterativeDepthPartitioner::createStateSets()
{
    unsigned int slices = _parameters.getNumSlices();
    unsigned int points = slices - 1;
    if (points == 0)
        return;

    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;

    _viewport = new osg::Viewport();
    _iteration = new osg::Uniform("iteration", 0);

    _createMinMaxCalculationStateSet();
    _createFirstCountStateSet();
    _createFirstFindQuantileIntervalsStateSet(quantiles);
    _createCountIterationStateSet(quantiles.size());
    _createFindQuantileIntervalsStateSet(quantiles);
    _createFinalReprojectionStateSet(quantiles.size());

    _quad = createQuad();
}

void GL3IterativeDepthPartitioner::updateShaderPrograms(
    const ProgramMap& extraShaders)
{
    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;

    _updateMinMaxCalculationPrograms(extraShaders);
    _updateFirstCountPrograms(extraShaders);
    _updateCountIterationPrograms(extraShaders, quantiles.size());
}

void GL3IterativeDepthPartitioner::_createMinMaxCalculationStateSet()
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    _minMax = new osg::StateSet;
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    attributes[new osg::BlendEquation(RGBA_MAX)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    setupStateSet(_minMax.get(), modes, attributes, uniforms);
}

void GL3IterativeDepthPartitioner::_updateMinMaxCalculationPrograms(
    const ProgramMap& extraShaders)
{
    std::map<std::string, std::string> vars;
    if (HALF_FLOAT_MIN_MAX_TEXTURE)
        vars["DEFINES"] = "#define HALF_FLOAT_MINMAX\n";
    std::string code =
        "//minmax.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "minmax.frag", vars);
    addPrograms(extraShaders, &_minMaxPrograms,
                _vertex_shaders = strings(sm("trivialShadeVertex();")),
                _fragment_shaders = strings(code));
}

void GL3IterativeDepthPartitioner::_createFirstCountStateSet()
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    _firstCount = new osg::StateSet;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    attributes[new osg::ColorMask(true, false, false, false)] = ON;

    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);

    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    setupTexture("minMaxTexture", _parameters.reservedTextureUnits,
                 *_firstCount, _minMaxTexture.get());
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;

    insertTextureArrayUniform(uniforms, "counts", 0, _countTextures.size());
    for (size_t i = 0; i != _countTextures.size(); ++i)
    {
        /* The texture unit numbers don't match the image unit numbers chosen
           above. This is on purpose. Actually we don't need the texture
           binding at all, but with OSG current design there's no other way
           of making the image unit binding effective. */
        _firstCount->setTextureAttributeAndModes(
            _parameters.reservedTextureUnits + 1 + i, _countTextures[i]);
    }

    setupStateSet(_firstCount, modes, attributes, uniforms);
}

void GL3IterativeDepthPartitioner::_updateFirstCountPrograms(
    const ProgramMap& extraShaders)
{
    std::map<std::string, std::string> vars;
    vars["DEFINES"] +=
        str(format("#define COUNT_TEXTURES %1%\n") % _countTextures.size());

    std::string code =
        "//first_count.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "first_count.frag", vars);
    addPrograms(extraShaders, &_firstCountPrograms,
                _vertex_shaders = strings(sm("shadeVertex();")),
                _fragment_shaders = strings(code));
}

void GL3IterativeDepthPartitioner::_createFirstFindQuantileIntervalsStateSet(
    const std::vector<float>& quantiles)
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    /* State to find the interval where each quantile is located */
    _firstFindQuantileIntervals = new osg::StateSet;
    /* Modes */
    modes[GL_DEPTH] = OFF;
    modes[GL_BLEND] = OFF;
    /* Attributes */
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    for (size_t i = 0; i < _countTextures.size(); ++i)
    {
        attributes[new osg::BindImageTexture(i, _countTextures[i],
                                             osg::BindImageTexture::READ_WRITE,
                                             GL_R32UI)];
    }
    /* Textures */
    int nextIndex = _parameters.reservedTextureUnits;
    insertTextureArrayUniform(uniforms, "countTextures", nextIndex,
                              _countTextures.size());
    setupTextureArray("countTextures", nextIndex, *_firstFindQuantileIntervals,
                      8, &_countTextures[0]);
    nextIndex += _countTextures.size();
    setupTexture("totalCountsTexture", nextIndex, *_firstFindQuantileIntervals,
                 _totalCountsTexture.get());
    /* Uniforms */
    _quantiles = new osg::Uniform();
    _quantiles->setName("quantiles");
    _quantiles->setNumElements(quantiles.size());
    _quantiles->setType(osg::Uniform::FLOAT);
    for (unsigned int i = 0; i < quantiles.size(); ++i)
        _quantiles->setElement(i, quantiles[i]);
    uniforms.insert(_quantiles.get());
    /* Workaround for issue
       https://bbpteam.epfl.ch/project/issues/browse/BBPRTN-48 */
    _quantiles2 = new osg::Uniform();
    _quantiles2->setName("quantiles[0]");
    _quantiles2->setNumElements(quantiles.size());
    _quantiles2->setType(osg::Uniform::FLOAT);
    for (unsigned int i = 0; i < quantiles.size(); ++i)
        _quantiles2->setElement(i, quantiles[i]);
    uniforms.insert(_quantiles2.get());

    /* Fragment shader code */
    std::map<std::string, std::string> vars;
    size_t points = quantiles.size();
    vars["DEFINES"] = str(format("#define POINTS %1%\n"
                                 "#define OPACITY_THRESHOLD %2%\n") %
                          points % _parameters.opacityThreshold);
    std::string code;
    code = "//first_find_quantiles_interval.frag\n" +
           readSourceAndReplaceVariables(
               SHADER_PATH + "first_find_quantiles_interval.frag", vars);

    /* Final setup */
    setupStateSet(_firstFindQuantileIntervals, modes, attributes, uniforms);
    addProgram(_firstFindQuantileIntervals,
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}

void GL3IterativeDepthPartitioner::_createCountIterationStateSet(
    const size_t points)
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    /* State for counting of how many fragments fall in each interval for
       the different quantile points. */
    _countIteration = new osg::StateSet;
    /* Modes */
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    /* Attributes */
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    for (size_t i = 0; i < _countTextures.size(); ++i)
    {
        attributes[new osg::BindImageTexture(i, _countTextures[i],
                                             osg::BindImageTexture::READ_WRITE,
                                             GL_R32UI)];
    }
    /* Textures */
    int nextIndex = _parameters.reservedTextureUnits;
    setupTexture("minMaxTexture", nextIndex++, *_countIteration,
                 _minMaxTexture.get());
    setupTexture("codedPreviousIntervalsTexture1", nextIndex++,
                 *_countIteration, _codedIntervalsTextures[0]);
    if (points > 4)
        setupTexture("codedPreviousIntervalsTexture2", nextIndex++,
                     *_countIteration, _codedIntervalsTextures[1]);

    /* Image buffers for fragment counting */
    assert(points < _countTextures.size());

    insertTextureArrayUniform(uniforms, "counts", 0, _countTextures.size());
    for (size_t i = 0; i != _countTextures.size(); ++i)
    {
        /* The texture unit numbers don't match the image unit numbers chosen
           above. This is on purpose. Actually we don't need the texture
           binding at all, but with OSG current design there's no other way
           of making the image unit binding effective. */
        _countIteration->setTextureAttributeAndModes(nextIndex++,
                                                     _countTextures[i]);
    }

    /* Uniforms */
    uniforms.insert(_iteration.get());
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    /* Final setup */
    setupStateSet(_countIteration, modes, attributes, uniforms);
}

void GL3IterativeDepthPartitioner::_updateCountIterationPrograms(
    const ProgramMap& extraShaders, const size_t points)
{
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define POINTS %1%\n") % points);
    std::string code1 =
        "//find_interval.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "find_interval.frag", vars);
    std::string code2 =
        "//count_iteration.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "count_iteration.frag",
                                      vars);
    addPrograms(extraShaders, &_countIterationPrograms,
                _vertex_shaders = strings(sm("trivialShadeVertex();")),
                _fragment_shaders = strings(code1, code2));
}

void GL3IterativeDepthPartitioner::_createFindQuantileIntervalsStateSet(
    const std::vector<float>& quantiles)
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    /* State to find the interval where each quantile is located */
    _findQuantileIntervals = new osg::StateSet;
    /* Modes */
    modes[GL_DEPTH] = OFF;
    /* Attributes */
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    /* Textures */
    const size_t points = quantiles.size();
    setupTexture("totalCountsTexture", 0, *_findQuantileIntervals,
                 _totalCountsTexture.get());
    setupTextureArray("countTextures", 1, *_findQuantileIntervals, 8,
                      &_countTextures[0]);
    setupTextureArray("leftAccumTextures", 9, *_findQuantileIntervals,
                      (points + 3) / 4, _leftAccumTextures);

    /* Uniforms */
    assert(_quantiles.get());
    uniforms.insert(_quantiles.get());
    uniforms.insert(_quantiles2.get());
    assert(_iteration.get());
    uniforms.insert(_iteration.get());
    /* Fragment shader code */
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define POINTS %1%\n") % points);
    const std::string code =
        "//find_quantiles_interval.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH +
                                          "find_quantiles_interval.frag",
                                      vars);
    /* Final setup */
    setupStateSet(_findQuantileIntervals, modes, attributes, uniforms);
    addProgram(_findQuantileIntervals,
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}

void GL3IterativeDepthPartitioner::_createFinalReprojectionStateSet(
    const unsigned int points)
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    _finalProjection = new osg::StateSet;
    /* Modes */
    modes[GL_DEPTH] = OFF;
    modes[GL_BLEND] = OFF;
    /* Attributes */
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    /* Textures */
    int nextIndex = 0;
    setupTexture("minMaxTexture", nextIndex++, *_finalProjection,
                 _minMaxTexture.get());
    setupTexture("totalCountsTexture", nextIndex++, *_finalProjection,
                 _totalCountsTexture.get());
    setupTextureArray("countTextures", nextIndex, *_finalProjection, points,
                      &_countTextures[0]);
    nextIndex += points;
    const unsigned numLeftTextures = (points + 3) / 4;
    setupTextureArray("leftAccumTextures", nextIndex, *_finalProjection,
                      numLeftTextures, _leftAccumTextures);
    nextIndex += numLeftTextures;
    setupTextureArray("codedPreviousIntervalsTextures", nextIndex,
                      *_finalProjection, numLeftTextures,
                      _codedIntervalsTextures);
    /* Uniforms */
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    uniforms.insert(_quantiles.get());
    uniforms.insert(_quantiles2.get());
    /* Fragment shader code */
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define POINTS %1%\n"
                                 "#define ITERATIONS %2%\n") %
                          points % ITERATIONS);
    if (!_parameters.unprojectDepths)
        vars["DEFINES"] += "#define PROJECT_Z\n";

    const std::string code =
        "//final_reprojection.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "final_reprojection.frag",
                                      vars);
    /* Final setup */
    setupStateSet(_finalProjection, modes, attributes, uniforms);
    addProgram(_finalProjection, _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}
}
}
}
