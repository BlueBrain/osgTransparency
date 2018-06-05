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
// Contributing Authors:
//
// $URL$
// $Revision$
//
// last changed: $Date$
// by $Author$
//////////////////////////////////////////////////////////////////////

#ifndef OSGTRANSPARENCY_UTIL_TRACE_H
#define OSGTRANSPARENCY_UTIL_TRACE_H

#if defined OSGTRANSPARENCY_USE_EXTRAE

#include <boost/current_function.hpp>
#include <extrae_user_events.h>

#define OSGTRANSPARENCY_TRACE_FUNCTION()            \
    Extrae_user_function(1);                        \
    struct EndFunction                              \
    {                                               \
        EndFunction() {}                            \
        ~EndFunction() { Extrae_user_function(0); } \
    } token__
/* The empty contructor works around gcc warning token__ not being used */

namespace bbp
{
namespace osgTransparency
{
namespace detail
{
struct FunctionTraceID
{
    static const extrae_type_t extraeType = 50000000;

    FunctionTraceID(const char *function);

    operator int() const;

    static void dumpSymbols();

private:
    int _id;
};
}
}
}

#define OSGTRANSPARENCY_TRACE_FUNCTION()                                       \
    {                                                                          \
        static osgTransparency::detail::FunctionTraceID id(                    \
            BOOST_CURRENT_FUNCTION);                                           \
        Extrae_event(osgTransparency::detail::FunctionTraceID::extraeType,     \
                     id);                                                      \
    }                                                                          \
    struct EndFunction                                                         \
    {                                                                          \
        EndFunction() {}                                                       \
        ~EndFunction()                                                         \
        {                                                                      \
            Extrae_event(osgTransparency::detail::FunctionTraceID::extraeType, \
                         0);                                                   \
        }                                                                      \
    } token__

#else

#define OSGTRANSPARENCY_TRACE_FUNCTION()
#define OSGTRANSPARENCY_TRACE_FUNCTION2()

#endif

#endif
