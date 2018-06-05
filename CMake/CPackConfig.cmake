##
## OSG Transparency
##
## Copyright (c) 2012-2017 Cajal Blue Brain, BBP/EPFL
## All rights reserved. Do not distribute without permission.
##
## Responsible Author: Juan Hernando Vieites (JHV)
## contact: jhernando@fi.upm.es

set(CPACK_PACKAGE_DESCRIPTION_FILE
    "${PROJECT_SOURCE_DIR}/CMake/CPackReadme.txt" )
set(CPACK_PACKAGE_NAME "libosgtransparency")
set(CPACK_PROJECT_NAME "OSG Transparency")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-filesystem-dev, libopenscenegraph-dev")

include(CommonCPack)
