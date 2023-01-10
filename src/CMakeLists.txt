# list of source files
set (remi-src remi-server.cpp remi-client.cpp remi-fileset.cpp)

# load package helper for generating cmake CONFIG packages
include (CMakePackageConfigHelpers)

# where to install files for "find_package"
set (remi-pkg "share/cmake/remi")

#
# library version set here (e.g. for shared libs).
#
set (REMI_VERSION_MAJOR 0)
set (REMI_VERSION_MINOR 4)
set (REMI_VERSION_PATCH 0)
set (remi-vers "${REMI_VERSION_MAJOR}.${REMI_VERSION_MINOR}")
set (REMI_VERSION "${remi-vers}.${REMI_VERSION_PATCH}")

add_library (remi ${remi-src})
target_link_libraries (remi
    PUBLIC thallium PkgConfig::MARGO PkgConfig::ABTIO PkgConfig::UUID
    PRIVATE coverage_config)
target_include_directories (remi PUBLIC $<INSTALL_INTERFACE:include>)

# local include's BEFORE, in case old incompatable .h files in prefix/include
target_include_directories (remi BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

# for shared libs, establish the lib version
set_target_properties (remi
    PROPERTIES VERSION ${REMI_VERSION}
    SOVERSION ${REMI_VERSION_MAJOR})


if (${ENABLE_BEDROCK})

  add_library (remi-bedrock remi-bedrock.c)
  target_link_libraries (remi-bedrock remi)
  target_include_directories (remi-bedrock PUBLIC $<INSTALL_INTERFACE:include>)
  target_include_directories (remi-bedrock BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

  set_target_properties (remi-bedrock
    PROPERTIES VERSION ${REMI_VERSION}
    SOVERSION ${REMI_VERSION_MAJOR})

endif ()


#
# installation stuff (packaging and install commands)
#
write_basic_package_version_file (
    "remi-config-version.cmake"
    VERSION ${REMI_VERSION}
    COMPATIBILITY AnyNewerVersion)

# generate our config file for find_package()
configure_file (remi-config.cmake.in remi-config.cmake @ONLY)

# some bits for the pkg-config file
set (DEST_DIR "${CMAKE_INSTALL_PREFIX}")
set (PRIVATE_LIBS "-lremi")
configure_file ("remi.pc.in" "remi.pc" @ONLY)

#
# "make install" rules
#
install (TARGETS remi EXPORT remi-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
if (${ENABLE_BEDROCK})
install (TARGETS remi-bedrock EXPORT remi-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
endif ()
install (EXPORT remi-targets
         DESTINATION ${remi-pkg}
         FILE "remi-targets.cmake")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/remi-config.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/remi-config-version.cmake"
         DESTINATION ${remi-pkg} )
install (DIRECTORY ../include/remi
         DESTINATION include
         FILES_MATCHING PATTERN "*.h")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/remi.pc"
         DESTINATION "lib/pkgconfig/")
