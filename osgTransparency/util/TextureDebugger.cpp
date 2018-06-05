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

#include "TextureDebugger.h"

#include "constants.h"
#include "extensions.h"
#include "helpers.h"

#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/GraphicsContext>
#include <osg/Image>
#include <osg/Texture2DArray>
#include <osg/TextureRectangle>

#include <osgViewer/GraphicsWindow>

// Disabled by default since it's problematic in some cases (Equalizer created
// buffers, optirun...)
// #define SHARE_CONTEXTS

#include <cassert>
#include <iostream>

namespace
{
std::string defaultCode()
{
#ifdef OSG_GL3_AVAILABLE
    return R"(#version 420
              uniform sampler2DRect input;
              out vec4 outColor;
              void main()
              {
                   outColor = texture(input, gl_FragCoord.xy);
              })";
#else
    return R"(#extension GL_ARB_texture_rectangle : enable
              uniform sampler2DRect texture;
              void main()
              {
                  vec4 sample = texture2DRect(texture, gl_FragCoord.xy);
                  gl_FragColor = sample;
              })";
#endif
}
}

/*
  TextureDebugger::Impl
*/
struct TextureDebugger::Impl
{
    osgViewer::GraphicsWindow* getOrCreateWindow(osg::GraphicsContext* context)
    {
        osg::ref_ptr<osgViewer::GraphicsWindow>& window = _windows[context];
        if (!window.valid())
        {
            /* Creating the output window */
            osg::GraphicsContext::Traits* traits =
                new osg::GraphicsContext::Traits();
            if (_name == "")
                traits->windowName = "texture debugger";
            traits->windowDecoration = true;
#ifdef SHARE_CONTEXTS
            traits->sharedContext = context;
#endif
            /* Tentative size */
            traits->width = 50;
            traits->height = 50;

            static_cast<osg::GraphicsContext::ScreenIdentifier&>(*traits) =
                static_cast<const osg::GraphicsContext::ScreenIdentifier&>(
                    *context->getTraits());

            osg::ref_ptr<osg::GraphicsContext> tmp =
                osg::GraphicsContext::createGraphicsContext(traits);
            window = dynamic_cast<osgViewer::GraphicsWindow*>(tmp.get());
#ifdef SHARE_CONTEXTS
            std::cout << "Created window " << window << " sharing with context "
                      << context << std::endl;
            assert(window->getState()->getContextID() ==
                   context->getState()->getContextID());
#endif
            window->realize();
            static int s_windows = 0;
            _indices[window] = ++s_windows;
        }
        return window.get();
    }

    void setWindowName(const std::string& name)
    {
        _name = name;
        for (Windows::iterator w = _windows.begin(); w != _windows.end(); ++w)
            w->second->setWindowName(name);
    }

    void renderTexture(osg::GraphicsContext* context,
                       osg::TextureRectangle* textureIn,
                       const std::string& code)
    {
        using namespace bbp::osgTransparency;

#ifdef SHARE_CONTEXTS
        osg::Texture* texture = textureIn;
#else
        GLint previousFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previousFBO);
        osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();
        fbo->setAttachment(COLOR_BUFFERS[0],
                           osg::FrameBufferAttachment(textureIn));
        osg::State& contextState = *context->getState();
        fbo->apply(contextState);

        osg::ref_ptr<osg::Image> image(new osg::Image());
        image->readPixels(0, 0, textureIn->getTextureWidth(),
                          textureIn->getTextureHeight(), GL_RGBA, GL_FLOAT);
        osg::ref_ptr<osg::TextureRectangle> texture =
            new osg::TextureRectangle(*textureIn);
        texture->setImage(image);

        FBOExtensions* ext = getFBOExtensions(contextState.getContextID());
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, previousFBO);

#endif
        _renderTexture(context, texture, code);
    }

    void renderTexture(osg::GraphicsContext* context,
                       osg::Texture2DArray* textureIn, const unsigned int layer,
                       const std::string& code)
    {
        using namespace bbp::osgTransparency;

        GLint previousFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previousFBO);
        osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject();
        fbo->setAttachment(COLOR_BUFFERS[0],
                           osg::FrameBufferAttachment(textureIn, layer));
        osg::State& contextState = *context->getState();
        fbo->apply(contextState);

        osg::ref_ptr<osg::Image> image(new osg::Image());
        const int width = textureIn->getTextureWidth();
        const int height = textureIn->getTextureHeight();
        image->readPixels(0, 0, width, height, GL_RGBA, GL_FLOAT);
        osg::ref_ptr<osg::TextureRectangle> texture =
            new osg::TextureRectangle();
        texture->setImage(image);
        texture->setTextureSize(width, height);
        texture->setInternalFormat(GL_RGBA);

        FBOExtensions* ext = getFBOExtensions(contextState.getContextID());
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, previousFBO);

        _renderTexture(context, texture, code);
    }

private:
    typedef std::map<osg::GraphicsContext*,
                     osg::ref_ptr<osgViewer::GraphicsWindow>>
        Windows;
    typedef std::map<osg::GraphicsContext*, int> WindowIndices;

    osg::ref_ptr<osg::Geometry> _quad;
    std::string _name;
    Windows _windows;
    WindowIndices _indices;

    void _renderTexture(osg::GraphicsContext* context,
                        osg::TextureRectangle* texture, const std::string& code)
    {
        using namespace bbp::osgTransparency;
        using namespace bbp::osgTransparency::keywords;

        context->releaseContext();

        osgViewer::GraphicsWindow* window = getOrCreateWindow(context);
        _setup(window, texture);
        window->makeCurrent();

        osg::State& state = *window->getState();
        const unsigned int width = texture->getTextureWidth();
        const unsigned int height = texture->getTextureHeight();
        osg::ref_ptr<osg::Viewport> vp(new osg::Viewport(0, 0, width, height));
        vp->apply(state);

        osg::ref_ptr<osg::StateSet> stateSet(new osg::StateSet);
        setupTexture("texture", 0, *stateSet, texture);
        addProgram(stateSet.get(),
                   _vertex_shaders =
                       std::vector<std::string>(1, BYPASS_VERT_SHADER),
                   _fragment_shaders = std::vector<std::string>(1, code));
        state.pushStateSet(stateSet.get());
        state.apply();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        osg::RenderInfo renderInfo(&state, 0);
        if (!_quad)
            _quad = createQuad();
        _quad->draw(renderInfo);

        state.popAllStateSets();
        state.apply();

        window->swapBuffers();
        window->releaseContext();
        context->makeCurrent();
    }

    void _setup(osgViewer::GraphicsWindow* window,
                osg::TextureRectangle* texture)
    {
        int width, height, dummy;
        const int index = _indices[window];
        window->getWindowRectangle(dummy, dummy, width, height);
        const int texWidth = texture->getTextureWidth();
        const int texHeight = texture->getTextureHeight();
        if (width != texWidth || height != texHeight)
        {
            int x, y;
            if (index < 6)
            {
                x = (texWidth + 2) * (index % 3);
                y = (texHeight + 5) * (index / 3);
            }
            else
                x = y = 0;
            window->setWindowRectangle(x, y, texWidth, texHeight);
        }
    }
};

/*
  TextureDebugger
*/
TextureDebugger::TextureDebugger()
    : _impl(new Impl())
{
}

TextureDebugger::~TextureDebugger()
{
    delete _impl;
}

void TextureDebugger::setWindowName(const std::string& name)
{
    _impl->setWindowName(name);
}

void TextureDebugger::renderTexture(osg::GraphicsContext* context,
                                    osg::TextureRectangle* texture,
                                    const std::string& code)
{
    if (code.empty())
        _impl->renderTexture(context, texture, defaultCode());
    else
        _impl->renderTexture(context, texture, code);
}

void TextureDebugger::renderTexture(osg::GraphicsContext* context,
                                    osg::Texture2DArray* texture,
                                    const unsigned int layer,
                                    const std::string& code)
{
    if (code.empty())
        _impl->renderTexture(context, texture, layer, defaultCode());
    else
        _impl->renderTexture(context, texture, layer, code);
}

std::string TextureDebugger::GLSL(const std::string& function, bool integer,
                                  const std::string& preamble)
{
#ifdef OSG_GL3_AVAILABLE
    std::string code = "#version 420\n";
    code += preamble;
    const std::string prefix = integer ? "i" : "";
    code += "uniform " + prefix +
            "sampler2DRect input;\n"
            "out " +
            prefix + "vec4 outColor;\n";
    code += function;
    code +=
        ("void main() {\n"
         "   outColor = transform(texture(input, gl_FragCoord.xy));\n"
         "}\n");
    return code;
#else
    std::string code = "#extension GL_ARB_texture_rectangle : enable\n";
    const std::string prefix = integer ? "i" : "";
    code += preamble;
    code += "uniform " + prefix + "sampler2DRect texture;\n";
    code += function;
    code +=
        ("void main() {\n"
         "   " +
         prefix +
         "vec4 sample =\n"
         "       textureRect(texture, gl_FragCoord.xy);\n"
         "   gl_FragColor = transform(sample);\n"
         "}\n");
    return code;
#endif
}
