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

#ifndef OSGTRANSPARENCY_MULTILAYER_CANVAS_H
#define OSGTRANSPARENCY_MULTILAYER_CANVAS_H

#include "DepthPeelingBin.h"

#include "osgTransparency/util/constants.h"
#include "osgTransparency/util/helpers.h"

#include <osg/FrameBufferObject>
#include <osg/StateSet>
#include <osg/TextureRectangle>
#include <osg/Uniform>
#include <osg/Viewport>

namespace bbp
{
namespace osgTransparency
{
class OcclusionQueryGroup;

namespace multilayer
{
class Canvas : public osg::Referenced, public osg::Observer
{
public:
    /*--- Public constructors/destructor ---*/

    Canvas(osg::RenderInfo& renderInfo, Context* context);

    /*--- Public member functions ---*/

    unsigned int getWidth() const
    {
        return static_cast<unsigned int>(_camera->getViewport()->width());
    }

    unsigned int getHeight() const
    {
        return static_cast<unsigned int>(_camera->getViewport()->height());
    }

    unsigned int getMaxWidth() const { return _maxWidth; }
    unsigned int getMaxHeight() const { return _maxHeight; }
    void getProjectionMatrixUniforms(Uniforms& uniforms)
    {
        uniforms.insert(_projection_33);
        uniforms.insert(_projection_34);
    }

    bool valid(const osg::Camera* camera);

    bool checkFinished();

    void startFrame(MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
                    osgUtil::RenderLeaf*& previous);

    void peel(MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo);

    void blend(osg::RenderInfo& renderInfo);

    void finishFrame(osg::RenderInfo& renderInfo);

protected:
    /*--- Protected constructors/destructor ---*/

    ~Canvas();

    /*--- Protected member functions ---*/

    void objectDeleted(void*) final;

private:
    /*--- Private declarations ---*/

    typedef std::list<osg::ref_ptr<osg::FrameBufferObject>> FBOList;
    typedef std::list<osg::ref_ptr<osg::Texture>> TextureList;

    /*--- Private member variables ---*/

    Context* _context;

    /* Viewport and camera state */
    osg::Camera* _camera;
    unsigned int _maxWidth;
    unsigned int _maxHeight;
    osg::ref_ptr<osg::Viewport> _viewport;
    osg::ref_ptr<osg::Uniform> _projection_33;
    osg::ref_ptr<osg::Uniform> _projection_34;
    osg::ref_ptr<osg::Uniform> _lowerLeftCorner;

    /* Counters */
    unsigned int _pass;
    int _index;
    unsigned int _lastSamplesPassed;
    unsigned int _timesSamplesRepeated;

    /* Textures and buffers */
    osg::ref_ptr<osg::FrameBufferObject> _peelFBO;
    osg::ref_ptr<osg::TextureRectangle> _depthTextures[MAX_DEPTH_BUFFERS][2];
#ifdef OSG_GL3_AVAILABLE
    osg::ref_ptr<osg::Texture2DArray> _frontColors;
    osg::ref_ptr<osg::Texture2DArray> _backColors;
#else
    osg::ref_ptr<osg::TextureRectangle> _colorTextures[MAX_SLICES];
    osg::ref_ptr<osg::TextureRectangle>
        _targetBlendColorTextures[MAX_SLICES * 2];
#endif
    osg::ref_ptr<osg::FrameBufferObject> _blendBuffers[2];
    osg::ref_ptr<osg::FrameBufferObject> _auxiliaryBuffer;

    /* State sets and shader maps */
    osg::ref_ptr<osg::StateSet> _blendStateSets[2];
    osg::ref_ptr<osg::StateSet> _firstPassStateSet;
    ProgramMap _firstPassPrograms;
    osg::ref_ptr<osg::StateSet> _peelStateSet;
    ProgramMap _peelPassPrograms;
    osg::ref_ptr<osg::StateSet> _finalStateSet;

    ProgramMap _extraShaders;

    /* Helper objects */

    osg::ref_ptr<osg::Geometry> _quad;
    osg::ref_ptr<DepthPartitioner> _depthPartitioner;
    osg::ref_ptr<OcclusionQueryGroup> _queryGroup;

    /*--- Private member functions ---*/

    void _createBuffersAndTextures();

    void _createStateSets();
    void _createFirstPassStateSet();
    void _createPeelStateSet();
    void _createBlendStateSet();
    void _createFinalCopyStateSet();

    void _updateProjectionMatrixUniforms();

    void _preparePeelFBOAndTextures(osg::RenderInfo& renderInfo);

#ifndef OSG_GL3_AVAILABLE
    void _blendSlices(osg::RenderInfo& renderInfo, const bool back);
#endif

    void _updateShaderPrograms(const ProgramMap& extraShaders);

    /* Debug functions */
    void _showPeelPassTextures(osg::State& state);
    void _debugAlphaAccumulationAtPeelPass();
    void _debugAlphaAccumulationAtBlendPass(osg::State& state);
};
}
}
}
#endif
