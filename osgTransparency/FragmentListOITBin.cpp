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

#include <osg/GL>

#ifdef OSG_GL3_AVAILABLE

#include "FragmentListOITBin.h"
//#include "TextureBuffer.h"

#include "util/GPUTimer.h"
#include "util/constants.h"
#include "util/extensions.h"
#include "util/glerrors.h"
#include "util/helpers.h"
#include "util/strings_array.h"

#include <osg/Version>

#include <osg/BlendFunc>
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
#  include <osg/BindImageTexture>
#endif
#include <osg/BufferIndexBinding>
#include <osg/BufferObject>
#include <osg/Depth>
#include <osg/FrameBufferObject>
#include <osg/GLDefines>
#include <osg/GLExtensions>
#include <osg/Geometry>
#include <osg/Stencil>
#include <osg/TextureBuffer>
#include <osg/TextureRectangle>
#include <osg/ValueObject>




#include <boost/lexical_cast.hpp>

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
namespace
{
const unsigned int MEAN_FRAGMENTS_PER_PIXEL =
    getenv("OSGTRANSPARENCY_FRAGMENTS_PER_PIXEL")
        ? (strtol(getenv("OSGTRANSPARENCY_FRAGMENTS_PER_PIXEL"), 0, 10) > 0
               ? strtol(getenv("OSGTRANSPARENCY_FRAGMENTS_PER_PIXEL"), 0, 10)
               : 24)
        : 24;
const unsigned int MAX_FRAGMENT_COUNT_INTERVALS = 8;
/* The first sorting interval has a minimum fragment count of 8 */
const unsigned int MAX_FRAGMENTS_PER_LIST = 4 << MAX_FRAGMENT_COUNT_INTERVALS;

const bool GPU_TIMING = getenv("OSGTRANSPARENCY_GPU_TIMING") != 0;

/*
  TextureBuffer allocation callback
*/
class SubloadCallback : public TextureBuffer::SubloadCallback
{
public:
    virtual void load(const TextureBuffer& t, osg::State& state) const
    {
        const osg::GLExtensions* extensions =
            osg::GLExtensions::Get(state.getContextID(), true);

        checkGLErrors("Before bufferdata");

        size_t size = t.getTextureWidth();
        extensions->glBufferData(GL_TEXTURE_BUFFER, size * sizeof(GLuint), 0,
                                 GL_DYNAMIC_DRAW);

        checkGLErrors("After bufferdata");
    }

    virtual void subload(const TextureBuffer&, osg::State&) const {}
};
}

typedef boost::shared_ptr<FragmentListOITBin::Parameters> ParametersPtr;

/*
  FragmentListOITBin::_Impl
*/

class FragmentListOITBin::_Impl
{
public:
    class Context;
    class StateObserver : public osg::Observer
    {
    public:
        void objectDeleted(void* object)
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_contextMapMutex);
            s_contextMap.erase(object);
        }
    };
    static Context& getContext(osg::State* state,
                               const ParametersPtr& parameters);

private:
    static OpenThreads::Mutex s_contextMapMutex;
    typedef boost::shared_ptr<Context> ContextPtr;
    typedef std::map<const void*, ContextPtr> ContextMap;
    static ContextMap s_contextMap;
    static StateObserver s_stateObserver;
};

OpenThreads::Mutex FragmentListOITBin::_Impl::s_contextMapMutex;
FragmentListOITBin::_Impl::ContextMap FragmentListOITBin::_Impl::s_contextMap;
FragmentListOITBin::_Impl::StateObserver
    FragmentListOITBin::_Impl::s_stateObserver;

/*
  FragmentListOITBin::Parameters
*/

struct FragmentListOITBin::Parameters::_Impl
{
    _Impl()
        : alphaCutOffThreshold(0)
    {
    }

    OpenThreads::Mutex mutex;
    std::map<unsigned int, CaptureCallback> captureCallbacks;
    float alphaCutOffThreshold;
};

FragmentListOITBin::Parameters::Parameters()
    : _impl(new _Impl)
{
}

FragmentListOITBin::Parameters::Parameters(const Parameters& other)
    : BaseRenderBin::Parameters(other)
    , _impl(new _Impl)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(other._impl->mutex);
    _impl->captureCallbacks = other._impl->captureCallbacks;
    _impl->alphaCutOffThreshold = other._impl->alphaCutOffThreshold;
}

FragmentListOITBin::Parameters::~Parameters()
{
    delete _impl;
}

void FragmentListOITBin::Parameters::setCaptureCallback(
    const unsigned int contextID, const CaptureCallback& callback)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_impl->mutex);
    _impl->captureCallbacks[contextID] = callback;
}

void FragmentListOITBin::Parameters::enableAlphaCutOff(const float threshold)
{
    if (threshold <= 0 || threshold > 1)
        throw std::runtime_error("Invalid alpha cut-off threshold value");
    _impl->alphaCutOffThreshold = threshold;
}

void FragmentListOITBin::Parameters::disableAlphaCutOff()
{
    _impl->alphaCutOffThreshold = 0;
}

bool FragmentListOITBin::Parameters::isAlphaCutOffEnabled() const
{
    return _impl->alphaCutOffThreshold != 0;
}

bool FragmentListOITBin::Parameters::update(const Parameters& other)
{
    if (!BaseRenderBin::Parameters::update(other))
        return false;

    if (_impl->alphaCutOffThreshold == 0 &&
        other._impl->alphaCutOffThreshold != 0)
        return false;

    /* There's no need to lock the mutex on this object because this function
       is only used on the internal copy of the parameters which is not
       accesible to the user. We lock the mutex on the parameter only. */
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(other._impl->mutex);
    _impl->captureCallbacks = other._impl->captureCallbacks;
    _impl->alphaCutOffThreshold = other._impl->alphaCutOffThreshold;

    return true;
}

/*
  FragmentListOITBin::_Impl::Context
*/

class FragmentListOITBin::_Impl::Context
{
public:
    friend class FragmentData;

    /*--- Public constructor ---*/

    Context(osg::State* state, const ParametersPtr& parameters)
        : _parameters(*parameters)
        , _camera(0)
        , _atomicBuffer(0)
        , _savedStackPosition(0)
        , _oldPrevious(0)
        , _gpuTimer(state)
    {
    }

    /*--- Public member functions ---*/

    void draw(FragmentListOITBin* bin, osg::RenderInfo& renderInfo,
              osgUtil::RenderLeaf*& previous)
    {
        _testAndInit(bin, renderInfo);

        _preDraw(renderInfo, previous);
        _captureFragments(bin, renderInfo, previous);

        const unsigned int contextID = renderInfo.getState()->getContextID();
        const CaptureCallback& callback =
            /* The _parameters object is completely internal, there's no
               need to lock it to access the callback map. */
            _parameters._impl->captureCallbacks[contextID];

        if (!callback.empty() &&
            !callback(FragmentData(renderInfo.getState(), this)))
        {
            /* Return to previous framebuffer */
            osg::GLExtensions* ext = osg::GLExtensions::Get(contextID, true);
            ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _previousFBO);
            return;
        }

        _sortAndDisplay(renderInfo);
        _postDraw(renderInfo, previous);
    }

    bool updateParameters(const ParametersPtr& parameters)
    {
        return _parameters.update(*parameters);
    }

private:
    /*--- Private member varibles ---*/

    Parameters _parameters;

    osg::Camera* _camera;
    unsigned int _maxWidth;
    unsigned int _maxHeight;

    osg::ref_ptr<osg::Geometry> _quad;

    osg::ref_ptr<osg::FrameBufferObject> _auxiliaryBuffer;

    osg::ref_ptr<osg::TextureRectangle> _fragmentLists;
    osg::ref_ptr<osg::TextureRectangle> _fragmentCounts;
    osg::ref_ptr<osg::TextureBuffer> _fragments;

    /* If alpha cut off is requested we need an extra image buffer that encodes
       the current transparency value and the largest depth found so far. The
       precision of the storage will be 24 bits for depth and 8 bit for
       transparency. That implies that only changes of transparency greater
       than 1/255.0 will be noticeable, but this should be OK.
       The transparency is encoded in the most significant byte and depth is
       encoded in the 3 remaining ones. This encoding allows the shader to use
       atomicMin to minimize the impact of race conditions while updating the
       same pixel from different fragments. */
    osg::ref_ptr<osg::TextureRectangle> _depthTranspBuffer;

    osg::ref_ptr<osg::Uniform> _minTransparency;
    osg::ref_ptr<osg::Uniform> _fragmentCountRange;
    osg::ref_ptr<osg::Uniform> _lowerLeftCorner;
    osg::ref_ptr<osg::Viewport> _viewport;

    ProgramMap _extraShaders;

    ProgramMap _saveFragmentsPrograms;

    osg::ref_ptr<osg::StateSet> _saveFragmentsStateSet;
    osg::ref_ptr<osg::StateSet> _fragmentCountFilterStateSet;
    osg::ref_ptr<osg::StateSet>
        _sortAndDisplayFragmentsStateSet[MAX_FRAGMENT_COUNT_INTERVALS];

    osg::ref_ptr<osg::BufferObject> _atomicBuffer;
    GLint _previousFBO;

    unsigned int _savedStackPosition;
    osgUtil::RenderLeaf* _oldPrevious;
#ifndef NDEBUG
    osg::ref_ptr<osg::StateSet> _oldState;
#endif

    GPUTimer _gpuTimer;

    /*--- Private member functions ---*/

    void _preDraw(osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous)
    {
        osg::State& state = *renderInfo.getState();
        const unsigned int frame = state.getFrameStamp()->getFrameNumber();
        if (GPU_TIMING)
            _gpuTimer.start("pre_draw", frame);

        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &_previousFBO);
        _oldPrevious = previous;
        _savedStackPosition = state.getStateSetStackSize();
#ifndef NDEBUG
        _oldState = new osg::StateSet;
        state.captureCurrentState(*_oldState);
#endif
        /* Resetting atomic counter.
           Maybe calling dirty on the underlying UIntArray is enough but
           this method is used instead because it seems less expensive. */
        osg::GLBufferObject* buffer =
            _atomicBuffer->getGLBufferObject(state.getContextID());
        if (buffer)
        {
            /* The atomic buffer has already been allocated so probably
               it has also been used and the initial buffer data
               changed. */
            buffer->bindBuffer();
            int flags = (GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                         GL_MAP_UNSYNCHRONIZED_BIT);
            GLuint* ptr = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0,
                                                    sizeof(GLuint), flags);
            *ptr = 0;
            glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
            buffer->unbindBuffer();
        }

        /* The scissor setup is needed for OSG cameras whose viewport is not
           at (0, 0) */
        glScissor(0, 0, _viewport->width(), _viewport->height());

        /* Clearing the fragment list head texture */
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[0],
                                        osg::FrameBufferAttachment(
                                            _fragmentLists.get()));
        _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[1],
                                        osg::FrameBufferAttachment(
                                            _fragmentCounts.get()));
        if (_parameters._impl->alphaCutOffThreshold)
        {
            _auxiliaryBuffer->setAttachment(COLOR_BUFFERS[2],
                                            osg::FrameBufferAttachment(
                                                _depthTranspBuffer.get()));
        }
        _auxiliaryBuffer->apply(state);
        /* Clear head pointer buffer to undefined. */
        GLuint colorui[] = {0xFFFFFFFF, 0, 0, 0};
        glClearBufferuiv(GL_COLOR, 0, colorui);
        /* Clear fragment count to 0. */
        colorui[0] = 0;
        glClearBufferuiv(GL_COLOR, 1, colorui);

        if (_parameters.isAlphaCutOffEnabled())
        {
            /* Clearing the depth to the minimum encoded value and the
               transparency to the maximum. */
            colorui[0] = 0xFF000000;
            glClearBufferuiv(GL_COLOR, 2, colorui);
        }
        if (GPU_TIMING)
            _gpuTimer.stop();
        checkGLErrors("After pre-draw");
    }

    void _captureFragments(FragmentListOITBin* bin, osg::RenderInfo& renderInfo,
                           osgUtil::RenderLeaf*& previous)
    {
        osg::State& state = *renderInfo.getState();
        const unsigned int frame = state.getFrameStamp()->getFrameNumber();

        if (GPU_TIMING)
            _gpuTimer.start("render", frame);

        glDrawBuffer(GL_NONE);

        bin->render(renderInfo, previous, _saveFragmentsStateSet.get(),
                    _saveFragmentsPrograms);

        if (GPU_TIMING)
            _gpuTimer.stop();
        checkGLErrors("After render");
    }

    void _sortAndDisplay(osg::RenderInfo& renderInfo)
    {
        osg::State& state = *renderInfo.getState();
        const unsigned int contextID = state.getContextID();
        osg::GLExtensions* ext = osg::GLExtensions::Get(contextID, true);
        const unsigned int frame = state.getFrameStamp()->getFrameNumber();
        if (GPU_TIMING)
            _gpuTimer.start("sort_and_display", frame);

        /* Display the sorted and composited per pixel list in batches
           depending on their fragment counts.
           The destination buffer must have a stencil buffer. */
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _previousFBO);
        glScissor(_camera->getViewport()->x(), _camera->getViewport()->y(),
                  _viewport->width(), _viewport->height());

        /* The following code only draws quads, so the projection and
           modelview matrices are reset. */
        state.applyProjectionMatrix(0);
        state.applyModelViewMatrix(0);
        size_t index = 0;
        unsigned int rangeStart = 1;
        for (unsigned int i = 8; i < MAX_FRAGMENTS_PER_LIST;
             rangeStart = i + 1, i <<= 1, ++index)
        {
            if (index == 0)
                /* This is needed for the first list */
                ext->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

            /* Selecting which pixels will be rendered in this batch*/
            _fragmentCountRange->set(rangeStart, i);
            state.apply(_fragmentCountFilterStateSet);
            glClearStencil(0x00);
            glClear(GL_STENCIL_BUFFER_BIT);
            _quad->draw(renderInfo);
            checkGLErrors("After stencil pass");

            state.apply(_sortAndDisplayFragmentsStateSet[index].get());
            _quad->draw(renderInfo);

            checkGLErrors("After sort & composite batch");
        }
        if (GPU_TIMING)
            _gpuTimer.stop();
    }

    void _postDraw(osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous)
    {
        osg::State& state = *renderInfo.getState();

        /* Restoring state */
        state.popStateSetStackToSize(_savedStackPosition);
        previous = _oldPrevious;
        state.apply();
#ifndef NDEBUG
        osg::ref_ptr<osg::StateSet> currentState(new osg::StateSet);
        state.captureCurrentState(*currentState);
        assert(*currentState == *_oldState);
#endif
        checkGLErrors("After post-draw");
        if (GPU_TIMING)
        {
            _gpuTimer.checkQueries();
            _gpuTimer.reportCompleted(std::cout);
        }
    }

    bool _valid(osg::Camera* camera)
    {
        unsigned int width = (unsigned int)camera->getViewport()->width();
        unsigned int height = (unsigned int)camera->getViewport()->height();
        return _camera == camera && _maxWidth >= width && _maxHeight >= height;
    }

    void _createBuffersAndTextures()
    {
        _auxiliaryBuffer = new osg::FrameBufferObject();

        _fragmentLists =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_R32UI, GL_RED_INTEGER);

        _fragmentCounts =
            createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                 GL_R32UI, GL_RED_INTEGER);

        osg::BufferData* data = new osg::BufferData();
        data->
        _fragments = new osg::TextureBuffer(bufferData);
        _fragments->setTextureWidth(_maxWidth * _maxHeight * 3 *
                                    MEAN_FRAGMENTS_PER_PIXEL);
        _fragments->setInternalFormat(GL_R32UI);
        _fragments->setSubloadCallback(new SubloadCallback());

        if (_parameters.isAlphaCutOffEnabled())
        {
            _depthTranspBuffer =
                createTexture<osg::TextureRectangle>(_maxWidth, _maxHeight,
                                                     GL_R32UI, GL_RED_INTEGER);
        }
    }

    void _createStateSets()
    {
        _viewport = new osg::Viewport(0, 0, 0, 0);
        _lowerLeftCorner = new osg::Uniform("corner", osg::Vec2(0, 0));
        _createFragmentCollectionStateSet();
        _createFragmentCountFilteringState();
        _createSortAndDisplayStateSets();
    }

    void _createFragmentCollectionStateSet()
    {
        using namespace keywords;
        Modes modes;
        Attributes attributes;
        Uniforms uniforms;
        std::map<std::string, std::string> vars;

        _saveFragmentsStateSet = new osg::StateSet();
        modes[GL_CULL_FACE] = OFF_OVERRIDE;
        modes[GL_DEPTH] = OFF;
        attributes[_viewport] = ON_OVERRIDE;
        osg::UIntArray* atomic = new osg::UIntArray();
        atomic->push_back(0);
        _atomicBuffer = new osg::AtomicCounterBufferObject();
        _atomicBuffer->addBufferData(atomic);
        attributes[new osg::AtomicCounterBufferBinding(0, _atomicBuffer, 0,
                                                       sizeof(GLuint))] = ON;

        /* Uniforms for samplers must be int. */
        int texUnit = _parameters.reservedTextureUnits;
        int imgUnit = 0;
        /* Blending operations don't seem to work in integer framebuffer
           attachments, so an image buffer is used instead. */
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
        attributes[new osg::BindImageTexture(imgUnit, _fragmentCounts,
                                             osg::BindImageTexture::READ_WRITE,
                                             GL_R32UI)];
#else
        _fragmentCounts->bindToImageUnit(imgUnit, osg::Texture::READ_WRITE);
#endif
        uniforms.insert(new osg::Uniform("fragmentCounts", imgUnit));
        _saveFragmentsStateSet->setTextureAttribute(texUnit, _fragmentCounts);
        ++texUnit;
        ++imgUnit;


#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
        attributes[new osg::BindImageTexture(
            imgUnit, _fragments, osg::BindImageTexture::READ_WRITE, GL_R32UI)];
#else
        _fragments->bindToImageUnit(imgUnit, osg::Texture::WRITE_ONLY);
#endif
        uniforms.insert(new osg::Uniform("fragmentBuffer", imgUnit));
        _saveFragmentsStateSet->setTextureAttribute(texUnit, _fragments);
        ++texUnit;
        ++imgUnit;

#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
        attributes[new osg::BindImageTexture(imgUnit, _fragmentLists,
                                             osg::BindImageTexture::READ_WRITE,
                                             GL_R32UI)];
#else
        _fragmentLists->bindToImageUnit(imgUnit, osg::Texture::READ_WRITE);
#endif
        uniforms.insert(new osg::Uniform("listHead", imgUnit));
        _saveFragmentsStateSet->setTextureAttribute(texUnit, _fragmentLists);
        ++texUnit;
        ++imgUnit;

        if (_parameters.isAlphaCutOffEnabled())
        {
            _minTransparency = new osg::Uniform("minTransparency", 0.f);
            uniforms.insert(_minTransparency);

#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 10)
        attributes[new osg::BindImageTexture(imgUnit, _depthTranspBuffer,
                                             osg::BindImageTexture::READ_WRITE,
                                             GL_R32UI)];
#else
            _depthTranspBuffer->bindToImageUnit(imgUnit,
                                                osg::Texture::READ_WRITE);
#endif
            uniforms.insert(new osg::Uniform("depthTranspBuffer", imgUnit));
            _saveFragmentsStateSet->setTextureAttribute(texUnit,
                                                        _depthTranspBuffer);
            ++texUnit;
            ++imgUnit;
        }

        setupStateSet(_saveFragmentsStateSet, modes, attributes, uniforms);

        /* The shaders are setup later inside _updatePrograms. */
    }

    void _createFragmentCountFilteringState()
    {
        using namespace keywords;
        Modes modes;
        Attributes attributes;
        Uniforms uniforms;
        std::map<std::string, std::string> vars;

        osg::StateSet* stateSet = new osg::StateSet();
        _fragmentCountFilterStateSet = stateSet;
        modes[GL_CULL_FACE] = OFF_OVERRIDE;
        modes[GL_DEPTH] = OFF;
        attributes[new osg::ColorMask(false, false, false, false)] = ON;
        attributes[_camera->getViewport()] = ON_OVERRIDE;
        _fragmentCountRange = new osg::Uniform("fragmentCountRange", 0u, 0u);
        uniforms.insert(_fragmentCountRange);
        uniforms.insert(_lowerLeftCorner);
        osg::Stencil* stencil = new osg::Stencil;
        stencil->setFunction(osg::Stencil::ALWAYS, 1, 0);
        stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP,
                              osg::Stencil::REPLACE);
        attributes[stencil] = ON;
        setupStateSet(stateSet, modes, attributes, uniforms);
        std::string code =
            "//fragment_count_filtering.frag\n" +
            readSourceAndReplaceVariables(
                "fragment_list/fragment_count_filtering.frag", vars);

        addProgram(stateSet, _vertex_shaders = strings(BYPASS_VERT_SHADER),
                   _fragment_shaders = strings(code));
        setupTexture("fragmentCounts", 0, *stateSet, _fragmentCounts);
    }

    void _createSortAndDisplayStateSets()
    {
        using namespace keywords;
        Modes modes;
        Attributes attributes;
        Uniforms uniforms;
        std::map<std::string, std::string> vars;

        modes[GL_CULL_FACE] = OFF_OVERRIDE;
        modes[GL_BLEND] = ON;
        /* It seems that if we use modes[GL_DEPTH] = OFF as usual, the stencil
           test fails in all cases with a depth pass fail. On the other hand,
           setting a explicit depth function works. */
        attributes[new osg::Depth(osg::Depth::ALWAYS, 0, 0, false)];
        attributes[new osg::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)] = ON;
        attributes[new osg::BlendEquation(FUNC_ADD)] = ON;
        attributes[_camera->getViewport()] = ON_OVERRIDE;
        osg::Stencil* stencil = new osg::Stencil;
        stencil->setFunction(osg::Stencil::NOTEQUAL, 0, 0xFF);
        stencil->setWriteMask(0);
        attributes[stencil] = ON;
        uniforms.insert(_lowerLeftCorner);

        size_t index = 0;
        for (unsigned int i = 8; i < MAX_FRAGMENTS_PER_LIST; i <<= 1, ++index)
        {
            osg::StateSet* stateSet = new osg::StateSet();
            _sortAndDisplayFragmentsStateSet[index] = stateSet;

            setupStateSet(stateSet, modes, attributes, uniforms);
            vars["MAX_FRAGMENTS_PER_LIST"] =
                boost::lexical_cast<std::string>(i);
            const std::string code =
                "//sort_and_display.frag\n" +
                readSourceAndReplaceVariables(
                    "fragment_list/sort_and_display.frag", vars);
            addProgram(stateSet, _vertex_shaders = strings(BYPASS_VERT_SHADER),
                       _fragment_shaders = strings(code));

            setupTexture("listHead", 0, *stateSet, _fragmentLists);
            setupTexture("fragmentBuffer", 1, *stateSet, _fragments);
        }
    }

    void _updatePrograms(const ProgramMap& extraShaders)
    {
        using namespace keywords;

        std::string code;
        std::map<std::string, std::string> vars;
        vars.clear();

        if (_parameters.isAlphaCutOffEnabled())
            vars["DEFINES"] = "#define USE_ALPHA_CUTOFF\n";

        code =
            "//save_fragments.frag\n" +
            readSourceAndReplaceVariables("fragment_list/save_fragments.frag",
                                          vars);
        addPrograms(extraShaders, &_saveFragmentsPrograms,
                    _vertex_shaders = strings(sm("shadeVertex();")),
                    _fragment_shaders = strings(code));
    }

    void _testAndInit(FragmentListOITBin* bin, osg::RenderInfo& renderInfo)
    {
        osg::Camera* camera = renderInfo.getCurrentCamera();
        osg::Viewport* viewport = camera->getViewport();
        if (!_valid(camera))
        {
            _camera = camera;
            _maxWidth = (unsigned int)viewport->width();
            _maxHeight = (unsigned int)viewport->height();
            osg::Vec2d maxViewport;
            if (_camera->getUserValue("max_viewport_hint", maxViewport))
            {
                _maxWidth = std::max(_maxWidth, (unsigned int)maxViewport.x());
                _maxHeight =
                    std::max(_maxHeight, (unsigned int)maxViewport.y());
            }

            _createBuffersAndTextures();
            _createStateSets();
            _quad = createQuad();
        }
        _viewport->width() = viewport->width();
        _viewport->height() = viewport->height();

        ProgramMap newShaders;
        updateProgramMap(*bin->_extraShaders, _extraShaders, newShaders);

        if (!newShaders.empty())
            _updatePrograms(newShaders);

        _lowerLeftCorner->set(osg::Vec2(viewport->x(), viewport->y()));
        if (_parameters.isAlphaCutOffEnabled())
            _minTransparency->set(
                std::max(0.f, 1 - _parameters._impl->alphaCutOffThreshold));
    }
};

/*
  FragmentListOITBin::_Impl implementation
*/

FragmentListOITBin::_Impl::Context& FragmentListOITBin::_Impl::getContext(
    osg::State* state, const ParametersPtr& parameters)
{
    /* Multiple draw threads might be trying to create their own
       alpha-blending context */
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_contextMapMutex);
    ContextPtr& context = s_contextMap[state];

    if (!context)
        state->addObserver(&s_stateObserver);

    if (!context || !context->updateParameters(parameters))
        context.reset(new Context(state, parameters));

    return *context;
}

/*
  FragmentData
*/
FragmentData::FragmentData(osg::State* state, void* data)
    : _state(state)
    , _data(data)
{
}

osg::State& FragmentData::getState() const
{
    return *_state;
}

osg::TextureRectangle* FragmentData::getCounts() const
{
    return static_cast<FragmentListOITBin::_Impl::Context*>(_data)
        ->_fragmentCounts;
}

osg::TextureRectangle* FragmentData::getHeads() const
{
    return static_cast<FragmentListOITBin::_Impl::Context*>(_data)
        ->_fragmentLists;
}

TextureBuffer* FragmentData::getFragments() const
{
    return static_cast<FragmentListOITBin::_Impl::Context*>(_data)->_fragments;
}

size_t FragmentData::getNumFragments() const
{
    unsigned int contextID = _state->getContextID();
    osg::ref_ptr<osg::BufferObject>& buffer =
        static_cast<FragmentListOITBin::_Impl::Context*>(_data)->_atomicBuffer;
    osg::GLBufferObject* glBuffer = buffer->getGLBufferObject(contextID);

    osg::GLExtensions* ext = osg::GLExtensions::Get(contextID, true);
    ext->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    glBuffer->bindBuffer();
    GLuint size = 0;
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &size);
    glBuffer->unbindBuffer();

    return size;
}

/*
  Constructors
*/

FragmentListOITBin::FragmentListOITBin(const Parameters& parameters)
    : BaseRenderBin(boost::shared_ptr<Parameters>(new Parameters(parameters)))
{
}

FragmentListOITBin::FragmentListOITBin(const FragmentListOITBin& renderBin,
                                       const osg::CopyOp& copyop)
    : BaseRenderBin(renderBin, copyop)
{
}

/*
  Member functions
*/

void FragmentListOITBin::drawImplementation(osg::RenderInfo& renderInfo,
                                            osgUtil::RenderLeaf*& previous)
{
    osg::State* state = renderInfo.getState();
    _Impl::Context& context =
        _Impl::getContext(state,
                          boost::static_pointer_cast<Parameters>(_parameters));
    context.draw(this, renderInfo, previous);
}

/*
  Free fucntions
*/
bool extractFragmentStatistics(const FragmentData& fragmentData)
{
    osg::State& state = fragmentData.getState();
    osg::TextureRectangle& counts = *fragmentData.getCounts();

    unsigned int contextID = state.getContextID();
    osg::GLExtensions* ext = osg::GLExtensions::Get(contextID, true);
    ext->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    counts.apply(state);
    size_t numFragments = fragmentData.getNumFragments();

    GLint width;
    GLint height;
    glPixelStorei(GL_PACK_ALIGNMENT, sizeof(int));
    glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE, 0, GL_TEXTURE_HEIGHT,
                             &height);
    unsigned int* data = new unsigned int[width * height];

    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
                  data);

    std::map<unsigned int, unsigned int> histogram;
    for (int i = 0; i != height; ++i)
    {
        for (int j = 0; j != width; ++j)
            ++histogram[data[i * width + j]];
    }

    std::cout << state.getFrameStamp()->getFrameNumber() << " fragment_counts ";
    for (const auto& i : histogram)
        std::cout << i.first << ' ' << i.second << ' ';
    std::cout << "total " << numFragments << std::endl;

    delete[] data;
    return true;
}

void writeTextures(const std::string& filename,
                   const FragmentData& fragmentData)
{
    osg::State& state = fragmentData.getState();
    osg::TextureRectangle& counts = *fragmentData.getCounts();
    osg::TextureRectangle& heads = *fragmentData.getHeads();
    TextureBuffer& fragments = *fragmentData.getFragments();

    std::ofstream out(filename, std::ios::binary);
    if (!out)
        throw std::runtime_error("Could not open file for writing: " +
                                 filename);

    const unsigned int contextID = state.getContextID();
    osg::GLExtensions* ext = osg::GLExtensions::Get(contextID, true);
    ext->glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    counts.apply(state);

    GLint width;
    GLint height;
    glPixelStorei(GL_PACK_ALIGNMENT, sizeof(int));
    glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE, 0, GL_TEXTURE_HEIGHT,
                             &height);
    out.write((char*)&width, sizeof(GLint));
    out.write((char*)&height, sizeof(GLint));

    uint32_t* data = new uint32_t[width * height];
    size_t size = sizeof(uint32_t) * width * height;
    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
                  data);
    uint32_t numFrags = 0;
    for (int i = 0; i != width * height; ++i)
        numFrags += data[i];
    out.write((char*)&numFrags, sizeof(uint32_t));

    out.write((char*)data, size);

    heads.apply(state);
    glPixelStorei(GL_PACK_ALIGNMENT, sizeof(int));
    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
                  data);
    out.write((char*)data, size);
    delete[] data;

    data = new uint32_t[numFrags * 3];
    osg::GLBufferObject* buffer = fragments.getGLBufferObject(contextID);
    buffer->bindBuffer();
    size = numFrags * 3 * sizeof(uint32_t);
    glGetBufferSubData(GL_TEXTURE_BUFFER, 0, size, data);
    buffer->unbindBuffer();
    out.write((char*)data, size);
    delete[] data;
}
}
}

#endif
