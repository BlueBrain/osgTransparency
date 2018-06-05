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

#include "osgTransparency/util/paths.h"
#include <osgTransparency/util/loaders.h>

#include <osg/Config>
#include <osg/Program>
#include <osg/Shader>

#include <boost/format.hpp>

namespace bbp
{
namespace osgTransparency
{
namespace test
{
namespace util
{
namespace
{
typedef std::vector<std::string> Strings;
typedef std::map<std::string, std::string> Vars;

osg::Program *_loadProgram(const Strings &sources, const Vars &vars = Vars())
{
    typedef boost::filesystem::path Path;
    typedef std::vector<Path> Paths;

    Paths paths;
#ifdef OSG_GL3_AVAILABLE
    paths.push_back(Path(SOURCE_PATH) / "tests/unit/shaders/GL3");
#else
    paths.push_back(Path(SOURCE_PATH) / "tests/unit/shaders/GL2");
#endif

    return loadProgram(sources, vars, paths);
}
}

osg::Program *createTrivialProgram(const bool withMain)
{
    Strings sources;
    sources.push_back("trivial.vert");
    sources.push_back("trivial.frag");
    if (withMain)
    {
        sources.push_back("main.vert");
        sources.push_back("main.frag");
    }
    return _loadProgram(sources);
}

osg::Program *createStretchProgram(const float left, const float right,
                                   const float bottom, const float top,
                                   const bool withMain)
{
    Strings sources;
    sources.push_back("stretch.vert");
    sources.push_back("trivial.frag");
    if (withMain)
    {
        sources.push_back("main.vert");
        sources.push_back("main.frag");
    }
    using boost::str;
    using boost::format;
    Vars vars;
    vars["DEFINES"] = str(format("#define LEFT %1%\n#define RIGHT %2%\n"
                                 "#define BOTTOM %3%\n#define TOP %4%\n") %
                          left % right % bottom % top);
    return _loadProgram(sources, vars);
}

osg::Program *createShallowMainProgram()
{
    Strings sources;
    sources.push_back("main.vert");
    sources.push_back("main.frag");
    return _loadProgram(sources);
}
}
}
}
}
