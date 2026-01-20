# NASM/YASM setup for x86_64 assembly

if(NOT VPX_ARCH_X86_64)
    return()
endif()

# Check for bundled NASM first (Windows)
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/nasm-3.01/nasm.exe")
    set(NASM_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/tools/nasm-3.01/nasm.exe")
    set(CMAKE_ASM_NASM_COMPILER "${NASM_EXECUTABLE}")
    message(STATUS "Using bundled NASM: ${NASM_EXECUTABLE}")
else()
    # Try to find system NASM
    find_program(NASM_EXECUTABLE nasm)
endif()

if(NOT NASM_EXECUTABLE)
    message(WARNING "NASM not found - building without SIMD optimizations")
    set(VPX_NO_ASM TRUE)
    return()
endif()

enable_language(ASM_NASM)

# Output format
if(WIN32)
    set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
elseif(APPLE)
    set(CMAKE_ASM_NASM_OBJECT_FORMAT macho64)
else()
    set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
endif()

# Include path for vpx_config.asm
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/config/${VPX_PLATFORM_DIR}/")
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/libvpx/")

message(STATUS "NASM format: ${CMAKE_ASM_NASM_OBJECT_FORMAT}")
