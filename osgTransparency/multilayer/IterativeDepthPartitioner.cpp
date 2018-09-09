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
#ifdef _WIN32
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>
#include <osg/GL>
#endif

#include "IterativeDepthPartitioner.h"

#include "../MultiLayerParameters.h"
#include "../util/ShapeData.h"
#include "../util/constants.h"
#include "../util/extensions.h"
#include "../util/glerrors.h"
#include "../util/helpers.h"
#include "../util/loaders.h"
#include "../util/strings_array.h"
#include "../util/trace.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/TextureRectangle>
#include <osgDB/WriteFile>

#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>

#if defined(__APPLE__)
#ifndef GL_VERSION_3_0
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

//#define DEBUG_TEXTURE_FORMATS

#ifndef NDEBUG
#include <iomanip>
#include <osg/io_utils>

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v)
{
    out << "[ ";
    std::for_each(v.begin(), v.end(), out << boost::lambda::_1 << ' ');
    return out << ']';
}
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
const bool DOUBLE_WIDTH = false;
const bool ROUND_UP_QUANTILES_COUNTS = false;
const std::string SHADER_PATH = "multilayer/depth_partition/iterative/";
const int ITERATIONS =
    ::getenv("OSGTRANSPARENCY_DEPTH_PARTITION_ITERATIONS") == 0
        ? 1
        : std::min(1l, strtol(::getenv(
                                  "OSGTRANSPARENCY_DEPTH_PARTITION_ITERATIONS"),
                              0, 10));
const bool ACCURATE_PIXEL_MIN_MAX =
    ::getenv("OSGTRANSPARENCY_NO_ACCURATE_MINMAX") == 0;
const bool HALF_FLOAT_MIN_MAX_TEXTURE = !ACCURATE_PIXEL_MIN_MAX;

struct Range
{
    Range(int x_, int y_)
        : x(x_)
        , y(y_)
    {
    }
    int x;
    int y;
};
}

/*
  Helper functions
*/
#ifndef NDEBUG
namespace
{
template <typename T>
std::ostream& out(std::ostream& o, const T& x, int, int)
{
    return o << x;
}

std::ostream& unpackIntervals(std::ostream& out, int f, int buffer,
                              int /* channel*/)
{
    if (buffer % 2 == 0)
    {
        out << (f & 31) << ',';
        f >>= 5;
        while (f != 0)
        {
            if (DOUBLE_WIDTH)
            {
                out << (f & 7) << ',';
                f >>= 3;
            }
            else
            {
                out << (f & 3) << ',';
                f >>= 2;
            }
        }
    }
    else
    {
        out << f;
    }
    return out;
}
}

static struct IterativeDepthPartitioner::DebugHelpers
{
    template <typename Functor>
    void readPixel(const std::string& message, const Range& buffers,
                   int components, const Functor& f)
    {
        if (!DepthPeelingBin::DEBUG_PARTITION.debugPixel())
            return;
        int col = DepthPeelingBin::DEBUG_PARTITION.column;
        int row = DepthPeelingBin::DEBUG_PARTITION.row;
        std::cout << message << std::endl;
        osg::ref_ptr<osg::Image> image(new osg::Image());
        std::map<float, float> counts;
        const char* channels[4] = {"R", "RG", "RGB", "RGBA"};
        std::cout << "Reading " << (buffers.y - buffers.x) << '['
                  << channels[components - 1] << ']' << std::endl;
        for (int b = buffers.x; b < buffers.y; ++b)
        {
            glReadBuffer(GL_BUFFER_NAMES[b]);
            image->readPixels(col, row, 1, 1, GL_RGBA, GL_FLOAT);
            for (int k = 0; k < components; ++k)
            {
                float value = ((float*)image->data())[k];
                counts[b * 4 + k] += value;
            }
        }
        int j = 0;
        for (std::map<float, float>::iterator i = counts.begin();
             i != counts.end(); ++i, ++j)
        {
            std::cout << i->first << ":" << std::setprecision(17);
            f(std::cout, i->second, j / components, j % components);
            if (j % components == components - 1)
                std::cout << std::endl;
            else
                std::cout << '\t';
        }
        if (j % components != 0)
            std::cout << std::endl;
    }

    void readPixel(const std::string& message, const Range& buffers,
                   int components)
    {
        readPixel(message, buffers, components, out<float>);
    }
} debug_helpers;
#else
namespace
{
void unpackIntervals()
{
}
}
static struct IterativeDepthPartitioner::DebugHelpers
{
    static void readPixel(const std::string&, ...) {}
} debug_helpers;
#endif

/*
  Constructor
*/
IterativeDepthPartitioner::IterativeDepthPartitioner(const Parameters& params)
    : DepthPartitioner(params)
{
}

/*
  Destructor
*/
IterativeDepthPartitioner::~IterativeDepthPartitioner()
{
}

/*
  Member functions
*/
void IterativeDepthPartitioner::createBuffersAndTextures(
    const unsigned int width, const unsigned height)
{
    /* Creating FBO */
    _auxiliaryBuffer = new osg::FrameBufferObject();

    const unsigned int slices = _parameters.getNumSlices();
    const unsigned int points = slices - 1;

/* Creating the texture with the partition points. */
#ifdef DEBUG_TEXTURE_FORMATS
    const int formats32[4] = {GL_RGBA32F_ARB, GL_RGBA32F_ARB, GL_RGBA32F_ARB,
                              GL_RGBA32F_ARB};
    const int* formats16 = formats32;
#else
    const int formats32[4] = {GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F_ARB};
    const int formats16[4] = {GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F_ARB};
#endif
    for (int i = 0; i < 8; ++i)
    {
        _countTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, formats16[3]);
    }
    for (unsigned int i = 0; i < (points + 3) / 4; ++i)
    {
        const unsigned int codedFormat =
            ITERATIONS > 3 ? formats32[3] : formats16[3];
        _codedIntervalsTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, codedFormat);
        _leftAccumTextures[i] =
            createTexture<osg::TextureRectangle>(width, height, formats16[3]);
    }
    if (HALF_FLOAT_MIN_MAX_TEXTURE)
    {
        _minMaxTexture =
            createTexture<osg::TextureRectangle>(width, height, formats16[1]);
    }
    else
    {
        _minMaxTexture =
            createTexture<osg::TextureRectangle>(width, height, formats32[1]);
    }
    if (_parameters.alphaAwarePartition)
    {
        /* In this case this texture also includes the approximate number
           of maximum layers required computed from the mean alpha value. */
        _totalCountsTexture =
            createTexture<osg::TextureRectangle>(width, height, formats16[1]);
    }
    else
    {
        _totalCountsTexture =
            createTexture<osg::TextureRectangle>(width, height, formats16[0]);
    }

    const unsigned int finalPoints = _parameters.getAdjustedNumPoints();
    _depthPartitionTexture[0] = createTexture<osg::TextureRectangle>(
        width, height, formats32[finalPoints > 4 ? 3 : finalPoints - 1]);
    if (finalPoints > 4)
        _depthPartitionTexture[1] = createTexture<osg::TextureRectangle>(
            width, height, formats32[(finalPoints - 1) % 4]);
}

void IterativeDepthPartitioner::createStateSets()
{
    const unsigned int slices = _parameters.getNumSlices();
    const unsigned int points = slices - 1;
    if (points == 0)
        return;

    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;

    _viewport = new osg::Viewport();
    _iteration = new osg::Uniform("iteration", 0);

    _createMinMaxCalculationStateSet();
    _createFirstCountStateSet();
    _createCountIterationStateSet(quantiles.size());
    _createFirstFindQuantileIntervalsStateSet(quantiles);
    _createFindQuantileIntervalsStateSet(quantiles);
    _createFinalReprojectionStateSet(quantiles.size());

    _quad = createQuad();
}

void IterativeDepthPartitioner::updateShaderPrograms(
    const ProgramMap& extraShaders)
{
    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;

    _updateMinMaxCalculationPrograms(extraShaders);
    _updateFirstCountPrograms(extraShaders);
    _updateCountIterationPrograms(extraShaders, quantiles.size());
}

void IterativeDepthPartitioner::computeDepthPartition(
    MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
    osgUtil::RenderLeaf*& previous)
{
    osg::State& state = *renderInfo.getState();
    GL2Extensions* ext = getGL2Extensions(state.getContextID());

    if (_parameters.getNumSlices() < 2)
        return;

    static bool done = false;
    if (done)
        return;
    if (::getenv("OSGTRANSPARENCY_DO_SPLIT_TEXTURE_ONCE") != 0)
        done = true;

    checkGLErrors("before computing depth partition");

    /* PLEASE NOTE: All the glFlush commands executed in this function may
       improve performance. The reasons are unclear, but it seems to be true
       for low to moderate depth complexities. What seems clear is that the
       profiling measures become more stable in most cases. */

    /* Getting the min and max depths */
    _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                                    osg::FrameBufferAttachment(
                                        _minMaxTexture.get()));
    _auxiliaryBuffer->apply(state);

    /* Updating viewport and scissor. The scissor setup is needed for
       OSG cameras whose viewport is not
       at (0, 0) */
    osg::Viewport* viewport = renderInfo.getCurrentCamera()->getViewport();
    _viewport->setViewport(0, 0, viewport->width(), viewport->height());
    glScissor(0, 0, _viewport->width(), _viewport->height());

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glClearColor(-1000000000.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    /* Rendering scene */
    if (ACCURATE_PIXEL_MIN_MAX)
        render(bin, renderInfo, previous, _minMax.get(), _minMaxPrograms);
    else
        renderBounds(bin, renderInfo, _minMax.get(), _minMaxPrograms, _shapes);

    checkGLErrors("after min/max calculation");

    debug_helpers.readPixel("min/max", Range(0, 1), 2);

    /* Updating the quantile uniforms */
    const Parameters::QuantileList& quantiles = _parameters.splitPointQuantiles;
    unsigned int points = quantiles.size();
    for (unsigned int i = 0; i < quantiles.size(); ++i)
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
#ifndef NDEBUG
    if (DepthPeelingBin::DEBUG_PARTITION.debugPixel())
        std::cout << "quantiles " << quantiles << std::endl;
#endif

    /* First iteration */
    for (int i = 0; i < 8; ++i)
    {
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[i],
                                        osg::FrameBufferAttachment(
                                            _countTextures[i].get()));
    }

    _auxiliaryBuffer->apply(state);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    debug_helpers.readPixel("first count after clear", Range(0, 8), 4);
    /** \bug For this cases, there should be a better method to ensure than
        parent state is applied. Setting previous to 0 has the overhead
        of traversing and applying the whole state graph */
    previous = 0;
    render(bin, renderInfo, previous, _firstCount.get(), _firstCountPrograms);
    checkGLErrors("after first count");

    debug_helpers.readPixel("first count", Range(0, 8), 4);

    /* First interval search */
    {
        size_t nextBuffer = 0;
        for (unsigned int i = 0; i < (points + 3) / 4; ++i)
        {
            assert(_codedIntervalsTextures[i].get());
            assert(_leftAccumTextures[i].get());
            _auxiliaryBuffer->setAttachment(
                COLOR_BUFFERS[nextBuffer++],
                osg::FrameBufferAttachment(_codedIntervalsTextures[i].get()));
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer++],
                                            osg::FrameBufferAttachment(
                                                _leftAccumTextures[i].get()));
        }
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer++],
                                        osg::FrameBufferAttachment(
                                            _totalCountsTexture.get()));
    }

    _auxiliaryBuffer->apply(state);
    const unsigned int intAndAddBuffers = (points + 3) / 4 * 2;
    ext->glDrawBuffers(intAndAddBuffers + 1, &GL_BUFFER_NAMES[0]);
    /* It may not be necessary to clear the buffers but we keep it for the
       moment. */
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    state.pushStateSet(_firstFindQuantileIntervals.get());
    state.apply();
    state.applyProjectionMatrix(0);
    state.applyModelViewMatrix(0);
    _quad->draw(renderInfo);
    state.popStateSet();
    checkGLErrors("after first interval search");

    debug_helpers.readPixel("first interval search", Range(0, intAndAddBuffers),
                            points > 4 ? 4 : points);
    debug_helpers.readPixel(_parameters.alphaAwarePartition
                                ? "total layers and last interval"
                                : "total",
                            Range(intAndAddBuffers, intAndAddBuffers + 1),
                            _parameters.alphaAwarePartition ? 2 : 1);

    /* Main approximation loop */
    for (int iteration = 1; iteration <= ITERATIONS; ++iteration)
    {
        _iteration->set(iteration);
        /* Fragment count */
        const unsigned countBuffers =
            points <= 4 && DOUBLE_WIDTH ? points * 2 : points;
        for (unsigned int i = 0; i < countBuffers; ++i)
        {
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[i],
                                            osg::FrameBufferAttachment(
                                                _countTextures[i].get()));
        }
        _auxiliaryBuffer->apply(state);
        ext->glDrawBuffers(countBuffers, &GL_BUFFER_NAMES[0]);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        /* Rendering scene */
        previous = 0;
        render(bin, renderInfo, previous, _countIteration.get(),
               _countIterationPrograms);
        checkGLErrors(str(format("after iteration %1% count") % iteration));

        debug_helpers.readPixel(str(format("iteration %1%") % iteration),
                                Range(0, countBuffers), 4);

        /* Quantile interval search */
        unsigned int nextBuffer = 0;
        for (unsigned int i = 0; i < (points + 3) / 4; ++i)
        {
            _auxiliaryBuffer->setAttachment(
                COLOR_BUFFERS[nextBuffer++],
                osg::FrameBufferAttachment(_codedIntervalsTextures[i].get()));
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[nextBuffer++],
                                            osg::FrameBufferAttachment(
                                                _leftAccumTextures[i].get()));
        }
        _auxiliaryBuffer->apply(state);
        ext->glDrawBuffers(((points + 3) / 4) * 2, &GL_BUFFER_NAMES[0]);

        state.pushStateSet(_findQuantileIntervals.get());
        state.apply();
        state.applyProjectionMatrix(0);
        state.applyModelViewMatrix(0);
        _quad->draw(renderInfo);
        state.popStateSet();
        checkGLErrors(
            str(format("after iteration %1% interval search") % iteration));
        debug_helpers.readPixel("interval search",
                                Range(0, (points + 3) / 4 * 2),
                                points > 4 ? 4 : points, unpackIntervals);
    }

    /* Projecting final approximate quantiles */
    _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER0,
                                    osg::FrameBufferAttachment(
                                        _depthPartitionTexture[0].get()));
    if (_depthPartitionTexture[1].valid())
    {
        _auxiliaryBuffer->setAttachment(osg::Camera::COLOR_BUFFER1,
                                        osg::FrameBufferAttachment(
                                            _depthPartitionTexture[1].get()));
    }
    _auxiliaryBuffer->apply(state);
    ext->glDrawBuffers(_depthPartitionTexture[1].valid() ? 2 : 1,
                       &GL_BUFFER_NAMES[0]);
    /* Might not be necessary to clear the buffer */
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    state.pushStateSet(_finalProjection.get());
    state.apply();
    state.applyProjectionMatrix(0);
    state.applyModelViewMatrix(0);
    _quad->draw(renderInfo);
    state.popStateSet();
    checkGLErrors("after final reprojection");

    if (_parameters.alphaAwarePartition)
        ++points;
    debug_helpers.readPixel("final projection", Range(0, (points + 3) / 4),
                            points > 4 ? 4 : points);
}

void IterativeDepthPartitioner::_createMinMaxCalculationStateSet()
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
    attributes[new osg::ColorMask(true, true, false, false)] = ON;
    setupStateSet(_minMax.get(), modes, attributes, uniforms);
}

void IterativeDepthPartitioner::_updateMinMaxCalculationPrograms(
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

void IterativeDepthPartitioner::_createFirstCountStateSet()
{
    Modes modes;
    Attributes attributes;
    Uniforms uniforms;
    _firstCount = new osg::StateSet;
    if (_parameters.alphaAwarePartition)
    {
        attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
        attributes[new osg::BlendFunc(GL_ONE, GL_ONE, GL_ONE,
                                      GL_ONE_MINUS_SRC_ALPHA)] = ON_OVERRIDE;
    }
    else
    {
        attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
        attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    }

    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);

    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    setupTexture("minMaxTexture", _parameters.reservedTextureUnits,
                 *_firstCount, _minMaxTexture.get());
    attributes[_viewport] = ON_OVERRIDE_PROTECTED;
    setupStateSet(_firstCount.get(), modes, attributes, uniforms);
}

void IterativeDepthPartitioner::_updateFirstCountPrograms(
    const ProgramMap& extraShaders)
{
    if (_parameters.alphaAwarePartition)
    {
        addPrograms(extraShaders, &_firstCountPrograms,
                    _filenames = strings("first_count_with_alpha.frag"),
                    _vertex_shaders = strings(sm("shadeVertex();")),
                    _shader_path = SHADER_PATH);
    }
    else
    {
        addPrograms(extraShaders, &_firstCountPrograms,
                    _filenames = strings("first_count.frag"),
                    _vertex_shaders = strings(sm("shadeVertex();")),
                    _shader_path = SHADER_PATH);
    }
}

void IterativeDepthPartitioner::_createCountIterationStateSet(
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
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    /* Textures */
    int nextIndex = _parameters.reservedTextureUnits;
    setupTexture("minMaxTexture", nextIndex, *_countIteration,
                 _minMaxTexture.get());
    const unsigned int numTextures = (points + 3) / 4;
    setupTextureArray("codedPreviousIntervalsTextures", ++nextIndex,
                      *_countIteration, numTextures, _codedIntervalsTextures);
    /* Uniforms */
    uniforms.insert(_iteration.get());
    uniforms.insert(_projection_33);
    uniforms.insert(_projection_34);
    /* Final setup */
    setupStateSet(_countIteration.get(), modes, attributes, uniforms);
}

void IterativeDepthPartitioner::_updateCountIterationPrograms(
    const ProgramMap& extraShaders, const size_t points)
{
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define POINTS %1%\n") % points);
    if (DOUBLE_WIDTH && points <= 4)
    {
        vars["DEFINES"] += "#define DOUBLE_WIDTH 1\n";
        vars["OUT_BUFFERS_DECLARATION"] =
            str(format("vec4 gl_FragData[%1%];\n") % (points * 2));
    }
    else
    {
        vars["OUT_BUFFERS_DECLARATION"] =
            str(format("vec4 gl_FragData[%1%];\n") % points);
    }
    vars["DEFINES"] +=
        str(format("#define FIRST_INTERVAL_SEARCH_WIDTH %.17f\n") %
            (_parameters.alphaAwarePartition ? 1 / 24.0 : 0.03125));
    const std::string code1 =
        "//find_interval.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "find_interval.frag", vars);
    const std::string code2 =
        "//count_iteration.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "count_iteration.frag",
                                      vars);
    addPrograms(extraShaders, &_countIterationPrograms,
                _vertex_shaders = strings(sm("trivialShadeVertex();")),
                _fragment_shaders = strings(code1, code2));
}

void IterativeDepthPartitioner::_createFirstFindQuantileIntervalsStateSet(
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
    /* Textures */
    setupTextureArray("countTextures", _parameters.reservedTextureUnits,
                      *_firstFindQuantileIntervals, 8, _countTextures);
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
    const size_t points = quantiles.size();
    vars["DEFINES"] = str(format("#define POINTS %1%\n"
                                 "#define OPACITY_THRESHOLD %2%\n"
                                 "#define ROUND %3%\n") %
                          points % _parameters.opacityThreshold %
                          (ROUND_UP_QUANTILES_COUNTS ? "round" : ""));
    vars["OUT_BUFFERS_DECLARATION"] =
        str(format("vec4 gl_FragData[%1%];\n") % (((points + 3) / 4) * 2 + 1));
    std::string code;
    if (_parameters.alphaAwarePartition)
    {
        code = "//alpha_adjusted_first_find_quantiles_interval.frag\n" +
               readSourceAndReplaceVariables(
                   SHADER_PATH +
                       "alpha_adjusted_first_find_quantiles_interval.frag",
                   vars);
    }
    else
    {
        code = "//first_find_quantiles_interval.frag\n" +
               readSourceAndReplaceVariables(
                   SHADER_PATH + "first_find_quantiles_interval.frag", vars);
    }
    /* Final setup */
    setupStateSet(_firstFindQuantileIntervals.get(), modes, attributes,
                  uniforms);
    addProgram(_firstFindQuantileIntervals.get(),
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}

void IterativeDepthPartitioner::_createFindQuantileIntervalsStateSet(
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
    size_t points = quantiles.size();
    int nextIndex = 0;
    setupTexture("totalCountsTexture", nextIndex, *_findQuantileIntervals,
                 _totalCountsTexture.get());
    unsigned int numCountTextures =
        points <= 4 && DOUBLE_WIDTH ? points * 2 : points;
    setupTextureArray("countTextures", ++nextIndex, *_findQuantileIntervals,
                      numCountTextures, _countTextures);
    nextIndex += numCountTextures;
    unsigned numLeftTextures = (points + 3) / 4;
    setupTextureArray("leftAccumTextures", nextIndex, *_findQuantileIntervals,
                      numLeftTextures, _leftAccumTextures);
    /* Uniforms */
    assert(_quantiles.get());
    uniforms.insert(_quantiles.get());
    uniforms.insert(_quantiles2.get());
    assert(_iteration.get());
    uniforms.insert(_iteration.get());
    /* Fragment shader code */
    std::map<std::string, std::string> vars;
    vars["DEFINES"] = str(format("#define POINTS %1%\n") % points);
    if (DOUBLE_WIDTH && points <= 4)
        vars["DEFINES"] += "#define DOUBLE_WIDTH\n";
    if (_parameters.alphaAwarePartition)
        vars["DEFINES"] += "#define ADJUST_QUANTILES_WITH_ALPHA\n";
    if (ROUND_UP_QUANTILES_COUNTS)
        vars["DEFINES"] += "#define ROUND round\n";
    else
        vars["DEFINES"] += "#define ROUND\n";
    vars["OUT_BUFFERS_DECLARATION"] =
        str(format("vec4 gl_FragData[%1%];\n") % (((points + 3) / 4) * 2));
    std::string code = "//find_quantiles_interval.frag\n" +
                       readSourceAndReplaceVariables(
                           SHADER_PATH + "find_quantiles_interval.frag", vars);
    /* Final setup */
    setupStateSet(_findQuantileIntervals.get(), modes, attributes, uniforms);
    addProgram(_findQuantileIntervals.get(),
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}

void IterativeDepthPartitioner::_createFinalReprojectionStateSet(
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
    const unsigned numCountTextures =
        points <= 4 && DOUBLE_WIDTH ? points * 2 : points;
    setupTextureArray("countTextures", nextIndex, *_finalProjection,
                      numCountTextures, _countTextures);
    nextIndex += numCountTextures;
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
        /* The peel pass is using projected z */
        vars["DEFINES"] += "#define PROJECT_Z\n";
    if (points <= 4 && DOUBLE_WIDTH)
        vars["DEFINES"] += "#define DOUBLE_WIDTH\n";
    if (_parameters.alphaAwarePartition)
        vars["DEFINES"] +=
            "#define ADJUST_QUANTILES_WITH_ALPHA\n"
            "#define FIRST_INTERVAL_SEARCH_WIDTH 0.041666666666666664\n";
    else
        vars["DEFINES"] += "#define FIRST_INTERVAL_SEARCH_WIDTH 0.03125\n";

    const std::string code =
        "//final_reprojection.frag\n" +
        readSourceAndReplaceVariables(SHADER_PATH + "final_reprojection.frag",
                                      vars);
    /* Final setup */
    setupStateSet(_finalProjection.get(), modes, attributes, uniforms);
    addProgram(_finalProjection.get(),
               _vertex_shaders = strings(BYPASS_VERT_SHADER),
               _fragment_shaders = strings(code));
}
}
}
}
