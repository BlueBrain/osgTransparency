# Copyright (c) 2006-2018, École Polytechnique Fédérale de Lausanne (EPFL) /
#                           Blue Brain Project and
#                          Universidad Politécnica de Madrid (UPM)
#                          Juan Hernando <juan.hernando@epfl.ch>
#
# This file is part of osgTransparency
# <https://github.com/BlueBrain/osgTransparency>
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3.0 as published
# by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(osgTransparency VERSION 0.8.1)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake"
                              "${CMAKE_SOURCE_DIR}/CMake/common")
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/CMake/common/Common.cmake)
  message(FATAL_ERROR "CMake/common missing, run: git submodule update --init")
endif()

set(osgTransparency_VERSION_ABI 8)

set(OSGTRANSPARENCY_DESCRIPTION
  "OSG library for transparency rendering algorithms")
set(OSGTRANSPARENCY_MAINTAINER_NAME "Blue Brain Project")
set(OSGTRANSPARENCY_MAINTAINER_EMAIL "<bbp-open-source@googlegroups.com>")
set(OSGTRANSPARENCY_MAINTAINER_MAINTAINER "${OSGTRANSPARENCY_MAINTAINER_NAME} ${OSGTRANSPARENCY_MAINTAINER_EMAIL}")
set(OSGTRANSPARENCY_LICENSE GPLv3)
set(OSGTRANSPARENCY_INCLUDE_NAME osgTransparency)
set(OSGTRANSPARENCY_DEB_DEPENDS libopenscenegraph-dev libboost-filesystem-dev)

set(COMMON_PROJECT_DOMAIN ch.epfl.bluebrain)
include(Common)

if(NOT IS_ABSOLUTE ${CMAKE_INSTALL_PREFIX})
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX})
endif()

common_find_package(Boost 1.41.0 REQUIRED COMPONENTS filesystem system
                                                     unit_test_framework)
common_find_package(OpenGL REQUIRED)
common_find_package(OpenSceneGraph 3.0 REQUIRED COMPONENTS osgGA osgText osgUtil
                                                           osgDB osgViewer)
common_find_package(OsgGL3)
common_find_package_post()
set(OSGTRANSPARENCY_DEPENDENT_LIBRARIES Boost)

if(OSG_GL3_AVAILABLE)
  include_directories(${PROJECT_SOURCE_DIR}/include)
endif()

include_directories(${PROJECT_SOURCE_DIR}/lib/
                    ${PROJECT_BINARY_DIR}/lib/
                    ${EXTRA_INCLUDE_DIRS}
                    SYSTEM ${OPENSCENEGRAPH_INCLUDE_DIRS}
                    ${OPENTHREADS_INCLUDE_PATH}
                    ${Boost_INCLUDE_DIRS})

add_subdirectory(osgTransparency)
add_subdirectory(examples)
add_subdirectory(tests)

include(CPackConfig)

set(DOXYGEN_MAINPAGE_MD README.md)
set(DOXYGEN_EXTRA_INPUT ${PROJECT_SOURCE_DIR}/README.md)
set(DOXYGEN_PROJECT_NAME "OSG Transparency")
include(DoxygenRule)
