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

#include "RenderContext.h"

#include <osg/GraphicsContext>
#include <osgDB/WriteFile>
#include <osgViewer/Viewer>

#include <boost/test/unit_test.hpp>

namespace bbp
{
namespace osgTransparency
{
namespace test
{
namespace
{
/* Just to force the invokation of the static constructors of osgViewer
   that will initialize the Windowing System Interface (WSI). */
const osgViewer::Viewer viewer;
}

namespace detail
{
class RenderContext
{
public:
    RenderContext()
        : savedStateSet(new osg::StateSet)
    {
    }

    osg::ref_ptr<osg::StateSet> savedStateSet;
};
}

RenderContext::RenderContext(const unsigned int width_,
                             const unsigned int height_)
    : width(width_)
    , height(height_)
    , previousLeaf(0)
    , _impl(new detail::RenderContext)
{
    osg::ref_ptr<osg::GraphicsContext::Traits> traits =
        new osg::GraphicsContext::Traits();
    traits->x = 0;
    traits->y = 0;
    traits->width = width;
    traits->height = height;
    traits->red = 8;
    traits->green = 8;
    traits->blue = 8;
    traits->alpha = 8;
    traits->windowDecoration = false;
    traits->pbuffer = true;
    traits->doubleBuffer = false;
    traits->sharedContext = 0;

    context = osg::GraphicsContext::createGraphicsContext(traits);
    BOOST_CHECK(context);
    context->realize();
    context->makeCurrent();

    state = context->getState();
    BOOST_CHECK(state);
    renderInfo = new osg::RenderInfo(state, 0);
}

RenderContext::~RenderContext()
{
    delete renderInfo;
    context->releaseContext();
    context->close();
}

void RenderContext::saveCurrentState(const std::string &filename)
{
    state->captureCurrentState(*_impl->savedStateSet);
    if (!filename.empty())
    {
        osgDB::writeObjectFile(*_impl->savedStateSet, filename);
    }
}

void RenderContext::compareCurrentAndSavedState()
{
    osg::ref_ptr<osg::StateSet> current(new osg::StateSet);
    state->captureCurrentState(*current);

    BOOST_CHECK(*current == *_impl->savedStateSet);
}
}
}
}
