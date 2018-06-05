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
#
#==================================
#
# - Find Extrae
# This module searches for the Extrae library
#
#==================================
#
# The following environment variables are respected for finding Extrae.
# CMAKE_PREFIX_PATH can also be used for this (see find_library() CMake
# documentation).
#
#    EXTRAE_ROOT
#
# This module defines the following output variables:
#
#    EXTRAE_FOUND - Was Extrae and all of the specified components found?
#
#    EXTRAE_VERSION - The version of Extrae which was found
#
#    EXTRAE_INCLUDE_DIRS - Where to find the headers
#
#    EXTRAE_LIBRARIES - The Extrae libraries
#
#==================================
# Example Usage:
#
#  find_package(Extrae 0.3.0 REQUIRED)
#  include_directories(${EXTRAE_INCLUDE_DIRS})
#
#  add_executable(foo foo.cc)
#  target_link_libraries(foo ${EXTRAE_LIBRARIES})
#
#==================================
# Naming convention:
#  Local variables of the form _Extrae_foo
#  Input variables of the form Extrae_FOO
#  Output variables of the form EXTRAE_FOO
#

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/oss)
include(FindPackageHandleStandardArgs)

# find and parse extra_version.h
if(NOT _extrae_INCLUDE_DIR OR
   NOT EXISTS "${_extrae_INCLUDE_DIR}/extrae_version.h")
  find_path(_extrae_INCLUDE_DIR extrae_version.h
    HINTS ${CMAKE_SOURCE_DIR}/../../.. $ENV{EXTRAE_ROOT} ${EXTRAE_ROOT}
    PATH_SUFFIXES include
    PATHS /usr /usr/local /opt/local /opt)
endif()

if(_extrae_INCLUDE_DIR)
  set(_extrae_Version_file "${_extrae_INCLUDE_DIR}/extrae_version.h")
endif()

if(_extrae_Version_file)
  file(READ "${_extrae_Version_file}" _extrae_Version_contents)
  string(REGEX MATCH "EXTRAE_VERSION_NUMBER\\([0-9]+,[0-9]+,[0-9]+\\)"
    _extrae_VERSION ${_extrae_Version_contents})
  string(REGEX MATCH "[0-9]+,[0-9]+,[0-9]+"
    _extrae_VERSION ${_extrae_VERSION})
  string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+)" "\\1"
    _extrae_VERSION_MAJOR ${_extrae_VERSION})
  string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+)" "\\2"
    _extrae_VERSION_MINOR ${_extrae_VERSION})
  string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+)" "\\1.\\2.\\3"
    EXTRAE_VERSION ${_extrae_VERSION})
endif()

if (NOT Extrae_FIND_COMPONENTS)
  # Defaulting to pptrace
  set(Extrae_FIND_COMPONENTS pptrace)
endif()

set(EXTRAE_LIBRARIES)
set(_extrae_component_library_vars)
foreach(_extrae_component ${Extrae_FIND_COMPONENTS})
  list(APPEND _extrae_component_library_vars ${_extrae_component}_LIBRARY)
  find_library(${_extrae_component}_LIBRARY
    NAMES ${_extrae_component}-${_extrae_VERSION_MAJOR}.${_extrae_VERSION_MINOR}
          ${_extrae_component}
    HINTS $ENV{EXTRAE_ROOT} ${EXTRAE_ROOT}
    PATHS /usr /usr/local /opt/local /opt
    PATH_SUFFIXES lib)

  if (${_extrae_component}_LIBRARY)
    list(APPEND EXTRAE_LIBRARIES ${${_extrae_component}_LIBRARY})
  endif()
endforeach()

set(EXTRAE_INCLUDE_DIRS ${_extrae_INCLUDE_DIR})

find_package_handle_standard_args(Extrae
  REQUIRED_VARS EXTRAE_LIBRARIES ${_extrae_component_library_vars}
  EXTRAE_INCLUDE_DIRS
  VERSION_VAR EXTRAE_VERSION)

if(EXTRAE_FOUND)
  message(STATUS "Found Extrae ${EXTRAE_VERSION} "
    "in ${EXTRAE_INCLUDE_DIRS}:${EXTRAE_LIBRARIES}")
endif()
