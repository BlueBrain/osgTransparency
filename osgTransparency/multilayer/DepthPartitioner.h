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

#ifndef OSGTRANSPARENCY_MULTILAYER_DEPTHPARTITIONER_H
#define OSGTRANSPARENCY_MULTILAYER_DEPTHPARTITIONER_H

#include "DepthPeelingBin.h"

namespace osg
{
class FrameBufferObject;
class RenderInfo;
class TextureRectangle;
}

namespace bbp
{
namespace osgTransparency
{
class MultiLayerDepthPeelingBin;

namespace multilayer
{
class DepthPartitioner : public osg::Referenced
{
public:
    /*--- Public constructors and destructor ---*/

    /* Stores a reference to the parameters */
    DepthPartitioner(const Parameters& parameters);

    virtual ~DepthPartitioner();

    /*--- Public member functions ---*/

    virtual void createBuffersAndTextures(unsigned int maxWidth,
                                          unsigned int maxHeight) = 0;

    virtual void createStateSets() = 0;

    void updateProjectionUniforms(const osg::Matrix& projection);

    virtual void updateShaderPrograms(const ProgramMap& extraShaders) = 0;

    virtual void computeDepthPartition(MultiLayerDepthPeelingBin* bin,
                                       osg::RenderInfo& renderInfo,
                                       osgUtil::RenderLeaf*& previous) = 0;

    /**
       Adds the depthPartition uniform to the stateset
    */
    virtual void addDepthPartitionExtraState(osg::StateSet* stateSet,
                                             unsigned int textureUnitHint);
    /**
       Completes the programs with functions specific to this depth partition.
    */
    virtual void addDepthPartitionExtraShaders(ProgramMap& programs);

    virtual size_t computeMaxDepthComplexity(MultiLayerDepthPeelingBin* bin,
                                             osg::RenderInfo& renderInfo,
                                             osgUtil::RenderLeaf*& previous);

    virtual void profileDepthPartition(MultiLayerDepthPeelingBin* bin,
                                       osg::RenderInfo& renderInfo,
                                       osgUtil::RenderLeaf*& previous);

    virtual osg::ref_ptr<osg::TextureRectangle>*
        getDepthPartitionTextureArray() = 0;

protected:
    /*--- Protected member attributes ---*/

    const Parameters& _parameters;

    osg::ref_ptr<osg::Uniform> _projection_33;
    osg::ref_ptr<osg::Uniform> _projection_34;

    /*--- Protected member functions ---*/
    void render(MultiLayerDepthPeelingBin* bin, osg::RenderInfo& renderInfo,
                osgUtil::RenderLeaf*& previous, osg::StateSet* baseStateSet,
                ProgramMap& programs, osg::RefMatrix* projection = 0,
                OcclusionQueryGroup* queryGroup = 0)
    {
        bin->render(renderInfo, previous, baseStateSet, programs, projection,
                    queryGroup);
    }

    void renderBounds(MultiLayerDepthPeelingBin* bin,
                      osg::RenderInfo& renderInfo, osg::StateSet* baseStateSet,
                      ProgramMap& programs, BoundShapesStorage& shapes,
                      osg::RefMatrix* projection = 0)
    {
        bin->renderBounds(renderInfo, baseStateSet, programs, shapes,
                          projection);
    }
};
}
}
}
#endif
