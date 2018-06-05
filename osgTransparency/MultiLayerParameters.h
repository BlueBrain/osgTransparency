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

#ifndef OSGTRANSPARENCY_MULTILAYERPARAMETERS_H
#define OSGTRANSPARENCY_MULTILAYERPARAMETERS_H

#include "BaseParameters.h"
#include "MultiLayerDepthPeelingBin.h"

namespace bbp
{
namespace osgTransparency
{
class OSGTRANSPARENCY_API MultiLayerDepthPeelingBin::Parameters
    : public BaseRenderBin::Parameters
{
public:
    /*--- Public declarations ---*/

    typedef std::vector<float> QuantileList;

    /*--- Public members ---*/

    const float opacityThreshold;
    const QuantileList splitPointQuantiles;
    const bool unprojectDepths;
    const bool alphaAwarePartition;

    /*--- Public constructors/destructor ---*/

    /**
       Creates a Parameters object

       @param slices Number of depth slices to use for depth peeling. A number
                  between 1 and 8.
       @param unprojectDepths Whether use unprojected z values or not.
       @param alphaAwarePartition Adjust the quatiles for the depth partition
                  based on alpha values (This only makes sense if slices >= 2).
       @param superSampling Unimplemented
       @param opacityThreshold The accumulated opacity at a fragment at which
                  fragments behind can be considered completely occluded.

       Default values for unspecified arguments are
       * opacityThreshold: 0.99
       * splitPointQuantiles: [0.5]
       * unprojectDepths: false
       * alphaAwarePartition: false

       The following environmental variables are looked up to override
       defaults:
       * OSGTRANSPARENCY_REPROJECT_QUANTILES
       * OSGTRANSPARENCY_ALPHA_AWARE_PARTITION
       * OSGTRANSPARENCY_OPACITY_THRESHOLD
       * OSGTRANSPARENCY_SPLIT_QUANTILES: A list of exact quantiles to use
       instead of even spacing.
    */
    Parameters(OptUInt slices = OptUInt(), OptBool unprojectDepths = OptBool(),
               OptBool alphaAwarePartition = OptBool(),
               OptUInt superSampling = 1,
               OptFloat opacityThreshold = OptFloat());

    /*--- Public member functions ---*/

    /** @sa BaseRenderBin::Parameters::update */
    bool update(const Parameters &other);

    /** @internal */
    unsigned int getNumSlices() const { return splitPointQuantiles.size() + 1; }
    /** @internal */
    unsigned int getAdjustedNumPoints() const
    {
        if (alphaAwarePartition)
            /* Adding an extra split point to detect which fragments fall
               beyond the depth at which opacity has been found to
               accumulate over the threshold. */
            return splitPointQuantiles.size() + 1;
        else
            return splitPointQuantiles.size();
    }
};
}
}
#endif
