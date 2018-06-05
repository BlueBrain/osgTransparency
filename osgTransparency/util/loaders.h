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

#ifndef OSGTRANSPARENCY_UTIL_LOADERS_H
#define OSGTRANSPARENCY_UTIL_LOADERS_H

#include <osg/Drawable>

#include <boost/filesystem/fstream.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

namespace bbp
{
namespace osgTransparency
{
/**
   Creates a GLSL program using the shader sources indicated.
   If the sourceFiles vector is emtpy, then returns an empty program.
 */
osg::Program *loadProgram(
    const std::vector<std::string> &sourceFiles,
    const std::map<std::string, std::string> &vars =
        std::map<std::string, std::string>(),
    const std::vector<boost::filesystem::path> &extraPaths =
        std::vector<boost::filesystem::path>());

/**
   Adds shader sources to a GLSL program using the file names indicated.
 */
void addShaders(osg::Program *program,
                const std::vector<std::string> &sourceFiles,
                const std::map<std::string, std::string> &vars =
                    std::map<std::string, std::string>(),
                const std::vector<boost::filesystem::path> &extraPaths =
                    std::vector<boost::filesystem::path>());

/**
   Loads a shader source file and replaces $varname words with the
   contents of the table inside the cell with the same name (but no dollar
   symbol).
   If any error is detected an empty string is returned.
*/
std::string readSourceAndReplaceVariables(
    const boost::filesystem::path &shaderFile,
    const std::map<std::string, std::string> &vars,
    const std::vector<boost::filesystem::path> &extraPaths =
        std::vector<boost::filesystem::path>());
}
}
#endif
