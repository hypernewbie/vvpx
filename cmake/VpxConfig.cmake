# Platform detection for libvpx

# Architecture detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(VPX_ARCH_ARM64 TRUE)
    set(VPX_ARCH_X86_64 FALSE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(VPX_ARCH_ARM64 FALSE)
    set(VPX_ARCH_X86_64 TRUE)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Platform directory selection
if(WIN32 AND VPX_ARCH_X86_64)
    set(VPX_PLATFORM_DIR "win_x64")
elseif(UNIX AND NOT APPLE AND VPX_ARCH_X86_64)
    set(VPX_PLATFORM_DIR "linux_x64")
elseif(APPLE AND VPX_ARCH_ARM64)
    set(VPX_PLATFORM_DIR "macos_arm64")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "VPX platform: ${VPX_PLATFORM_DIR}")