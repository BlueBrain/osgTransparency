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

#include <osg/Image>

namespace osg
{
class Viewport;
}

namespace bbp
{
namespace osgTransparency
{
namespace test
{
bool compare(const osg::Image &image1, const osg::Image &image2,
             const float channelThreshold = 1.0);

bool compare(const std::string &referenceFile, const int x, const int y,
             const int width, const int height,
             const GLenum pixelFormat = GL_RGBA,
             const GLenum type = GL_UNSIGNED_BYTE,
             const float channelThreshold = 1.0);

bool compare(const std::string &referenceFile, const osg::Viewport &viewport,
             const GLenum pixelFormat = GL_RGBA,
             const GLenum type = GL_UNSIGNED_BYTE,
             const float channelThreshold = 1.0);
}
}
}
