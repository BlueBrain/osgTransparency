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

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <osg/GL>

#include "OcclusionQueryGroup.h"
#include "util/extensions.h"
#include "util/glerrors.h"
#include "util/trace.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>
#include <osg/Drawable>
#include <osg/State>

#include <boost/circular_buffer.hpp>

#include <cassert>
#include <deque>
#include <iostream>
#include <limits>

namespace bbp
{
namespace osgTransparency
{
namespace
{
/** Maximum number of passes before queries may be forced to be resolved */
const unsigned int MAX_QUERY_LATENCY = 5;

/*
  Helper classes
*/
class QueryListVisitor
{
public:
    enum Result
    {
        STOP,
        RESOLVED,
        PENDING
    };
    virtual ~QueryListVisitor(){};

    virtual Result processQuery(unsigned int objIndex, GLuint queryId,
                                DrawExtensions* ext) = 0;
};
}

class OcclusionQueryGroup::PerContextQueries
{
public:
    /*--- Public constructors ---*/

    PerContextQueries()
        : _started(false)
        , _currentQueryUseAny(false)
        /* We'd prefer to use RAII for the extensions pointer, but
           std::map requires this class to be default constructible. */
        , _extensions(0)
    {
    }

    /*--- Public member functions ---*/

    void initContext(unsigned int contextID)
    {
        _extensions = getDrawExtensions(contextID);
    }

    GLuint beginQuery(unsigned int index, bool useAny = false)
    {
        checkGLErrors("Before beginQuery");
        if (_started)
        {
            std::cerr << "Trying to start an overlapping occlusion query. "
                         "Ignoring"
                      << std::endl;
            return 0;
        }

        if (_available.empty())
        {
            GLuint query;
            _extensions->glGenQueries(1, &query);
            _available.push_back(query);
        }
        GLuint query = _available.front();

#ifdef GL_ARB_occlusion_query2
        if (useAny)
            _extensions->glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
#endif
        _extensions->glBeginQuery(GL_SAMPLES_PASSED_ARB, query);

        /* Moving the available query to the pending list. */
        _pending.push_back(PendingQuery(query, index));
        _available.pop_front();
        _currentQueryUseAny = useAny;
        _started = true;

        checkGLErrors("After beginQuery");

        return query;
    }

    void endQuery()
    {
        assert(_started);
        _started = false;

        checkGLErrors("Before endQuery");
#ifdef GL_ARB_occlusion_query2
        if (_currentQueryUseAny)
            _extensions->glEndQuery(GL_ANY_SAMPLES_PASSED);
        else
#endif
            _extensions->glEndQuery(GL_SAMPLES_PASSED_ARB);
        checkGLErrors("After endQuery");
    }

    void clearPending()
    {
        while (!_pending.empty())
        {
            _available.push_back(_pending.front().id);
            _pending.pop_front();
        }
    }

    void checkQueries(QueryListVisitor& visitor)
    {
        OSGTRANSPARENCY_TRACE_FUNCTION();

        QueryList::iterator q = _pending.begin();
        while (q != _pending.end())
        {
            switch (visitor.processQuery(q->index, q->id, _extensions))
            {
            case QueryListVisitor::STOP:
                return;
            case QueryListVisitor::RESOLVED:
                _available.push_back(q->id);
                q = _pending.erase(q);
                break;
            case QueryListVisitor::PENDING:
                ++q;
                break;
            }
        }
    }

    void releaseGLObjects()
    {
        clearPending();
        std::vector<GLuint> ids;
        ids.resize(_available.size());
        std::copy(_available.begin(), _available.end(), ids.begin());
        _extensions->glDeleteQueries(ids.size(), ids.data());
        _available.clear();
    }

private:
    /*--- Private member attributes ---*/
    struct PendingQuery
    {
        PendingQuery(GLuint id_, unsigned int index_)
            : id(id_)
            , index(index_)
        {
        }
        GLuint id;
        unsigned int index;
    };
    std::deque<GLuint> _available;
    typedef std::deque<PendingQuery> QueryList;
    QueryList _pending;

    bool _started;
    bool _currentQueryUseAny;
    DrawExtensions* _extensions;
};

namespace
{
/**
   This class is used to share the same queries for all the
   OcclusionQueryGroups that have been created inside the same context.
*/
class ContextQueriesMap : private osg::Observer
{
public:
    /*--- Public member functions ---*/

    OcclusionQueryGroup::PerContextQueries& get(const osg::State* state)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        PerContextQueryMap::iterator i = _perContextQueryMap.find(state);
        if (i != _perContextQueryMap.end())
            return i->second;
        state->addObserver(this);
        OcclusionQueryGroup::PerContextQueries& contextQueries =
            _perContextQueryMap[state];
        contextQueries.initContext(state->getContextID());
        return contextQueries;
    }

    void objectDeleted(void* object) final
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        _perContextQueryMap[object].releaseGLObjects();
        /* The PerContextQueryMap object won't be deleted from the map
           because there may be OcclusionQueryGroup::Impl objects holding
           a reference to it. */
    }

private:
    /*--- Private member attributes ---*/
    OpenThreads::Mutex _mutex;

    /** As in the ContextMap, we use a void* instead of the osg::State*
        referred to ensure that the state object is never used through the
        map. */
    typedef std::map<const void*, OcclusionQueryGroup::PerContextQueries>
        PerContextQueryMap;

    PerContextQueryMap _perContextQueryMap;
};

ContextQueriesMap s_contextQueriesMap;
}

class OcclusionQueryGroup::Impl : public QueryListVisitor
{
public:
    /*--- Public declarations ---*/
    struct Query
    {
        unsigned int id;
        unsigned int pass;
    };

    /* Using a boost circular buffer implies that every pass memory is
       allocated and deallocated to store the query info. */
    typedef boost::circular_buffer<Query> QueryBuffer;

    /*
      Per object query list.
    */
    struct Queries
    {
        Queries()
            : pending(MAX_QUERY_LATENCY)
            , valid(false)
            , samples(0)
        {
        }

        /* Pending queries on an object. Each query belongs to a different
            rendering pass. */
        QueryBuffer pending;
        /* True if samples contains a valid value. */
        bool valid;
        unsigned int samples;
    };

    struct PassInfo
    {
        PassInfo(unsigned int pass)
            : number(pass)
            , queriesIssued(0)
            , queriesResolved(0)
            , samples(0)
        {
        }

        unsigned int number;
        unsigned int queriesIssued;
        unsigned int queriesResolved;
        unsigned int samples;
    };

    /*--- Public member attributes ---*/

    PerContextQueries& _contextQueries;

    /* Per object queries.
       This declaration assumes that OcclusionQueryGroup::beginQuery is going
       to be called with consecutive index number for each query inside a
       pass. Otherwise checkQueries might end up being slower than
       needed. */
    std::vector<Queries> _queryStatus;

    /* Oldest pass is at the front.
       This should be a circular buffer with as many elements as the maximum
       query latency, however this shouldn't be a performance problem. */
    std::deque<PassInfo> _passStatus;
    unsigned int _pass;

#ifdef OSG_GL3_AVAILABLE
    bool _conditionalActive;
#endif

    /*--- Public constructors/destructor ---*/

    Impl(const osg::RenderInfo& renderInfo)
        : _contextQueries(s_contextQueriesMap.get(renderInfo.getState()))
        , _pass(0)
#ifdef OSG_GL3_AVAILABLE
        , _conditionalActive(false)
#endif
        , _latency(0)
        , _stopVisiting(false)
    {
    }

    /*--- Public member functions ---*/

    void checkQueries(unsigned int latency)
    {
        _stopVisiting = false;
        _latency = latency;
        _contextQueries.checkQueries(*this);
    }

    Result processQuery(unsigned int objIndex, GLuint queryId,
                        DrawExtensions* ext)
    {
        if (_stopVisiting)
            return STOP;

        assert(_queryStatus[objIndex].pending.size() != 0);
        Queries& queries = _queryStatus[objIndex];
        const Query& query = queries.pending.front();
        /* If queries are resolved in strict order queryId must match the
           query at the front of the query list of the object. */
        assert(query.id == queryId);

        /* If the allowed latency is exceeded, the query will be resolved
           unconditionally. */
        GLuint getSamples = true;
        if (query.pass + _latency > _pass)
            ext->glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_AVAILABLE_ARB,
                                     &getSamples);
        if (getSamples)
        {
            GLuint samples;
            ext->glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_ARB, &samples);

            const unsigned int pass = query.pass;
            queries.pending.pop_front();
            queries.valid = true;
            queries.samples = samples;

            for (std::deque<PassInfo>::iterator p = _passStatus.begin();
                 p != _passStatus.end(); ++p)
            {
                if (p->number == pass)
                {
                    ++p->queriesResolved;
                    p->samples += samples;
                }
            }

            return RESOLVED;
        }
        _stopVisiting = true;
        return PENDING;
    }

private:
    unsigned int _latency;
    bool _stopVisiting;
};

/*
  Member functions
*/
OcclusionQueryGroup::OcclusionQueryGroup(osg::RenderInfo& renderInfo)
    : _impl(new Impl(renderInfo))
{
}

OcclusionQueryGroup::~OcclusionQueryGroup()
{
    delete _impl;
    _impl = 0;
}

void OcclusionQueryGroup::reset()
{
    _impl->_pass = 0;
#ifdef OSG_GL3_AVAILABLE
    _impl->_conditionalActive = false;
#endif
    /* Note this doesn't deallocate the vector. */
    _impl->_queryStatus.clear();
    _impl->_passStatus.clear();
    _impl->_pass = 0;

    /* Ignoring all pending queries since they aren't needed */
    _impl->_contextQueries.clearPending();
}

void OcclusionQueryGroup::beginPass()
{
    ++_impl->_pass;
    _impl->_passStatus.push_back(Impl::PassInfo(_impl->_pass));
}

void OcclusionQueryGroup::beginQuery(const unsigned int index)
{
    PerContextQueries& queries = _impl->_contextQueries;
    if (_impl->_queryStatus.size() <= index)
        _impl->_queryStatus.resize(index + 1);

    Impl::Queries& objectQueries = _impl->_queryStatus[index];

#ifdef OSG_GL3_AVAILABLE
    if (objectQueries.pending.size() != 0)
    {
        /* Conditional rendering is started using the last query issued for
           this object as dependency. */
        assert(!_impl->_conditionalActive);
        assert(objectQueries.pending.back().pass == _impl->_pass - 1);
        _impl->_conditionalActive = true;
        glBeginConditionalRender(objectQueries.pending.back().id,
                                 GL_QUERY_NO_WAIT);
    }
#endif

    Impl::Query query;
    query.id = queries.beginQuery(index);
    query.pass = _impl->_pass;
    objectQueries.pending.push_back(query);

    ++_impl->_passStatus.back().queriesIssued;
}

void OcclusionQueryGroup::endQuery()
{
#ifdef OSG_GL3_AVAILABLE
    if (_impl->_conditionalActive)
    {
        _impl->_conditionalActive = false;
        glEndConditionalRender();
    }
#endif
    _impl->_contextQueries.endQuery();
}

bool OcclusionQueryGroup::checkQueries(unsigned int& pass,
                                       unsigned int& samples,
                                       const unsigned int latency)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    _impl->checkQueries(std::min(latency, MAX_QUERY_LATENCY));

    bool valid = false;
    while (!_impl->_passStatus.empty())
    {
        const Impl::PassInfo& info = _impl->_passStatus.front();
        if (info.queriesIssued == info.queriesResolved)
        {
            valid = true;
            samples = info.samples;
            pass = info.number;
            _impl->_passStatus.pop_front();
        }
        else
            break;
    }
    return valid;
}

bool OcclusionQueryGroup::lastestSamplesPassed(const unsigned int index,
                                               unsigned int& samples)
{
    /* Check available queries? */

    if (_impl->_queryStatus.size() <= index)
        return false;
    Impl::Queries& objectQueries = _impl->_queryStatus[index];
    if (objectQueries.valid)
        samples = objectQueries.samples;

    return objectQueries.valid;
}
}
}
