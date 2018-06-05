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

#ifndef OSGTRANSPARENCY_UTIL_CONSTANTS_H
#define OSGTRANSPARENCY_UTIL_CONSTANTS_H

// Including GL headers for various OpenGL tokens
#ifdef _WIN32
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>
#include <osg/GL>
#endif

#include <osg/BlendEquation>
#include <osg/Camera>
#include <osg/StateAttribute>
#include <string>

namespace bbp
{
namespace osgTransparency
{
/*
   Compile time constants
*/
static const unsigned int SUPERSAMPLING_FACTOR = 3;
static const unsigned int MAX_SLICES = 8;
static const unsigned int MAX_DEPTH_BUFFERS = MAX_SLICES / 2;

/* Some declarations used to enhance readbility */
static const int OFF_OVERRIDE =
    osg::StateAttribute::OVERRIDE | osg::StateAttribute::OFF;
static const int ON_OVERRIDE =
    osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON;
static const int OFF_PROTECTED =
    osg::StateAttribute::PROTECTED | osg::StateAttribute::OFF;
static const int ON_PROTECTED =
    osg::StateAttribute::PROTECTED | osg::StateAttribute::ON;
static const int ON_OVERRIDE_PROTECTED = osg::StateAttribute::PROTECTED |
                                         osg::StateAttribute::ON |
                                         osg::StateAttribute::OVERRIDE;
static const int OFF = osg::StateAttribute::OFF;
static const int ON = osg::StateAttribute::ON;
static const osg::BlendEquation::Equation FUNC_ADD =
    osg::BlendEquation::FUNC_ADD;
static const osg::BlendEquation::Equation RGBA_MAX =
    osg::BlendEquation::RGBA_MAX;

static const osg::Camera::BufferComponent COLOR_BUFFERS[] = {
    osg::Camera::COLOR_BUFFER0, osg::Camera::COLOR_BUFFER1,
    osg::Camera::COLOR_BUFFER2, osg::Camera::COLOR_BUFFER3,
    osg::Camera::COLOR_BUFFER4, osg::Camera::COLOR_BUFFER5,
    osg::Camera::COLOR_BUFFER6, osg::Camera::COLOR_BUFFER7};

static const GLenum GL_BUFFER_NAMES[] = {
    GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
    GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7};

inline std::string sm(const char *main)
{
    /* The forward declaration of shadeVertex is used for trivial shaders that
       forward the vertex shading to the default vertex shading contained in
       an external StateSet */
    static std::string preamble(
        "//sm\nvoid shadeVertex(); void trivialShadeVertex();"
        "void main() {");
    return preamble + main + "}";
}

extern const std::string BYPASS_VERT_SHADER;
extern const std::string TRIVIAL_VERT_SHADER;
extern const std::string PROJECT_UNPROJECT_SNIPPET;

/*
   Run-time time constants depending on environmental variables
*/
extern bool ADJUST_QUANTILES_WITH_ALPHA;
extern bool COMPUTE_MAX_DEPTH_COMPLEXITY;
extern bool PROFILE_DEPTH_PARTITION;
extern float OPACITY_THRESHOLD;
}
}
#endif
