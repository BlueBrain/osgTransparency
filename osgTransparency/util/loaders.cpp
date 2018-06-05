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

#include "loaders.h"
#include "osgTransparency/util/paths.h"

#include <osg/Config>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <iostream>
#include <sstream>
#include <string.h>

namespace bbp
{
namespace osgTransparency
{
namespace
{
const char *_getShaderPath()
{
    char *path = ::getenv("OSGTRANSPARENCY_SHADER_PATH");
    if (path)
        return path;
    return "";
}
}

const std::string s_shaderPath(_getShaderPath());

/*
  Helper functions
*/
osg::Program *loadProgram(
    const std::vector<std::string> &sourceFiles,
    const std::map<std::string, std::string> &vars,
    const std::vector<boost::filesystem::path> &extraPaths)
{
    osg::Program *program = new osg::Program();
    addShaders(program, sourceFiles, vars, extraPaths);
    return program;
}

void addShaders(osg::Program *program,
                const std::vector<std::string> &sourceFiles,
                const std::map<std::string, std::string> &vars,
                const std::vector<boost::filesystem::path> &extraPaths)
{
    std::string programName = program->getName();

    for (std::vector<std::string>::const_iterator source = sourceFiles.begin();
         source != sourceFiles.end(); ++source)
    {
        programName += *source + ", ";

        osg::Shader *shader = new osg::Shader();
        shader->setName(*source);

        if (!strcmp(&source->c_str()[source->length() - 4], "vert"))
            shader->setType(osg::Shader::VERTEX);
        else if (!strcmp(&source->c_str()[source->length() - 4], "geom"))
            shader->setType(osg::Shader::GEOMETRY);
        else
            shader->setType(osg::Shader::FRAGMENT);

        std::string text =
            readSourceAndReplaceVariables(*source, vars, extraPaths);
        shader->setShaderSource(text);

        program->addShader(shader);
    }

    program->setName(programName);
}

std::string readSourceAndReplaceVariables(
    const boost::filesystem::path &shaderFile,
    const std::map<std::string, std::string> &vars,
    const std::vector<boost::filesystem::path> &extraPaths)
{
#ifdef OSG_GL3_AVAILABLE
    std::string shaders = "shaders/GL3";
#else
    std::string shaders = "shaders/GL2";
#endif

    typedef boost::filesystem::path Path;
    typedef std::vector<Path> Paths;
    Paths paths;
    paths.resize(extraPaths.size());
    std::copy(extraPaths.begin(), extraPaths.end(), paths.begin());
    paths.push_back(Path(INSTALL_PREFIX) / "share/osgTransparency" / shaders);
    paths.push_back(Path(SOURCE_PATH) / "osgTransparency" / shaders);
    paths.push_back(Path("./" + shaders));
    if (!s_shaderPath.empty())
        paths.push_back(s_shaderPath);

    while (!paths.empty())
    {
        const Path path(paths.back() / shaderFile);
        paths.pop_back();
        try
        {
            if (!boost::filesystem::exists(path))
                continue;

            boost::filesystem::ifstream file(path);
            if (file.fail())
                continue;

            std::stringstream out;
            /* Reading until a $ is found or EOF is reached. */
            do
            {
                int ch = file.get();
                if (!file.eof())
                {
                    if (file.fail())
                    {
                        std::cerr
                            << "Error reading shader source '" << shaderFile
                            << "' for transparent render bin" << std::endl;
                        break;
                    }
                    if (ch == '$')
                    {
                        /* Getting the variable name. */
                        std::string name;
                        char c;
                        while (!file.get(c).fail() && (isalnum(c) || c == '_'))
                            name += c;
                        if (!file.fail())
                            file.unget();
                        std::map<std::string, std::string>::const_iterator
                            entry = vars.find(name);
                        /* Outputing the variable substitution if the name is
                           found in the table. */
                        if (entry != vars.end())
                            out << entry->second;
                    }
                    else
                    {
                        out << (char)ch;
                    }
                }
            } while (!file.eof());

            return out.str();
        }
#if BOOST_FILESYSTEM_VERSION == 3
        catch (const boost::filesystem::filesystem_error &)
#else
        catch (const boost::filesystem::basic_filesystem_error<Path> &)
#endif
        {
            // ignore and try next location
        }
    }

    std::cerr << "Couldn't find shader source '" << shaderFile
              << "' in any of the default locations" << std::endl;
    return "";
}
}
}
