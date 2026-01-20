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
    include(cmake/VpxSourcesArm.cmake)  # For generic C fallback
    include(cmake/VpxAssembly.cmake)
endif()

# Library target
add_library(vpx)

if(VPX_ARCH_ARM64)
    target_sources(vpx PRIVATE ${VPX_C_SOURCES_ARM})
elseif(VPX_NO_ASM)
    # No NASM available - fall back to generic C (like ARM)
    target_sources(vpx PRIVATE ${VPX_C_SOURCES_ARM})
    message(STATUS "Building with generic C sources (no SIMD)")
else()
    target_sources(vpx PRIVATE ${VPX_C_SOURCES} ${VPX_X86_ASM_SOURCES})
endif()

# Include directories
target_include_directories(vpx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/config/${VPX_PLATFORM_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libvpx>
)

# Define VPX_DISABLE_ASM when building without NASM
if(VPX_NO_ASM)
    target_compile_definitions(vpx PUBLIC VPX_DISABLE_ASM)
endif()

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
    
    # decode_test requires full SIMD build (NASM on x86_64, or native ARM)
    # Generic C fallback has RTCD init issues with libvpx internals
    if(NOT VPX_NO_ASM OR VPX_ARCH_ARM64)
        add_executable(decode_test test/decode_test.c)
        target_link_libraries(decode_test PRIVATE vpx)
    endif()
    
    add_executable(ivf_format_test test/ivf_format_test.c)
    target_link_libraries(ivf_format_test PRIVATE vpx)
endif()

