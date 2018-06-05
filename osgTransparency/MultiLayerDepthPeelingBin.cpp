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

#include "MultiLayerParameters.h"
#include "multilayer/Canvas.h"
#include "multilayer/Context.h"
#include "multilayer/DepthPeelingBin.h"
#include "util/constants.h"
#include "util/trace.h"

#include <osg/Config>

#include <iostream>
#include <stdlib.h>
#include <string.h>

namespace bbp
{
namespace osgTransparency
{
/*
  Constructors/destructor
*/
MultiLayerDepthPeelingBin::MultiLayerDepthPeelingBin()
{
}

MultiLayerDepthPeelingBin::MultiLayerDepthPeelingBin(
    const Parameters& parameters)
    : BaseRenderBin(boost::shared_ptr<Parameters>(new Parameters(parameters)))
{
}

MultiLayerDepthPeelingBin::MultiLayerDepthPeelingBin(
    const MultiLayerDepthPeelingBin& renderBin, const osg::CopyOp& copyop)
    : BaseRenderBin(renderBin, copyop)
{
}

MultiLayerDepthPeelingBin::~MultiLayerDepthPeelingBin()
{
}

/*
  Member functions
*/

const MultiLayerDepthPeelingBin::Parameters&
    MultiLayerDepthPeelingBin::getParameters() const
{
    return static_cast<const Parameters&>(*_parameters);
}

MultiLayerDepthPeelingBin::Parameters&
    MultiLayerDepthPeelingBin::getParameters()
{
    return static_cast<Parameters&>(*_parameters);
}

void MultiLayerDepthPeelingBin::sort()
{
    /* Do nothing as sortByState does nothing in the original implementation
       from OSG */
}

void MultiLayerDepthPeelingBin::drawImplementation(
    osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* This render bin must be transparent to the state management.
       Our goal here is that OSG has a consistent State object and that
       the following algorithm is transparent to any calling prerender bins.
       That means that they have to find the state stack and the current
       OpenGL state in the state you can expect from a regular bin. */

    /* Each frame a new alphablend::MultiLayerDepthPeelingBin is created by OSG
       internals we can't rely on the attributes of this class and have to store
       frame to frame state in an static per context map. */

    /** \bug The current implementation doesn't support recursive calls
        resulting from nested render bins */
    osg::State& state = *renderInfo.getState();
    multilayer::Context& context = multilayer::DepthPeelingBin::getContext(
        state, static_cast<const Parameters&>(*_parameters));

    context.startFrame(this, renderInfo, previous);

    multilayer::Canvas* canvas = context.getCanvas();
    while (!canvas->checkFinished())
    {
        canvas->peel(this, renderInfo);
        canvas->blend(renderInfo);
    }

    context.finishFrame(renderInfo, previous);
}
}
}
