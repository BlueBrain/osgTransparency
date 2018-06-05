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

#include <osg/Config>

#include "constants.h"

namespace bbp
{
namespace osgTransparency
{
/*
  Compile time constants
*/
#ifdef OSG_GL3_AVAILABLE
const std::string BYPASS_VERT_SHADER =
    "//BYPASS\n"
    "#version 410\n"
    "in vec4 osg_Vertex;\n"
    "void main() { gl_Position = osg_Vertex; }";
const std::string TRIVIAL_VERT_SHADER =
    "//TRIVIAL\n"
    "#version 410\n"
    "uniform mat4x4 osg_ModelViewProjectionMatrix;\n"
    "in vec4 osg_Vertex;\n"
    "void main() { gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex; }";
#else
const std::string BYPASS_VERT_SHADER =
    "//BYPASS\nvoid main() { gl_Position = gl_Vertex; }";
const std::string TRIVIAL_VERT_SHADER =
    "//TRIVIAL\nvoid main() { gl_Position = ftransform(); }";
#endif

const std::string PROJECT_UNPROJECT_SNIPPET =
    "uniform float proj33;\n"
    "uniform float proj34;\n"
    "float unproject(float x) { return -proj34 / (proj33 + 2.0 * x - 1.0); }\n"
    "float reproject(float x) { return 0.5 * (1.0 - proj33 - proj34 / x); }\n";
}
}
