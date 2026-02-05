cmake_minimum_required(VERSION 3.14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(yatl
    VERSION 0.1.0
    DESCRIPTION "Yet Another TOML Library"
    LANGUAGES C
)

option(YATL_BUILD_SHARED "Build shared library instead of static" OFF)
option(YATL_BUILD_EXAMPLES "Build example programs" ON)
option(YATL_BUILD_TESTS "Build test cases" ON)
option(YATL_BUILD_FUZZERS "Build fuzzing targets" OFF)
option(YATL_ENABLE_LOGGING "Enable debug logging" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library sources
set(YATL_SOURCES
    src/yatl.c
    src/yatl_lexer.c
    src/yatl_writer.c
)

set(YATL_HEADERS
    include/yatl.h
)

# Build library
if(YATL_BUILD_SHARED)
    add_library(yatl SHARED ${YATL_SOURCES})
else()
    add_library(yatl STATIC ${YATL_SOURCES})
endif()

target_include_directories(yatl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(YATL_ENABLE_LOGGING)
    target_compile_definitions(yatl PUBLIC YATL_ENABLE_LOGGING)
endif()

set_target_properties(yatl PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${YATL_HEADERS}"
)

# Install
include(GNUInstallDirs)

install(TARGETS yatl
    EXPORT yatlTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yatl
)

install(EXPORT yatlTargets
    FILE yatlTargets.cmake
    NAMESPACE yatl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yatl
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/yatlConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/yatlConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yatl
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/yatlConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/yatlConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/yatlConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yatl
)

# Subdirectories
if(YATL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(YATL_BUILD_FUZZERS)
    add_subdirectory(fuzzers)
endif()

if(YATL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
