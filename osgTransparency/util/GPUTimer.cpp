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

#include "GPUTimer.h"

#include "extensions.h"

#include <osg/Version>

#if OSG_VERSION_GREATER_OR_EQUAL(3, 3, 3)
#include <osg/ContextData>
#endif

#include <cassert>

namespace bbp
{
namespace osgTransparency
{
#if OSG_VERSION_GREATER_OR_EQUAL(3, 3, 3)
namespace
{
class QueryObjectManager : public osg::GLObjectManager
{
public:
    QueryObjectManager(unsigned int contextID)
        : osg::GLObjectManager("osgTransparency::QueryObjectManager", contextID)
    {
    }

    virtual void deleteGLObject(GLuint handler)
    {
        const osg::GLExtensions* ext = osg::GLExtensions::Get(_contextID, true);
        ext->glDeleteQueries(1, &handler);
    }
};
}
#endif

class GPUTimer::Impl
{
public:
    Impl(osg::State* state)
        : _contextID(state->getContextID())
        , _extensions(getDrawExtensions(_contextID))
        , _started(false)
    {
    }

    ~Impl()
    {
/* The query objects are leaked in older versions of OSG because it's
   very hard to delete them safely. */
#if OSG_VERSION_GREATER_OR_EQUAL(3, 3, 3)
        _available.splice(_available.end(), _pending);
        auto objectManager = osg::get<QueryObjectManager>(_contextID);
        for (auto query : _pending)
            objectManager->scheduleGLObjectForDeletion(query.id);
#endif
    }

    void start(const std::string& name, const unsigned int frameNumber)
    {
        if (_started)
        {
            std::cerr << "Trying to start an overlapping GPU timer. Ignoring"
                      << std::endl;
            return;
        }

        _started = true;
        if (_available.empty())
        {
            /* Creating a new query object. */
            Query query;
            _extensions->glGenQueries(1, &query.id);
            _available.push_back(query);
        }
        /* Accesing the next available query object. */
        Query& query = _available.front();
        query.frame = frameNumber;
        query.name = name;
        _extensions->glBeginQuery(GL_TIME_ELAPSED, query.id);
        /* Moving the available query to the pending list. */
        _pending.splice(_pending.end(), _available, _available.begin());
    }

    void stop()
    {
        assert(_started);
        _extensions->glEndQuery(GL_TIME_ELAPSED);
        _started = false;
    }

    bool checkQueries(const bool waitForCompletion)
    {
        _completed.clear();

        QueryList::iterator q = _pending.begin();
        while (q != _pending.end())
        {
            /* Checking query availability only if waitForCompletion is
               false. */
            GLuint available = waitForCompletion;
            if (!waitForCompletion)
                _extensions->glGetQueryObjectuiv(q->id,
                                                 GL_QUERY_RESULT_AVAILABLE_ARB,
                                                 &available);
            if (!available)
                /* No more queries to consider, they won't be available. */
                break;

            /* Getting the query result.  */
            GLuint64EXT elapsed;
            _extensions->glGetQueryObjectui64v(q->id, GL_QUERY_RESULT_ARB,
                                               &elapsed);
            _completed.resize(_completed.size() + 1);
            Result& result = _completed.back();
            result.name = q->name;
            result.frame = q->frame;
            result.milliseconds = float(elapsed) * 1e-6;

            /* Moving the node pointed by q to the end of the available list.
               After this call q points to the node that was following the
               moved one (which hasn't been invalidated at all). */
            _available.splice(_available.end(), _pending, q++);
        }
        return _pending.empty();
    }

    void reportCompleted(std::ostream& out) const
    {
        for (Results::const_iterator i = _completed.begin();
             i != _completed.end(); ++i)
        {
            out << i->name << " " << i->frame << " " << i->milliseconds
                << std::endl;
        }
    }

    bool pendingQueriesMinimumFrame(unsigned int& frame) const
    {
        if (_pending.empty())
            return false;
        frame = _pending.front().frame;
        return true;
    }

    const unsigned int _contextID;
    const DrawExtensions* _extensions;

    bool _started;

    struct Query
    {
        GLuint id;
        unsigned int frame;
        std::string name;
    };
    typedef std::list<Query> QueryList;
    QueryList _available;
    QueryList _pending;

    typedef std::vector<Result> Results;
    Results _completed;
};

GPUTimer::GPUTimer(osg::State* state)
    : _impl(new Impl(state))
{
}

GPUTimer::~GPUTimer()
{
    delete _impl;
}

void GPUTimer::start(const std::string& name, const unsigned int frameNumber)
{
    _impl->start(name, frameNumber);
}

void GPUTimer::stop()
{
    _impl->stop();
}

bool GPUTimer::checkQueries(const bool waitForCompletion)
{
    return _impl->checkQueries(waitForCompletion);
}

const GPUTimer::Results& GPUTimer::getCompleted() const
{
    return _impl->_completed;
}

void GPUTimer::reportCompleted(std::ostream& out) const
{
    _impl->reportCompleted(out);
}

bool GPUTimer::pendingQueriesMinimumFrame(unsigned int& frame) const
{
    return _impl->pendingQueriesMinimumFrame(frame);
}
}
}
