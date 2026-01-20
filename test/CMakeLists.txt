cmake_minimum_required(VERSION 3.21)
project(ivf_format_test C)

# Standalone IVF format test - no libvpx dependency
add_executable(ivf_format_test ivf_format_test.c)

# Set C standard
set_property(TARGET ivf_format_test PROPERTY C_STANDARD 99)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(ivf_format_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()