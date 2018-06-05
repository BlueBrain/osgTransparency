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

#ifndef OSGTRANSPARENCY_GPUTIMER_H
#define OSGTRANSPARENCY_GPUTIMER_H

#include "Stats.h"

#include <osg/Drawable>
#include <osg/Timer>
#include <osg/Version>

#include <iostream>
#include <vector>

namespace bbp
{
namespace osgTransparency
{
/** A timer using OpenGL timer queries.

    The timer is associated to a graphics context at construction and can only
    be used when that context is current.

    The stats gathered by the timer are printed when either the context is
    destroyed or the report() function is called.
*/
class GPUTimer
{
    /*--- Public constructors/destructor */
public:
    /** Create a GPU timer associated with a given OpenGL state.

        For proper cleanup, the timer addds itself to the list of observers of
        the state object and keeps a reference to the state to remove itself
        from the observer list at destruction. */
    GPUTimer(osg::State* state);

    ~GPUTimer();

    /*--- Public member functions ---*/

    /** Start a new time query.

        If this function is called again without calling stop first the second
        call will have no effect.
        The information provided as parameters will be copied over to the
        result once the query is completed.

        @param name The name that will identify this timer query.
        @param frameNumber The frame in which the timer is started.
    */
    void start(const std::string& name, const unsigned int frameNumber);

    /** End the current timer query.

        Calling stop without having called start has undefined behaviour.
    */
    void stop();

    /** Check if timer queries are completed an retreive results.

        @param waitForCompletion Wait for all pending queries to complete.
    */
    bool checkQueries(const bool waitForCompletion = false);

    struct Result
    {
        std::string name;
        unsigned int frame;
        float milliseconds;
    };
    typedef std::vector<Result> Results;

    /** Return the list of timer queries completed in last call to
        checkQueries.
    */
    const Results& getCompleted() const;

    /** Print to the given stream the list of timer queries completed in
        last call to checkQueries
    */
    void reportCompleted(std::ostream& out) const;

    /** Return the minimum frame number of all the queries which have not been
        resolved yet.
    */
    bool pendingQueriesMinimumFrame(unsigned int& frame) const;

private:
    class Impl;
    Impl* _impl;
};
}
}
#endif
