cmake_minimum_required(VERSION 3.21)
project(vpx VERSION 1.15.1 LANGUAGES C CXX)

# Options
option(BUILD_SHARED_LIBS "Build shared library" OFF)

# Platform detection
include(cmake/VpxConfig.cmake)

# Source lists
if(VPX_ARCH_ARM64)
    include(cmake/VpxSourcesArm.cmake)
else()
    include(cmake/VpxSources.cmake)
    include(cmake/VpxAssembly.cmake)
endif()

# Library target
add_library(vpx)

if(VPX_ARCH_ARM64)
    target_sources(vpx PRIVATE ${VPX_C_SOURCES_ARM})
else()
    target_sources(vpx PRIVATE ${VPX_C_SOURCES} ${VPX_X86_ASM_SOURCES})
endif()

# Include directories
target_include_directories(vpx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/config/${VPX_PLATFORM_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libvpx>
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(vpx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(vpx PRIVATE Threads::Threads)
endif()

# On macOS, we might need pthread too, but it's usually implicit? 
# libvpx code checks HAVE_PTHREAD_H and uses pthread.
if(APPLE)
    # No explicit link needed usually, but good to be safe if strictly required
    # target_link_libraries(vpx PRIVATE pthread)
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS vpx
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libvpx/vpx/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vpx
    FILES_MATCHING PATTERN "*.h"
)

# Tests
if(PROJECT_IS_TOP_LEVEL)
    add_executable(version_test test/version_test.c)
    target_link_libraries(version_test PRIVATE vpx)
endif()
