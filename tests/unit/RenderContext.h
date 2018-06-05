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

#include <osg/ref_ptr>

#include <string>

namespace osg
{
class GraphicsContext;
class State;
class RenderInfo;
}

namespace osgUtil
{
class RenderLeaf;
}

namespace bbp
{
namespace osgTransparency
{
namespace test
{
namespace detail
{
class RenderContext;
}

struct RenderContext
{
    osg::ref_ptr<osg::GraphicsContext> context;
    const unsigned int width;
    const unsigned int height;
    osgUtil::RenderLeaf *previousLeaf;

    osg::RenderInfo *renderInfo;
    osg::ref_ptr<osg::State> state;

    /** Creates and makes current and OpenGL context with an offsreen
        framebuffer of the desired size. */
    RenderContext(const unsigned int width, const unsigned int height);
    virtual ~RenderContext();

    void saveCurrentState(const std::string &filename = "");
    void compareCurrentAndSavedState();

private:
    RenderContext(const RenderContext &);
    RenderContext &operator=(const RenderContext &);

    detail::RenderContext *_impl;
};

template <int WIDTH = 600, int HEIGHT = 400>
struct SizedRenderContext : public RenderContext
{
    SizedRenderContext()
        : RenderContext(WIDTH, HEIGHT)
    {
    }
};
}
}
}
