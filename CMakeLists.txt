#
# OSSIM_DEV_HOME needs to be defined either with -DOSSIM_DEV_HOME=/path/to/ossim/top or
# via an environment variable of the same name.

cmake_minimum_required(VERSION 2.8)
project(orthoism)

Message("################## Setting up orthoism  #########################")

SET(CMAKE_MODULE_PATH   ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )

set(CMAKE_CXX_STANDARD 14)
set(SOURCE_FILES)
set(HEADER_FILES)
set(OBJECT_LIB)
set(CMAKE_VERBOSE_MAKEFILE ON)
if (DEFINED OSSIM_DEV_HOME)
   set(ENV{OSSIM_DEV_HOME} ${OSSIM_DEV_HOME})
endif()
if( EXISTS $ENV{OSSIM_DEV_HOME} )
   SET(CMAKE_MODULE_PATH "$ENV{OSSIM_DEV_HOME}/ossim/cmake/CMakeModules")
elseif( EXISTS $ENV{OSSIM_INSTALL_PREFIX} )
   SET(CMAKE_MODULE_PATH "$ENV{OSSIM_INSTALL_PREFIX}/share/ossim/CMakeModules")
else()
   message( FATAL_ERROR "Need either CMAKE_MODULE_PATH, OSSIM_INSTALL_PREFIX or OSSIM_DEV_HOME defined!" )
endif()
message("Using CMAKE_MODULE_PATH = <${CMAKE_MODULE_PATH}> to locate OSSIM components")

include(OssimVersion)
include(OssimCommonVariables)
include(OssimUtilities)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if ( NOT DEFINED OSSIM_LIBRARIES )
   find_package(ossim)
endif()
if ( NOT DEFINED GEOTIFF_LIBRARY )
   find_package(GEOTIFF)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${OSSIM_INCLUDE_DIR})

set(PACKAGE_LIBRARIES ${OSSIM_LIBRARIES} ${GEOTIFF_LIBRARY} pthread)

add_executable (orthoism orthoism.cpp OrthoTileSource.cpp )
target_link_libraries(orthoism ${PACKAGE_LIBRARIES}  )

install(TARGETS orthoism  DESTINATION bin )
