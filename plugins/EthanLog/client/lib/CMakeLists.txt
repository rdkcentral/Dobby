
add_library( ethanlog
        SHARED
        ethanlog.c
        )

get_filename_component(
        ETHANLOG_HEADER_PATH
        ethanlog.h
        REALPATH
)

set_target_properties( ethanlog
        PROPERTIES
        PUBLIC_HEADER ${ETHANLOG_HEADER_PATH}
        VERSION       ${DOBBY_MAJOR_VERSION}.1.0
        SOVERSION     ${DOBBY_MAJOR_VERSION}

        )

# Install the library and headers in the usual places
include( GNUInstallDirs )
install( TARGETS ethanlog
         EXPORT ethanlog-export
         LIBRARY DESTINATION lib
         PUBLIC_HEADER DESTINATION include
        )

# Install the export targets for others to import
install( EXPORT ethanlog-export
         FILE ethanlog-targets.cmake
         NAMESPACE ethanlog::
         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
         ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config-version.cmake
         VERSION ${PROJECT_VERSION}
         COMPATIBILITY AnyNewerVersion
        )

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/ethanlog-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config.cmake
         INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

# Install the config, configversion and custom find modules
install( FILES       ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config.cmake
                     ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config-version.cmake
         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

