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

#include "BaseParameters.h"

namespace bbp
{
namespace osgTransparency
{
/*
  Static definitions and constants
*/
bool s_singleQueryPerPass = ::getenv("OSGTRANSPARENCY_SINGLE_QUERY") != 0;

/*
  Constructor
*/
BaseRenderBin::Parameters::Parameters(OptUInt superSampling_)
    : maximumPasses(100)
    , samplesCutoff(0)
    , reservedTextureUnits(4)
    , superSampling(superSampling_)
    , singleQueryPerPass(s_singleQueryPerPass)
{
}

/*
  Member functions
*/
bool BaseRenderBin::Parameters::update(const Parameters &other)
{
    maximumPasses = other.maximumPasses;
    samplesCutoff = other.samplesCutoff;
    return true;
}

boost::shared_ptr<BaseRenderBin::Parameters> BaseRenderBin::Parameters::clone()
    const
{
    return boost::shared_ptr<Parameters>(new Parameters(*this));
}
}
}
