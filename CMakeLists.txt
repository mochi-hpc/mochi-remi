cmake_minimum_required (VERSION 3.12)
project (remi C CXX)
enable_testing ()

option (ENABLE_TESTS    "Build tests. May require CppUnit_ROOT" OFF)
option (ENABLE_EXAMPLES "Build examples" OFF)
option (ENABLE_BEDROCK  "Build Bedrock module" ON)
option (ENABLE_COVERAGE "Enable coverage reporting" OFF)

add_library (coverage_config INTERFACE)

set (BUILD_SHARED_LIBS ON)

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

# packages we depend on
find_package (thallium REQUIRED)
find_package (PkgConfig REQUIRED)
pkg_check_modules (abt-io REQUIRED IMPORTED_TARGET abt-io)
pkg_check_modules (margo REQUIRED IMPORTED_TARGET margo)
pkg_check_modules (uuid  REQUIRED IMPORTED_TARGET uuid)
if (${ENABLE_BEDROCK})
  find_package (bedrock REQUIRED)
endif ()

if (ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options (coverage_config INTERFACE
      -O0        # no optimization
      -g         # generate debug info
      --coverage # sets all required flags
  )
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
    target_link_options (coverage_config INTERFACE --coverage)
  else ()
    target_link_libraries (coverage_config INTERFACE --coverage)
  endif ()
endif ()

add_subdirectory (src)
if (${ENABLE_TESTS})
  add_subdirectory (test)
endif (${ENABLE_TESTS})
if (${ENABLE_EXAMPLES})
  add_subdirectory (examples)
endif (${ENABLE_EXAMPLES})
