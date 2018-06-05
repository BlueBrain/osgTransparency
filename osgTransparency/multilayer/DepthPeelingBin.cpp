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

#include "DepthPeelingBin.h"
#include "Context.h"

#include <string.h>

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
/*
  Static definitions
*/

namespace
{
OpenThreads::Mutex _contextMapMutex;
}

const bool DepthPeelingBin::COMPUTE_MAX_DEPTH_COMPLEXITY =
    ::getenv("OSGTRANSPARENCY_COMPUTE_DEPTH_COMPLEXITY") != 0;
const bool DepthPeelingBin::PROFILE_DEPTH_PARTITION =
    ::getenv("OSGTRANSPARENCY_PROFILE_DEPTH_PARTITION") != 0;
const DepthPeelingBin::DebugPartition DepthPeelingBin::DEBUG_PARTITION;

DepthPeelingBin::ContextMap DepthPeelingBin::s_contextMap;

/*
  Helper classes functions
*/

class DepthPeelingBin::StateObserver : public osg::Observer
{
public:
    void objectDeleted(void *object) final
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_contextMapMutex);
        s_contextMap.erase(object);
    }
} s_contextObserver;

DepthPeelingBin::DebugPartition::DebugPartition()
    : column(-1)
    , row(-1)
    , badPixelFeatures(std::make_pair(-1, -1))
{
    const char *debug = ::getenv("OSGTRANSPARENCY_DEBUG_PARTITION");
    if (debug != 0)
    {
        if (!strncmp(debug, "find=", 5))
        {
            for (debug += 5, badPixelFeatures.first = 0; *debug == '.';
                 ++debug, ++badPixelFeatures.first)
                ;
            badPixelFeatures.second = strtol(debug, 0, 10);
        }
        else
        {
            char *endptr;
            column = strtol(debug, &endptr, 10);
            row = strtol(endptr + 1, &endptr, 10);
        }
    }
}

bool DepthPeelingBin::DebugPartition::debugPixel() const
{
    return column != -1 && row != -1;
}

bool DepthPeelingBin::DebugPartition::findBadPixel() const
{
    return badPixelFeatures.first != -1;
}

bool DepthPeelingBin::DebugPartition::debugAlphaAccumulation() const
{
    static bool debug =
        ::getenv("OSGTRANSPARENCY_DEBUG_ALPHA_ACCUMULATION") != 0;
    return debug;
}

Context &DepthPeelingBin::getContext(const osg::State &state,
                                     const Parameters &parameters)
{
    /* Multiple draw threads might be trying to create their own
       alpha-blending context */
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_contextMapMutex);
    ContextPtr &context = s_contextMap[&state];

    if (!context)
        state.addObserver(&s_contextObserver);

    if (!context || !context->updateParameters(parameters))
        context.reset(new Context(parameters));

    return *context;
}
}
}
}
