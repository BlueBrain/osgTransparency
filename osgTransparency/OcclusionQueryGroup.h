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

#ifndef OSGTRANSPARENCY_OCCLUSIONQUERYGROUP_H
#define OSGTRANSPARENCY_OCCLUSIONQUERYGROUP_H

#include <osg/Referenced>

#include <memory>

namespace osg
{
class RenderInfo;
}

namespace bbp
{
namespace osgTransparency
{
class OcclusionQueryGroup : public osg::Referenced
{
public:
    /*--- Public declarations ---*/

    /** @internal */
    class PerContextQueries;

    /*--- Public constructors/destructor ---*/

    /**
       Must be created inside a valid OpenGL context.
     */
    OcclusionQueryGroup(osg::RenderInfo& renderInfo);

    ~OcclusionQueryGroup();

    /*--- Public member functions ----*/

    void reset();

    /**
       Increments the pass number.
     */
    void beginPass();

    void beginQuery(unsigned int index);

    void endQuery();

    /** @param pass Maximum pass with all queries completed since
        last reset.
        @param samples Total number of samples for the last completed
        pass for each query index.
        @param latency Maximum difference allowed between the current pass
        and the pass of pending queries.
        @return true when pass and samples are written valid information
        and false when they are left unmodified. */
    bool checkQueries(unsigned int& pass, unsigned int& samples,
                      unsigned int latency = 1);

    bool lastestSamplesPassed(unsigned int index, unsigned int& samples);

private:
    /*--- Private member attributes ---*/
    class Impl;
    Impl* _impl;
};
}
}
#endif
