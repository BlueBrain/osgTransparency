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

#ifndef OSGTRANSPARENCY_UTIL_TEXTUREDEBUGGER_H
#define OSGTRANSPARENCY_UTIL_TEXTUREDEBUGGER_H

#include <string>

#include <boost/noncopyable.hpp>

namespace osg
{
class GraphicsContext;
class TextureRectangle;
class Texture2DArray;
}

class TextureDebugger : boost::noncopyable
{
public:
    TextureDebugger();

    ~TextureDebugger();

    void setWindowName(const std::string& name);

    void renderTexture(osg::GraphicsContext* context,
                       osg::TextureRectangle* texture,
                       const std::string& code = "");

    void renderTexture(osg::GraphicsContext* context,
                       osg::Texture2DArray* texture, const unsigned int layer,
                       const std::string& code = "");

    /* Fuction prototype must be either ivec4 transform(ivec4 sample) or
       vec4 transform(vec4 sample) depending on whether it's going to
       be used with a integer texture or not */
    static std::string GLSL(const std::string& function, bool integer = false,
                            const std::string& preamble = "");

protected:
    struct Impl;
    Impl* _impl;
};

#endif
