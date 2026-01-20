# NASM/YASM setup for x86_64 assembly

if(NOT VPX_ARCH_X86_64)
    return()
endif()

# Try to find NASM/YASM, but make it optional
find_program(NASM_EXECUTABLE nasm)
find_program(YASM_EXECUTABLE yasm)

if(NASM_EXECUTABLE)
    set(CMAKE_ASM_NASM_COMPILER ${NASM_EXECUTABLE})
    enable_language(ASM_NASM)
elseif(YASM_EXECUTABLE)
    set(CMAKE_ASM_NASM_COMPILER ${YASM_EXECUTABLE})
    enable_language(ASM_NASM)
else()
    message(WARNING "Neither NASM nor YASM found. Building without assembly optimizations.")
    set(VPX_NO_ASM TRUE)
    return()
endif()

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