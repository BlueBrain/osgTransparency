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

#ifndef OSGTRANSPARENCY_UTIL_HELPERS_H
#define OSGTRANSPARENCY_UTIL_HELPERS_H

/** Note, this header must be included before any other boost header,
    otherwise the max argument arity configuration macro will be redefined. */
#define BOOST_PARAMETER_MAX_ARITY 6
#include <boost/parameter/keyword.hpp>
#include <boost/parameter/name.hpp>
#include <boost/parameter/preprocessor.hpp>

#include "loaders.h"

#include "../types.h"

#include <osg/Config>
#include <osg/ShapeDrawable> // To include gl headers

namespace osg
{
class FrameBufferObject;
class TextureRectangle;
class Texture2DArray;
}

namespace bbp
{
namespace osgTransparency
{
osg::ref_ptr<osg::Geometry> createQuad();

typedef std::map<GLenum, int> Modes;
typedef std::map<osg::ref_ptr<osg::StateAttribute>, int> Attributes;
typedef std::set<osg::ref_ptr<osg::Uniform>> Uniforms;

/**
   Clones a program
*/
osg::Program* copyProgram(const osg::Program* program);

/**
   Returns a copy of current in old (state sets and programs are not copied).
   The difference between current and old is returned in updates.
   Performs in O(n) if current and old are already equivalent
*/
void updateProgramMap(const ProgramMap& current, ProgramMap& old,
                      ProgramMap& updates);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
namespace keywords
{
BOOST_PARAMETER_NAME(extra_shaders);
BOOST_PARAMETER_NAME(program_map);
BOOST_PARAMETER_NAME(state_set);
BOOST_PARAMETER_NAME(modes);
BOOST_PARAMETER_NAME(attributes);
BOOST_PARAMETER_NAME(uniforms);
BOOST_PARAMETER_NAME(filenames);
BOOST_PARAMETER_NAME(vertex_shaders);
BOOST_PARAMETER_NAME(fragment_shaders);
BOOST_PARAMETER_NAME(shader_path);
}
#pragma clang diagnostic pop

inline void setupTexture(const std::string& name, int unit,
                         osg::StateSet& stateSet, osg::Texture* texture)
{
    stateSet.setTextureAttributeAndModes(unit, texture);
    stateSet.addUniform(new osg::Uniform(name.c_str(), unit));
}

/**
   The parameter append_0 adds [0] to the uniform name.
 */
inline osg::Uniform* createTextureArrayUniform(const std::string& name,
                                               int firstUnit,
                                               unsigned int count,
                                               bool appendZero = false)
{
    osg::Uniform* uniform = new osg::Uniform();
    uniform->setNumElements(count);
    uniform->setName(name + (appendZero ? "[0]" : ""));
    uniform->setType(osg::Uniform::INT);
    for (unsigned int i = 0; i < count; ++i)
        uniform->setElement(i, firstUnit + (int)i);
    return uniform;
}

/**
   Insert two array uniforms in uniform set. One of the uniforms has [0]
   appended and the other the original name. This redundancy is a workaround
   with a problem related to uniform naming as returned by glGetActiveUniform
   that appeared in NVidia drivers 275.x and it's still active.
   More info: http://forum.openscenegraph.org/viewtopic.php?t=8581
 */
inline void insertTextureArrayUniform(Uniforms& uniforms,
                                      const std::string& name, int firstUnit,
                                      unsigned int count)
{
    uniforms.insert(createTextureArrayUniform(name, firstUnit, count));
    uniforms.insert(createTextureArrayUniform(name, firstUnit, count, true));
}

template <typename T>
inline void setupTextureArray(const std::string& name, int firstUnit,
                              osg::StateSet& stateSet, unsigned int count,
                              osg::ref_ptr<T>* textures)
{
    for (unsigned int i = 0; i < count; ++i)
        stateSet.setTextureAttributeAndModes(firstUnit + i, textures[i].get());
    osg::Uniform* uniform = createTextureArrayUniform(name, firstUnit, count);
    stateSet.addUniform(uniform);
    /* Workaround for uniform array naming in NVidia drivers >= 275 */
    uniform = createTextureArrayUniform(name, firstUnit, count, true);
    stateSet.addUniform(uniform);
}

template <typename Texture>
Texture* createTexture(int width, int height, GLuint format = GL_RGBA8,
                       GLuint sourceFormat = GL_RGBA)
{
    Texture* texture = new Texture();
    texture->setTextureSize(width, height);
    texture->setInternalFormat(format);
    texture->setSourceFormat(sourceFormat);
    return texture;
}

#ifdef OSG_GL3_AVAILABLE
void clearTexture(osg::State& state, osg::FrameBufferObject* fbo,
                  osg::TextureRectangle* texture,
                  const osg::Vec4& value = osg::Vec4(0, 0, 0, 0),
                  bool rebindPreviousFBO = true);

void clearTexture(osg::State& state, osg::FrameBufferObject* fbo,
                  osg::Texture2DArray* texture,
                  const osg::Vec4& value = osg::Vec4(0, 0, 0, 0),
                  bool rebindPreviousFBO = true);
#endif

template <typename Filenames, typename VertexShaders, typename FragmentShaders>
static void addProgramsImplementation(const ProgramMap& extraShaders,
                                      ProgramMap& programMap,
                                      const Filenames& filenames,
                                      const VertexShaders& vertexShaders,
                                      const FragmentShaders& fragmentShaders,
                                      const std::string& shaderPath)
{
    for (ProgramMap::const_iterator stateProgram = extraShaders.begin();
         stateProgram != extraShaders.end(); ++stateProgram)
    {
        osg::Program* program = copyProgram(stateProgram->second.get());

        for (typename VertexShaders::const_iterator i = vertexShaders.begin();
             i != vertexShaders.end(); ++i)
        {
            osg::Shader* shader = new osg::Shader(osg::Shader::VERTEX, *i);
            program->addShader(shader);
            if (i->substr(0, 3) != "///")
                program->setName(program->getName() +
                                 i->substr(0, i->find_first_of('\n')) + ", ");
        }

        for (typename FragmentShaders::const_iterator i =
                 fragmentShaders.begin();
             i != fragmentShaders.end(); ++i)
        {
            osg::Shader* shader = new osg::Shader(osg::Shader::FRAGMENT, *i);
            program->addShader(shader);
            if (i->substr(0, 3) != "///")
                program->setName(program->getName() +
                                 i->substr(0, i->find_first_of('\n')) + ", ");
        }

        std::vector<std::string> sources;
        for (typename Filenames::const_iterator i = filenames.begin();
             i != filenames.end(); ++i)
        {
            sources.push_back(shaderPath + *i);
            program->setName(program->getName() + *i + ", ");
        }
        addShaders(program, sources);
        programMap[stateProgram->first] = program;
    }
}

template <typename Filenames, typename VertexShaders, typename FragmentShaders>
osg::Program* addProgramImplementation(const Filenames& filenames,
                                       const VertexShaders& vertexShaders,
                                       const FragmentShaders& fragmentShaders,
                                       const std::string& shaderPath)
{
    std::vector<std::string> sources;
    for (typename Filenames::const_iterator i = filenames.begin();
         i != filenames.end(); ++i)
    {
        sources.push_back(shaderPath + *i);
    }
    osg::Program* program = loadProgram(sources);

    for (typename VertexShaders::const_iterator i = vertexShaders.begin();
         i != vertexShaders.end(); ++i)
    {
        osg::Shader* shader = new osg::Shader(osg::Shader::VERTEX, *i);
        program->addShader(shader);
    }

    for (typename FragmentShaders::const_iterator i = fragmentShaders.begin();
         i != fragmentShaders.end(); ++i)
    {
        osg::Shader* shader = new osg::Shader(osg::Shader::FRAGMENT, *i);
        program->addShader(shader);
    }

    return program;
}

#if BOOST_VERSION <= 104100
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#endif
/* In gcc < 4.6 there's no push/pop and it seems that the diagnostic type
   cannot be changed multiple times in the same file */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
BOOST_PARAMETER_FUNCTION(
    (void), addPrograms, keywords::tag,
    /* The osg::StateSet object */
    (required(extra_shaders, (ProgramMap))(in_out(program_map), (ProgramMap*)))(
        optional /* A container with a list of filename, the requirement on
                    the actual parameter is that it must provide a standard
                    input iterator. */
        (filenames, *, std::vector<std::string>())
        /* A container with a list of vertex shader source strings,
           the requirement on the actual parameter is that it must
           provide a standard input iterator. */
        (vertex_shaders, *, std::vector<std::string>())
        /* A container with a list of vertex fragment source strings,
           the requirement on the actual parameter is that it must
           provide a standard input iterator. */
        (fragment_shaders, *, std::vector<std::string>())
        /* The path to the shader files. */
        (shader_path, *, std::string())))
{
    addProgramsImplementation(extra_shaders, *program_map, filenames,
                              vertex_shaders, fragment_shaders, shader_path);
}

BOOST_PARAMETER_FUNCTION(
    (void), addProgram, keywords::tag,
    /* The osg::StateSet object */
    (required(in_out(state_set), (osg::StateSet*)))(
        optional /* A container with a list of filename, the requirement on
                    the actual parameter is that it must provide a standard
                    input iterator. */
        (filenames, *, std::vector<std::string>())
        /* A container with a list of vertex shader source strings,
           the requirement on the actual parameter is that it must
           provide a standard input iterator. */
        (vertex_shaders, *, std::vector<std::string>())
        /* A container with a list of vertex fragment source strings,
           the requirement on the actual parameter is that it must
           provide a standard input iterator. */
        (fragment_shaders, *, std::vector<std::string>())
        /* The path to the shader files. */
        (shader_path, *, std::string())))
{
    state_set->setAttributeAndModes(
        addProgramImplementation(filenames, vertex_shaders, fragment_shaders,
                                 shader_path));
}

BOOST_PARAMETER_FUNCTION(
    (void), setupStateSet, keywords::tag,
    /* The osg::StateSet object */
    (required(in_out(state_set), (osg::StateSet*)))(
        optional /* A map with GL modes and their on/off state. */
        (modes, (Modes), Modes())
        /* A map with osg::StateAttributes and their on/off state. */
        (attributes, (Attributes), Attributes())
        /* A set of attributes */
        (uniforms, (Uniforms), Uniforms())))
{
    for (Modes::const_iterator mode = modes.begin(); mode != modes.end();
         ++mode)
    {
        state_set->setMode(mode->first, mode->second);
    }
    for (Attributes::const_iterator attr = attributes.begin();
         attr != attributes.end(); ++attr)
    {
        state_set->setAttributeAndModes(attr->first.get(), attr->second);
    }
    for (Uniforms::const_iterator uniform = uniforms.begin();
         uniform != uniforms.end(); ++uniform)
    {
        state_set->addUniform(uniform->get());
    }
}
#if BOOST_VERSION <= 104100
#if __GNUC__ > 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif
#endif
}
}
#endif
