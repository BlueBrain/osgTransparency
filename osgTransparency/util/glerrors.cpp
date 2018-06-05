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

#ifndef NDEBUG
#include "glerrors.h"

#define GL_GLEXT_PROTOTYPES
#include <osg/GL>

#include <GL/glext.h>

#if defined(__APPLE__)
#include <OpenGL/glu.h>
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#else
#include <GL/glu.h>
#endif

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
void checkGLErrors(const std::string &message)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        std::cout << "OpenGL error detected: " << gluErrorString(error) << ", "
                  << message << std::endl;
}
}
}
#endif
