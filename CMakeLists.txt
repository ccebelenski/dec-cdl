cmake_minimum_required(VERSION 3.20)
project(cdl
    VERSION 0.2.0
    DESCRIPTION "DEC DCL command parsing utilities for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(CDL_BUILD_TESTS "Build tests" ON)
option(CDL_BUILD_EXAMPLES "Build examples" ON)

# Library
add_library(cdl
    src/cli_tables.cpp
    src/cli_parse.cpp
    src/cli_present.cpp
    src/cli_get_value.cpp
    src/cli_dispatch.cpp
    src/cli_util.cpp
)

add_library(cdl::cdl ALIAS cdl)

target_compile_options(cdl PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(cdl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Tests
if(CDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(CDL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS cdl EXPORT cdlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT cdlTargets
    NAMESPACE cdl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cdl)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cdlConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdlConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cdlConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cdl)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cdlConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cdlConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cdl)

install(DIRECTORY include/cdl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# CPack
set(CPACK_PACKAGE_NAME "cdl")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "Chris Cebelenski")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)
