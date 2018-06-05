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

#include <osg/GraphicsContext>
#include <osgViewer/Viewer>

namespace
{
/* Forcing the invokation of the static constructors of osgViewer
   to initialize the Windowing System Interface (WSI), otherwise
   GraphicsContext::realize seg faults */
const osgViewer::Viewer viewer;
}

int main(int, char *[])
{
    osg::ref_ptr<osg::GraphicsContext::Traits> traits =
        new osg::GraphicsContext::Traits;
    traits->width = 600;
    traits->height = 480;
    traits->windowDecoration = false;
    traits->red = 8;
    traits->green = 8;
    traits->blue = 8;
    traits->alpha = 8;
    traits->windowDecoration = false;
    traits->pbuffer = true;
    traits->doubleBuffer = false;
    traits->sharedContext = 0;

    osg::ref_ptr<osg::GraphicsContext> context =
        osg::GraphicsContext::createGraphicsContext(traits);
    if (!context->realize())
        abort();
    if (!context->makeCurrent())
        abort();
    if (!context->releaseContext())
        abort();
    context->close();
}
