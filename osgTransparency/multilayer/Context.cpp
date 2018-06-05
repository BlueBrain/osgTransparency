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

#define GL_GLEXT_PROTOTYPES
#include <osg/GL>

#include <GL/glext.h>

#include "Canvas.h"
#include "Context.h"
#ifdef OSG_GL3_AVAILABLE
#include "GL3IterativeDepthPartitioner.h"
#else
#include "IterativeDepthPartitioner.h"
#endif
#include "../MultiLayerParameters.h"

#include "../util/TextureDebugger.h"
#include "../util/extensions.h"
#include "../util/helpers.h"
#include "../util/loaders.h"
#include "../util/strings_array.h"
#include "../util/trace.h"

#include <osg/BlendFunc>
#include <osg/ColorMask>
#include <osg/Depth>
#include <osg/LogicOp>
#include <osg/Texture2DArray>
#include <osg/Version>
#include <osgDB/WriteFile>
#include <osgUtil/SceneView>
#include <osgViewer/Renderer>

#include <boost/format.hpp>

#include <map>

#if defined(__APPLE__)
#ifndef GL_VERSION_3_0
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif
#endif

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
/*
  Constructors
*/
Context::Context(const Parameters& parameters)
    : oldPrevious(0)
    , _id(0)
    , _savedStackPosition(0)
    , _parameters(parameters)
{
}

/*
  Destructor
*/
Context::~Context()
{
}

/*
  Member functions
*/

bool Context::updateParameters(const Parameters& parameters)
{
    return _parameters.update(parameters);
}

void Context::startFrame(MultiLayerDepthPeelingBin* bin,
                         osg::RenderInfo& renderInfo,
                         osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    _updateFrameStamp(bin, renderInfo);

    /* Saving part of the state to be restored at the end of the drawing. */
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &_previousFBO);
    oldPrevious = previous;
    _savedStackPosition = state.getStateSetStackSize();
#ifndef NDEBUG
    _oldState = new osg::StateSet;
    state.captureCurrentState(*_oldState);
#endif

    osg::Camera* camera = renderInfo.getCurrentCamera();
    if (_canvas == 0 || !_canvas->valid(camera))
    {
        _id = state.getContextID();
        /* Creating a new canvas for the camera */
        _canvas = new Canvas(renderInfo, this);
    }

    _canvas->startFrame(bin, renderInfo, previous);
}

void Context::finishFrame(osg::RenderInfo& renderInfo,
                          osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* Returning to previously bound buffer. */
    osg::State& state = *renderInfo.getState();
    FBOExtensions* fbo_ext = getFBOExtensions(state.getContextID());
    fbo_ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _previousFBO);

    _canvas->finishFrame(renderInfo);

    state.popStateSetStackToSize(_savedStackPosition);
    previous = oldPrevious;
    state.apply();

#ifndef NDEBUG
    osg::ref_ptr<osg::StateSet> currentState(new osg::StateSet);
    state.captureCurrentState(*currentState);
    assert(*currentState == *_oldState);
#endif
}

void Context::_updateFrameStamp(BaseRenderBin* bin, osg::RenderInfo& renderInfo)
{
    osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(
        renderInfo.getCurrentCamera()->getRenderer());
    if (renderer)
    {
        for (int i = 0; i < 2; ++i)
        {
            osgUtil::SceneView* sv = renderer->getSceneView(i);
            if ((sv->getDisplaySettings() != 0 &&
                 sv->getDisplaySettings()->getStereo() &&
                 (sv->getRenderStageLeft() == bin->getStage() ||
                  sv->getRenderStageRight() == bin->getStage())) ||
                sv->getRenderStage() == bin->getStage())
            {
                _frameStamp = sv->getFrameStamp();
                break;
            }
        }
    }
    else
    {
        _frameStamp = renderInfo.getState()->getFrameStamp();
    }
}
}
}
}
