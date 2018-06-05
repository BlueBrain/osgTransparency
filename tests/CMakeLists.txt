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

try_run(TEST_GRAPHICS_CONTEXT_RESULT _dummy
  ${PROJECT_BINARY_DIR}/tests/test_graphics_context
  ${CMAKE_CURRENT_SOURCE_DIR}/test_graphics_context.cpp
  CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${OPENSCENEGRAPH_INCLUDE_DIRS}"
  LINK_LIBRARIES "${OPENSCENEGRAPH_LIBRARIES}")
if(TEST_GRAPHICS_CONTEXT_RESULT MATCHES FAILED_TO_RUN)
  # Since all tests require a GPU, we skip them completely
  if(NOT TARGET tests)
    add_custom_target(tests)
  endif()
  if(NOT TARGET ${PROJECT_NAME}-tests)
    add_custom_target(${PROJECT_NAME}-tests)
  endif()
  return()
endif()

set(COMMON_TEST_SOURCES
  unit/RenderContext.cpp
  unit/compare.cpp
  unit/util.cpp)
set(TEST_LIBRARIES osgTransparency
                   ${Boost_LIBRARIES}
                   ${OPENSCENEGRAPH_LIBRARIES}
                   ${OPENGL_LIBRARIES})

add_library(testCommon ${COMMON_TEST_SOURCES})
target_link_libraries(testCommon "${TEST_LIBRARIES}")
list(APPEND TEST_LIBRARIES testCommon)

set(EXCLUDE_FROM_TESTS ${COMMON_TEST_SOURCES} test_graphics_context.cpp)
include(CommonCTest)
