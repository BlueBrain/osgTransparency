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

#include "constants.h"
#include "helpers.h"

#include <osg/FrameBufferObject>
#include <osg/GLExtensions>
#include <osg/Geometry>
#include <osg/Program>
#include <osg/Texture2DArray>
#include <osg/TextureRectangle>

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
/*
  Helper classes
*/
class Extensions : public osg::Referenced
{
    /* Constructor */
protected:
    Extensions()
    {
        osg::setGLExtensionFuncPtr(_glClearColorIi, "glClearColorIi",
                                   "glClearColorIiEXT");
        osg::setGLExtensionFuncPtr(_glClearColorIui, "glClearColorIui",
                                   "glClearColorIuiEXT");
        if (_glClearColorIi == 0 || _glClearColorIui == 0)
        {
            std::cerr << "RTNeuron: glClearColorIi or glClearColorIui "
                         "extensions not available"
                      << std::endl;
            abort();
        }
    }

    /* Member functions */
public:
    static Extensions* getOrCreate(int contextID)
    {
        static OpenThreads::Mutex _mutex;
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

        osg::ref_ptr<Extensions>& ptr = _extensions[contextID];
        if (!ptr.valid())
        {
            ptr = new Extensions();
        }
        return ptr.get();
    }

    void glClearColorIi(GLint r, GLint g, GLint b, GLint a)
    {
        _glClearColorIi(r, g, b, a);
    }
    void glClearColorIui(GLuint r, GLuint g, GLuint b, GLuint a)
    {
        _glClearColorIui(r, g, b, a);
    }

protected:
    static std::map<int, osg::ref_ptr<Extensions>> _extensions;

    typedef void(APIENTRY* ClearColorIuiProc)(GLuint, GLuint, GLuint, GLuint);
    ClearColorIuiProc _glClearColorIui;
    typedef void(APIENTRY* ClearColorIiProc)(GLint, GLint, GLint, GLint);
    ClearColorIiProc _glClearColorIi;
};
std::map<int, osg::ref_ptr<Extensions>> Extensions::_extensions;

/*
  Helper functions
*/
namespace
{
#ifdef OSG_GL3_AVAILABLE

void chooseClearColor(osg::State& state, osg::Texture::InternalFormatType type,
                      const osg::Vec4& value)
{
    switch (type)
    {
    case osg::Texture::SIGNED_INTEGER:
    {
        Extensions* ext = Extensions::getOrCreate(state.getContextID());
        ext->glClearColorIi(GLint(value[0]), GLint(value[1]), GLint(value[2]),
                            GLint(value[3]));
        break;
    }
    case osg::Texture::UNSIGNED_INTEGER:
    {
        Extensions* ext = Extensions::getOrCreate(state.getContextID());
        ext->glClearColorIui(GLuint(value[0]), GLuint(value[1]),
                             GLuint(value[2]), GLuint(value[3]));
        break;
    }
    default:
        glClearColor(value[0], value[1], value[2], value[3]);
    }
}
#endif
}

/*
  Free functions
*/
osg::Program* copyProgram(const osg::Program* program)
{
    osg::Program* copy = new osg::Program;
    const osg::Program::AttribBindingList& attribs =
        program->getAttribBindingList();
    for (osg::Program::AttribBindingList::const_iterator i = attribs.begin();
         i != attribs.end(); ++i)
        copy->addBindAttribLocation(i->first, i->second);

    copy->setParameter(GL_GEOMETRY_VERTICES_OUT_EXT,
                       program->getParameter(GL_GEOMETRY_VERTICES_OUT_EXT));
    copy->setParameter(GL_GEOMETRY_INPUT_TYPE_EXT,
                       program->getParameter(GL_GEOMETRY_INPUT_TYPE_EXT));
    copy->setParameter(GL_GEOMETRY_OUTPUT_TYPE_EXT,
                       program->getParameter(GL_GEOMETRY_OUTPUT_TYPE_EXT));

    for (size_t i = 0; i < program->getNumShaders(); ++i)
        copy->addShader(new osg::Shader(*program->getShader(i)));
    copy->setName(program->getName());
    return copy;
}

void updateProgramMap(const ProgramMap& current, ProgramMap& old,
                      ProgramMap& updates)
{
    updates.clear();
    /* This loop finds which state sets are new or have been updated with
       a new program in O(n) time if there are no changes. */
    ProgramMap::iterator j = old.begin();
    for (ProgramMap::const_iterator i = current.begin(); i != current.end();
         ++i)
    {
        if (j == old.end() || j->first != i->first)
        {
            j = old.insert(j, *i);
            updates.insert(updates.end(), *i);
        }
        else if (j->second != i->second)
        {
            j->second = i->second;
            updates.insert(updates.end(), *i);
        }
        if (j != old.end())
            ++j;
    }
}

osg::ref_ptr<osg::Geometry> createQuad()
{
    osg::ref_ptr<osg::Geometry> quad = new osg::Geometry();
    osg::Vec2Array* vertices = new osg::Vec2Array();
    vertices->push_back(osg::Vec2(-1, -1));
    vertices->push_back(osg::Vec2(1, -1));
    vertices->push_back(osg::Vec2(-1, 1));
    vertices->push_back(osg::Vec2(1, 1));
    quad->setVertexArray(vertices);
    quad->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    return quad;
}

#ifdef OSG_GL3_AVAILABLE
void clearTexture(osg::State& state, osg::FrameBufferObject* fbo,
                  osg::TextureRectangle* texture, const osg::Vec4& value,
                  bool rebindPreviousFBO)
{
    GLint previous;
    GLint drawBuffer;
    if (rebindPreviousFBO)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previous);
        glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
    }

    osg::FrameBufferAttachment buffer(texture);
    fbo->setAttachment(osg::Camera::COLOR_BUFFER0, buffer);
    fbo->apply(state);
    chooseClearColor(state, texture->getInternalFormatType(), value);
    glClear(GL_COLOR_BUFFER_BIT);
    if (rebindPreviousFBO)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, previous);
        glDrawBuffer(drawBuffer);
    }
}

void clearTexture(osg::State& state, osg::FrameBufferObject* fbo,
                  osg::Texture2DArray* texture, const osg::Vec4& value,
                  bool rebindPreviousFBO)
{
    GLint previous;
    GLint drawBuffer;
    if (rebindPreviousFBO)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previous);
        glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
    }

    chooseClearColor(state, texture->getInternalFormatType(), value);

    for (int i = 0; i < texture->getTextureDepth(); i += 8)
    {
        for (int j = 0; i + j < texture->getTextureDepth() && j < 8; ++j)
        {
            osg::FrameBufferAttachment buffer(texture, j + i);
            fbo->setAttachment(COLOR_BUFFERS[j + i], buffer);
        }
        fbo->apply(state);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (rebindPreviousFBO)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, previous);
        glDrawBuffer(drawBuffer);
    }
}
#endif
}
}
