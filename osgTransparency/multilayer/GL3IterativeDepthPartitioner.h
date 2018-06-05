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

#ifndef OSGTRANSPARENCY_MULTILAYER_GL3ITERATIVEDEPTHPARTITIONER_H
#define OSGTRANSPARENCY_MULTILAYER_GL3ITERATIVEDEPTHPARTITIONER_H

#include "DepthPartitioner.h"

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
class GL3IterativeDepthPartitioner : public DepthPartitioner
{
public:
    /*--- Public constructors/destructor ---*/

    GL3IterativeDepthPartitioner(const Parameters& parameters);

    /*--- Public member functions ---*/

    void createBuffersAndTextures(unsigned int maxWidth,
                                  unsigned int maxHeight) final;

    void createStateSets() final;

    virtual void updateShaderPrograms(const ProgramMap& extraShaders) final;

    void computeDepthPartition(MultiLayerDepthPeelingBin* bin,
                               osg::RenderInfo& renderInfo,
                               osgUtil::RenderLeaf*& previous) final;

    osg::ref_ptr<osg::TextureRectangle>* getDepthPartitionTextureArray()
    {
        return _depthPartitionTexture;
    }

private:
    /*--- Private member attributes ---*/
    static std::string _shaderPath;

    /* State sets with the different operations for depth partitioning */
    osg::ref_ptr<osg::StateSet> _minMax;
    osg::ref_ptr<osg::StateSet> _firstCount;
    osg::ref_ptr<osg::StateSet> _firstFindQuantileIntervals;
    osg::ref_ptr<osg::StateSet> _countIteration;
    osg::ref_ptr<osg::StateSet> _findQuantileIntervals;
    osg::ref_ptr<osg::StateSet> _finalProjection;

    osg::ref_ptr<osg::Viewport> _viewport;

    osg::ref_ptr<osg::Uniform> _iteration;
    osg::ref_ptr<osg::Uniform> _quantiles;
    osg::ref_ptr<osg::Uniform> _quantiles2;

    osg::ref_ptr<osg::TextureRectangle> _minMaxTexture;
    std::vector<osg::ref_ptr<osg::TextureRectangle>> _countTextures;
    osg::ref_ptr<osg::TextureRectangle> _codedIntervalsTextures[2];
    osg::ref_ptr<osg::TextureRectangle> _leftAccumTextures[2];
    osg::ref_ptr<osg::TextureRectangle> _totalCountsTexture;

    osg::ref_ptr<osg::TextureRectangle> _depthPartitionTexture[2];

    osg::ref_ptr<osg::FrameBufferObject> _auxiliaryBuffer;

    osg::ref_ptr<osg::Geometry> _quad;

    ProgramMap _minMaxPrograms;
    ProgramMap _firstCountPrograms;
    ProgramMap _countIterationPrograms;

    BoundShapesStorage _shapes;

    struct DebugHelpers;

    /*--- Private member functions ---*/

    void _createMinMaxCalculationStateSet();
    void _createFirstCountStateSet();
    void _createCountIterationStateSet(size_t points);
    void _createFirstFindQuantileIntervalsStateSet(
        const std::vector<float>& quantiles);
    void _createFindQuantileIntervalsStateSet(
        const std::vector<float>& quantiles);
    void _createFinalReprojectionStateSet(unsigned int points);

    void _updateMinMaxCalculationPrograms(const ProgramMap& extraShaders);
    void _updateFirstCountPrograms(const ProgramMap& extraShaders);
    void _updateCountIterationPrograms(const ProgramMap& extraShaders,
                                       size_t points);
};
}
}
}
#endif
