##
## OSG Transparency
##
## Copyright (c) 2012-2016 Cajal Blue Brain, BBP/EPFL
## All rights reserved. Do not distribute without permission.
##
## Responsible Author: Juan Hernando Vieites (JHV)
## contact: jhernando@fi.upm.es

set(OSGTRANSPARENCY_PUBLIC_HEADERS
  BaseRenderBin.h
  BaseParameters.h
  DepthPeelingBin.h
  FragmentListOITBin.h
  MultiLayerDepthPeelingBin.h
  MultiLayerParameters.h
  types.h
)

set(OSGTRANSPARENCY_HEADERS
  OcclusionQueryGroup.h
  multilayer/Canvas.h
  multilayer/Context.h
  multilayer/DepthPartitioner.h
  multilayer/DepthPeelingBin.h
  multilayer/GL3IterativeDepthPartitioner.h
  multilayer/IterativeDepthPartitioner.h
  util/Stats.h
  util/ShapeData.h
  util/TextureDebugger.h
  util/constants.h
  util/extensions.h
  util/glerrors.h
  util/helpers.h
  util/loaders.h
  util/paths.in.h
  util/strings_array.h
  util/trace.h
)

set(OSGTRANSPARENCY_SOURCES
  ${OSGTRANSPARENCY_HEADERS}
  BaseParameters.cpp
  BaseRenderBin.cpp
  DepthPeelingBin.cpp
  FragmentListOITBin.cpp
  OcclusionQueryGroup.cpp
  MultiLayerDepthPeelingBin.cpp
  multilayer/DepthPartitioner.cpp
  multilayer/DepthPeelingBin.cpp
  multilayer/Canvas.cpp
  multilayer/Context.cpp
  multilayer/Parameters.cpp
  util/GPUTimer.cpp
  util/TextureDebugger.cpp
  util/constants.cpp
  util/glerrors.cpp
  util/helpers.cpp
  util/loaders.cpp
  util/trace.cpp
)

configure_file(util/paths.in.h
  ${PROJECT_BINARY_DIR}/lib/osgTransparency/util/paths.h)
list(APPEND OSGTRANSPARENCY_SOURCES
  ${PROJECT_BINARY_DIR}/lib/osgTransparency/util/paths.h)

if(OSG_GL3_AVAILABLE)
  list(APPEND OSGTRANSPARENCY_SOURCES
    multilayer/GL3IterativeDepthPartitioner.cpp
    TextureBuffer.cpp)

  list(APPEND OSGTRANSPARENCY_PUBLIC_HEADERS
    TextureBuffer.h)

else()
  list(APPEND OSGTRANSPARENCY_SOURCES
    multilayer/IterativeDepthPartitioner.cpp)
endif()
