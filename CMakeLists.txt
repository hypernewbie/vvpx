cmake_minimum_required(VERSION 3.21)
project(vpx VERSION 1.15.1 LANGUAGES C CXX)

# Options
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(VPX_ENABLE_NEON "Enable ARM NEON optimisations (macOS ARM64)" ON)

# MSVC: Enable parallel compilation
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)
endif()

# Platform detection
include(cmake/VpxConfig.cmake)

# Source lists
if(VPX_ARCH_ARM64)
    include(cmake/VpxSourcesArm.cmake)
    if(VPX_ENABLE_NEON AND APPLE)
        include(cmake/VpxSourcesArmNeon.cmake)
    endif()
else()
    include(cmake/VpxSources.cmake)
    include(cmake/VpxSourcesArm.cmake)  # For generic C fallback
    include(cmake/VpxAssembly.cmake)
endif()

# Apply SIMD flags to source files (for GCC/Clang)
if(NOT MSVC AND NOT VPX_ARCH_ARM64)
    foreach(source_file ${VPX_C_SOURCES})
        get_filename_component(filename ${source_file} NAME)
        if(filename MATCHES "_mmx\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-mmmx")
        elseif(filename MATCHES "_sse2\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-msse2")
        elseif(filename MATCHES "_ssse3\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-mssse3")
        elseif(filename MATCHES "_sse4\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-msse4.1")
        elseif(filename MATCHES "_avx\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-mavx")
        elseif(filename MATCHES "_avx2\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-mavx2")
        elseif(filename MATCHES "_avx512\\.c$")
            set_source_files_properties(${source_file} PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx512cd;-mavx512bw;-mavx512dq;-mavx512vl")
        endif()
    endforeach()
endif()

# Library target
add_library(vpx)

if(VPX_ARCH_ARM64)
    if(VPX_ENABLE_NEON AND APPLE)
        target_sources(vpx PRIVATE ${VPX_C_SOURCES_ARM} ${VPX_NEON_SOURCES})
        message(STATUS "Building with ARM NEON optimisations")
    else()
        target_sources(vpx PRIVATE ${VPX_C_SOURCES_ARM})
        message(STATUS "Building with generic C (no NEON)")
    endif()
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
    target_link_libraries(vpx PRIVATE Threads::Threads m)
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
