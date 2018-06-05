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

#ifndef OSGTRANSPARENCY_TYPES_H
#define OSGTRANSPARENCY_TYPES_H

#include <map>
namespace osg
{
class Drawable;
class Program;
class RenderInfo;
class StateSet;
class ShapeDrawable;

template <typename T>
class ref_ptr;
}

namespace osgUtil
{
class RenderLeaf;
}

namespace bbp
{
namespace osgTransparency
{
/* Interal implementation types */

/** @internal */
class ShapeData;
typedef std::map<const osg::Drawable*, ShapeData> BoundShapesStorage;

typedef osg::ref_ptr<osg::Program> ProgramPtr;
typedef std::map<const osg::StateSet*, ProgramPtr> ProgramMap;
}
}
#endif
