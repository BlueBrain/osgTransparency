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

#include <osg/GL>

#include "osgTransparency/MultiLayerParameters.h"

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
/*
  Static definitions and constants
*/
static class DefaultSplitPoints
    : public MultiLayerDepthPeelingBin::Parameters::QuantileList
{
public:
    DefaultSplitPoints()
    {
        char *value = ::getenv("OSGTRANSPARENCY_SPLIT_QUANTILES");
        bool useDefault = !value;
        if (value)
        {
            char *endptr = 0;
            bool error = false;
            do
            {
                float quantile = (float)strtod(value, &endptr);
                if (endptr != value)
                {
                    if (size() != 0 && quantile < operator[](size() - 1))
                        error = true;
                    else
                    {
                        push_back(quantile);
                        if (*endptr != '\0')
                            value = endptr + 1;
                        else
                            value = endptr;
                    }
                }
                else if (*endptr != '\0')
                {
                    error = true;
                }
            } while (endptr != value && !error);

            if (error || (endptr == value && *endptr != '\0'))
            {
                useDefault = true;
                std::cerr << "osgTransparency: Error parsing"
                             " OSGTRANSPARENCY_SPLIT_QUANTILES"
                             " default value will be used"
                          << std::endl;
            }
        }
        if (useDefault)
        {
            clear();
            push_back(0.5);
        }
    }
} s_defaultSplitPointQuantiles;

const float s_opacityThreshold =
    ::getenv("OSGTRANSPARENCY_OPACITY_THRESHOLD") &&
            strtod(::getenv("OSGTRANSPARENCY_OPACITY_THRESHOLD"), 0) > 0
        ? strtod(::getenv("OSGTRANSPARENCY_OPACITY_THRESHOLD"), 0)
        : 0.99;

/*
  Helper functions
*/
MultiLayerDepthPeelingBin::Parameters::QuantileList _quantiles(
    unsigned int slices)
{
    if (slices > 8 || slices < 1)
    {
        std::cerr << "osgTransparency: Unsupported number of slices " << slices
                  << ". Ignoring " << std::endl;
        return s_defaultSplitPointQuantiles;
    }
    else
    {
        MultiLayerDepthPeelingBin::Parameters::QuantileList quantiles;
        quantiles.reserve(slices - 1);
        for (unsigned int i = 1; i < slices; ++i)
            quantiles.push_back(i / (float)slices);
        return quantiles;
    }
}

/*
  Member functions
*/
MultiLayerDepthPeelingBin::Parameters::Parameters(OptUInt slices_,
                                                  OptBool unprojectDepths_,
                                                  OptBool alphaAwarePartition_,
                                                  OptUInt superSampling_,
                                                  OptFloat opacityThreshold_)
    : BaseRenderBin::Parameters(superSampling_)
    , opacityThreshold(opacityThreshold_.valid() ? float(opacityThreshold_)
                                                 : s_opacityThreshold)
    , splitPointQuantiles(slices_.valid() ? _quantiles(slices_)
                                          : s_defaultSplitPointQuantiles)
    , unprojectDepths(unprojectDepths_.valid()
                          ? bool(unprojectDepths_)
                          : ::getenv("OSGTRANSPARENCY_UNREPROJECT_DEPTHS") != 0)
    , alphaAwarePartition(
          alphaAwarePartition_.valid()
              ? bool(alphaAwarePartition_)
              : ::getenv("OSGTRANSPARENCY_ADJUST_QUANTILES_WITH_ALPHA") != 0)
{
#ifdef OSG_GL3_AVAILABLE
    if (alphaAwarePartition)
    {
        std::cerr << "Warning: Alpha aware depth partitions not supported in"
                     " GL3 version of multi-layer depth peeling"
                  << std::endl;
    }
#endif
}

bool MultiLayerDepthPeelingBin::Parameters::update(const Parameters &other)
{
    if (!BaseRenderBin::Parameters::update(other))
        return false;

    if (splitPointQuantiles.size() == other.splitPointQuantiles.size() &&
        other.alphaAwarePartition == alphaAwarePartition &&
        other.unprojectDepths == unprojectDepths &&
        other.opacityThreshold == opacityThreshold)
    {
        return true;
    }
    return false;
}
}
}
