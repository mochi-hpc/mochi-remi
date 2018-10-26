#
#  general cmake flags:
#    -DCMAKE_INSTALL_PREFIX=/usr/local     -- the prefix for installing
#    -DCMAKE_BUILD_TYPE=type               -- type can be Debug, Release, ...
#    -DCMAKE_PREFIX_PATH=/dir              -- external packages
#
#     note that CMAKE_PREFIX_PATH can be a list of directories:
#      -DCMAKE_PREFIX_PATH='/dir1;/dir2;/dir3'
#

cmake_minimum_required (VERSION 3.0)
project (remi C CXX)
enable_testing ()

option(ENABLE_TESTS    "Build tests. May require CppUnit_ROOT" OFF)
option(ENABLE_EXAMPLES "Build examples" OFF)

# add our cmake module directory to the path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# link shared lib with full rpath
set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# setup cache variables for ccmake
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
         CACHE STRING "Choose the type of build." FORCE)
    set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS 
                  "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif ()
set (CMAKE_PREFIX_PATH "" CACHE STRING "External dependencies path")
set (BUILD_SHARED_LIBS "OFF" CACHE BOOL "Build a shared library")

# packages we depend on
include (xpkg-import)
find_package (thallium REQUIRED)
xpkg_import_module (margo REQUIRED margo)
xpkg_import_module (abt-io REQUIRED abt-io)
xpkg_import_module (uuid REQUIRED uuid)

add_subdirectory (src)
if(${ENABLE_TESTS})
  add_subdirectory (test)
endif(${ENABLE_TESTS})
if(${ENABLE_EXAMPLES})
  add_subdirectory (examples)
endif(${ENABLE_EXAMPLES})
