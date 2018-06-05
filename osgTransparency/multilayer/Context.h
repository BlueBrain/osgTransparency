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

#ifndef OSGTRANSPARENCY_MULTILAYER_CONTEXT_H
#define OSGTRANSPARENCY_MULTILAYER_CONTEXT_H

#include "DepthPeelingBin.h"

#include "osgTransparency/MultiLayerParameters.h"
#include "osgTransparency/util/constants.h"

#include <osg/FrameBufferObject>
#include <osg/Texture2DArray>
#include <osg/TextureRectangle>

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
class Canvas;

class Context
{
public:
    /*--- Public declarations ---*/

    osgUtil::RenderLeaf* oldPrevious;

    /*--- Public  constructors/destructor ---*/

    Context(const Parameters& parameters);

    ~Context();

    /*--- Public member functions ---*/

    const Parameters& getParameters() const { return _parameters; }
    /** Returns false if new parameters are incompatible with this context */
    bool updateParameters(const Parameters& parameters);

    Canvas* getCanvas() { return _canvas.get(); }
    unsigned int getID() { return _id; }
    void startFrame(MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
                    osgUtil::RenderLeaf*& previous);

    void finishFrame(osg::RenderInfo& renderInfo,
                     osgUtil::RenderLeaf*& previous);

    const osg::FrameStamp* getFrameStamp() { return _frameStamp.get(); }
private:
    /*--- Private member variables ---*/

    unsigned int _id;
    osg::ref_ptr<Canvas> _canvas;

    GLint _previousFBO;
    unsigned int _savedStackPosition;

    Parameters _parameters;

#ifndef NDEBUG
    osg::ref_ptr<osg::StateSet> _oldState;
#endif

    osg::ref_ptr<const osg::FrameStamp> _frameStamp;

    /*--- Private member functions ---*/

    /**
       Recovers the frame stamp from the osgUtil::SceneView object that is
       being rendered.
       renderInfo.getState().getFrameStamp() has race conditions in draw thread
       per context configurations.
       This function accesses the framestamp from the current SceneView,
       which can be thread safe if a different FrameStamp object is used each
       frame.
     */
    void _updateFrameStamp(BaseRenderBin* bin, osg::RenderInfo& renderInfo);
};
}
}
}
#endif
