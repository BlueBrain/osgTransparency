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

#ifndef OSGTRANSPARENCY_MULTILAYER_DEPTHPEELINGBIN_H
#define OSGTRANSPARENCY_MULTILAYER_DEPTHPEELINGBIN_H

#include "osgTransparency/MultiLayerDepthPeelingBin.h"

namespace bbp
{
namespace osgTransparency
{
namespace multilayer
{
typedef MultiLayerDepthPeelingBin::Parameters Parameters;

class Context;

class DepthPeelingBin
{
    /*-- Public declarations ---*/
public:
    class StateObserver;

    /* Constants from environmental variables */
    static const bool COMPUTE_MAX_DEPTH_COMPLEXITY;
    static const bool PROFILE_DEPTH_PARTITION;
    struct DebugPartition
    {
        DebugPartition();
        bool debugPixel() const;
        bool findBadPixel() const;
        bool debugAlphaAccumulation() const;
        int column;
        int row;
        std::pair<int, int> badPixelFeatures; /* slice, size */
    } static const DEBUG_PARTITION;           // Name class with macro

    /*--- Public member functions ---*/
public:
    static Context &getContext(const osg::State &state,
                               const Parameters &options);

private:
    /*--- Private member attributes ---*/
    typedef boost::shared_ptr<Context> ContextPtr;
    /* The void pointer is the osg::State* from the osg::GraphicsContext.
       We use a void pointer to avoid the temptation of using the osg::State*,
       for example when the osg::State signals that it's to be deleted
       (the signal comes from ~Referenced, so the osg::State is already
       destroyed and we shouldn't use it) */
    typedef std::map<const void *, ContextPtr> ContextMap;
    static ContextMap s_contextMap;
};
}
}
}
#endif
