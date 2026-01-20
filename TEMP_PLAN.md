# Libvpx CMake Build - Implementation Plan

## Overview

Build a cross-platform CMake wrapper for libvpx (ShiftMediaProject clone) targeting:
- **Windows x64** (MSVC, NASM) - Full SIMD optimisations
- **Linux x64** (GCC/Clang, NASM) - Full SIMD optimisations
- **macOS ARM64** (Clang) - Generic C only (no SIMD, for now)

---

## Key Decisions & Justifications

### Decision 1: Use SMP as Source of Truth

**Choice:** Extract source lists from ShiftMediaProject's `libvpx_files.props` rather than parsing original `.mk` files.

**Justification:**
- SMP has already solved the "which files to compile" problem
- Their XML is trivial to parse vs complex Makefile syntax
- They have pre-generated RTCD headers that work
- Reduces risk of missing files or incorrect conditionals

**Trade-off:** We're coupled to SMP's configuration choices. Acceptable since they enable all features.

---

### Decision 2: Pre-Generated RTCD Headers

**Choice:** Use static, pre-generated RTCD headers rather than running `rtcd.pl` at build time.

**Justification:**
- No Perl dependency at build time
- No complex `config.mk` generation
- Faster configure step
- SMP's approach works for Windows, we extend it to Linux

**Trade-off:** Need separate header sets per platform. Acceptable since we only have 3 platforms.

---

### Decision 3: macOS Uses Generic C (No SIMD)

**Choice:** On macOS ARM64, use pure C implementations only. No NEON intrinsics.

**Justification:**
- SMP only has x86 source lists
- Extracting ARM sources from `.mk` files is additional work
- Generic C is "correct", just slower
- macOS is primarily a dev/test platform, not production

**Trade-off:** VP8/VP9 encode/decode will be slower on Mac. Documented as future improvement.

**Future Work:** Parse `.mk` files for `HAVE_NEON` patterns, extract ARM source list, generate ARM RTCD headers.

---

### Decision 4: No 32-bit Support

**Choice:** x86_64 only. No Win32, no x86 Linux.

**Justification:**
- It's 2026. 32-bit is legacy.
- Reduces testing matrix
- SMP's x64-only assembly exclusions don't need handling

---

### Decision 5: No macOS Intel

**Choice:** Apple Silicon only. No x86_64 macOS.

**Justification:**
- Intel Macs are EOL (last sold 2021)
- Reduces testing matrix
- Can't use SMP's x86 assembly on macOS anyway (needs Mach-O format handling)

---

### Decision 6: Static Library Default

**Choice:** Default to static library. Shared optional via `BUILD_SHARED_LIBS`.

**Justification:**
- Simplest integration for consuming projects
- No symbol export complexity
- Matches typical dependency consumption pattern

---

### Decision 7: Disable WebM IO & LibYUV (IVF-Only)

**Choice:** Disable `CONFIG_WEBM_IO` and `CONFIG_LIBYUV` initially.

**Justification:**
- WebM IO pulls in libwebm (C++, additional complexity)
- LibYUV pulls in another dependency
- Core VP8/VP9 codec doesn't need them
- **Our use case is IVF container** (not WebM)

**Impact:** No `.webm` container parsing built-in. Use IVF format instead.

**IVF Container:**
- Simple container format used by libvpx's own test suite
- ffmpeg can output IVF: `ffmpeg -i input.mp4 -c:v libvpx-vp9 output.ivf`
- Native parsing example in `libvpx/test/ivf_video_source.h`
- 32-byte file header + 12-byte frame headers + raw frame data

---

### Decision 8: SMP Helper Files

**Choice:** Handle `dce_defs.c` and `vpx_config.c` carefully.

**Files:**
- **`SMP/dce_defs.c`** - Dead Code Elimination workaround for MSVC. Contains dummy references to ensure global initialisers run. May be needed if we hit "undefined symbol" linker errors.
- **`SMP/vpx_config.c`** - Implements `vpx_codec_build_config()` returning a string of build options. We'll create our own or copy from SMP.
- **`SMP/vpx_config.asm`** - Assembly defines for NASM. Required for x86_64 assembly.

**Approach:**
- Initially exclude `dce_defs.c` - add it back if we hit linker issues
- Create minimal `vpx_config.c` ourselves
- Copy `vpx_config.asm` to config directories

---

## Architecture

### File Structure (After Implementation)

```
vvpx/
├── CMakeLists.txt                 # Main build file
├── README.md                      # Project docs
├── cmake/
│   ├── VpxSources.cmake           # Source file lists (from SMP)
│   ├── VpxSourcesArm.cmake        # ARM-specific sources (generic C)
│   ├── VpxConfig.cmake            # Platform detection
│   └── VpxAssembly.cmake          # NASM setup for x86_64
├── config/
│   ├── win_x64/                   # Windows x64 config
│   │   ├── vpx_config.h           # From SMP
│   │   ├── vpx_config.asm         # From SMP
│   │   ├── vpx_version.h          # From SMP
│   │   ├── vp8_rtcd.h             # From SMP/x86_64/
│   │   ├── vp9_rtcd.h             # From SMP/x86_64/
│   │   ├── vpx_dsp_rtcd.h         # From SMP/x86_64/
│   │   └── vpx_scale_rtcd.h       # From SMP/x86_64/
│   ├── linux_x64/                 # Linux x64 config
│   │   └── (same files, tweaked for GCC/pthread)
│   └── macos_arm64/               # macOS ARM64 config
│       ├── vpx_config.h           # ARM-specific, no x86 SIMD
│       ├── vpx_version.h          # Same as others
│       ├── vp8_rtcd.h             # Generic C only mappings
│       ├── vp9_rtcd.h
│       ├── vpx_dsp_rtcd.h
│       └── vpx_scale_rtcd.h
├── scripts/
│   └── extract_sources.py         # One-shot source extraction
├── test/
│   └── version_test.c             # Minimal link test
└── libvpx/                        # Submodule (ShiftMediaProject)
```

---

## Implementation Phases

### Phase 1a: Windows x64 (Primary Development)

**Goal:** Get libvpx building on Windows with MSVC, NASM, and all x86_64 SIMD.

**Steps:**

1. **Create extraction script** (`scripts/extract_sources.py`)
   - Parse `libvpx/SMP/libvpx_files.props`
   - Output `cmake/VpxSources.cmake`

2. **Copy SMP config files**
   ```
   SMP/vpx_config.h      → config/win_x64/vpx_config.h
   SMP/vpx_config.asm    → config/win_x64/vpx_config.asm
   SMP/vpx_version.h     → config/win_x64/vpx_version.h
   SMP/x86_64/vp8_rtcd.h → config/win_x64/vp8_rtcd.h
   SMP/x86_64/vp9_rtcd.h → config/win_x64/vp9_rtcd.h
   SMP/x86_64/vpx_dsp_rtcd.h → config/win_x64/vpx_dsp_rtcd.h
   SMP/x86_64/vpx_scale_rtcd.h → config/win_x64/vpx_scale_rtcd.h
   ```

3. **Tweak vpx_config.h**
   - Disable `CONFIG_WEBM_IO` (0)
   - Disable `CONFIG_LIBYUV` (0)
   - Keep everything else as-is

4. **Create CMakeLists.txt**
   - Project definition with C, CXX, ASM_NASM
   - Include source lists
   - Configure NASM for win64 format
   - Add `vpx` static library target
   - Set include paths

5. **Create cmake/VpxAssembly.cmake**
   - Enable ASM_NASM language
   - Set format to `win64`
   - Set include path for `vpx_config.asm`

6. **Create cmake/VpxConfig.cmake**
   - Detect platform (WIN32, APPLE, UNIX)
   - Detect architecture (CMAKE_SYSTEM_PROCESSOR)
   - Set VPX_PLATFORM_DIR variable

7. **Build & Test**
   - `cmake -B build -G "Visual Studio 17 2022"`
   - `cmake --build build`
   - Link test with `vpx_codec_version()`

**Deliverables:**
- [ ] `CMakeLists.txt`
- [ ] `cmake/VpxSources.cmake`
- [ ] `cmake/VpxConfig.cmake`
- [ ] `cmake/VpxAssembly.cmake`
- [ ] `config/win_x64/*` (7 files)
- [ ] `scripts/extract_sources.py`
- [ ] `test/version_test.c`

---

### Phase 1b: Linux x64 (Transplant & Fix)

**Goal:** Get libvpx building on Linux with GCC/Clang and NASM.

**Steps:**

1. **Create Linux config files**
   - Copy from `config/win_x64/`
   - Modify `vpx_config.h`:
     - `CONFIG_MSVS` → 0
     - `CONFIG_GCC` → 1
     - `HAVE_PTHREAD_H` → 1
     - `HAVE_UNISTD_H` → 1

2. **Update VpxAssembly.cmake**
   - Detect Linux: use `elf64` format instead of `win64`
   - Use `nasm` or `yasm` (whichever is available)

3. **Update VpxConfig.cmake**
   - Handle Linux platform detection
   - Link `pthread` library

4. **Build & Test on Linux box**
   - Fix any compilation errors
   - Run link test

**Expected Issues:**
- NASM may need different flags
- pthread linking
- Possible include path differences

---

### Phase 1c: macOS ARM64 (Transplant & Fix)

**Goal:** Get libvpx building on macOS with Clang, using generic C fallbacks.

**Steps:**

1. **Create ARM source list** (`cmake/VpxSourcesArm.cmake`)
   - Start from x86 source list
   - Remove all `x86/` paths
   - Remove all `.asm` files
   - The remaining `.c` files should be portable

2. **Create macOS ARM config files**
   - `vpx_config.h`:
     - `VPX_ARCH_X86_64` → 0
     - `VPX_ARCH_AARCH64` → 1
     - All `HAVE_SSE*`, `HAVE_AVX*` → 0
     - `HAVE_NEON` → 0 (we're not using NEON yet)
     - `CONFIG_GCC` → 1 (Clang is GCC-compatible)
     - `HAVE_PTHREAD_H` → 1
     - `HAVE_UNISTD_H` → 1

3. **Create generic RTCD headers**
   - All function pointers map to `_c` (C) versions
   - Example: `#define vpx_convolve_avg vpx_convolve_avg_c`
   - Can generate these by modifying SMP's headers

4. **Update VpxConfig.cmake**
   - Detect Apple + ARM64
   - Disable assembly entirely
   - Use `VpxSourcesArm.cmake` instead of `VpxSources.cmake`

5. **Update CMakeLists.txt**
   - Conditional source inclusion based on platform
   - No NASM on ARM

6. **Build & Test on Mac**
   - Fix any compilation errors
   - Run link test

**Expected Issues:**
- Missing x86-specific sources that have no generic equivalent
- RTCD header generation may need manual work
- Possible Apple-specific compiler flags

---

## Source Extraction Script

```python
#!/usr/bin/env python3
"""
Extract source lists from SMP/libvpx_files.props
Run once, commit the output.
"""
import xml.etree.ElementTree as ET
from pathlib import Path

def parse_props(props_path: Path) -> tuple[list, list, list]:
    """Parse SMP props file, return (c_sources, asm_sources, headers)"""
    tree = ET.parse(props_path)
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    
    c_sources = []
    asm_sources = []
    headers = []
    
    for item in tree.findall('.//m:ClCompile', ns):
        path = item.get('Include')
        # Convert Windows paths to CMake-friendly format
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Skip SMP-specific files (we'll handle config separately)
        if path.startswith('libvpx/SMP/') or 'vpx_config.c' in path or 'dce_defs.c' in path:
            continue
        c_sources.append(path)
    
    for item in tree.findall('.//m:YASM', ns):
        path = item.get('Include')
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Check for x64-only files (we only support x64)
        excluded = item.find('m:ExcludedFromBuild', ns)
        if excluded is not None:
            cond = excluded.get('Condition', '')
            if 'Win32' in cond:
                # x64 only - include it
                pass
            else:
                # Some other exclusion - skip for safety
                continue
        asm_sources.append(path)
    
    for item in tree.findall('.//m:ClInclude', ns):
        path = item.get('Include')
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Skip SMP-specific headers
        if path.startswith('libvpx/SMP/') or 'vpx_config.h' in path or 'vpx_version.h' in path or '_rtcd.h' in path:
            continue
        headers.append(path)
    
    return sorted(c_sources), sorted(asm_sources), sorted(headers)


def generate_cmake(c_sources: list, asm_sources: list, headers: list) -> str:
    """Generate VpxSources.cmake content"""
    lines = [
        '# Auto-generated from libvpx/SMP/libvpx_files.props',
        '# Do not edit manually - re-run scripts/extract_sources.py',
        '',
        'set(VPX_C_SOURCES',
    ]
    for src in c_sources:
        lines.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines.append(')')
    lines.append('')
    
    lines.append('set(VPX_X86_ASM_SOURCES')
    for src in asm_sources:
        lines.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines.append(')')
    lines.append('')
    
    lines.append('set(VPX_HEADERS')
    for src in headers:
        lines.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines.append(')')
    
    return '\n'.join(lines)


def main():
    root = Path(__file__).parent.parent
    props = root / 'libvpx' / 'SMP' / 'libvpx_files.props'
    
    if not props.exists():
        print(f"Error: {props} not found. Is the submodule initialised?")
        return 1
    
    c, asm, h = parse_props(props)
    cmake = generate_cmake(c, asm, h)
    
    out_path = root / 'cmake' / 'VpxSources.cmake'
    out_path.parent.mkdir(exist_ok=True)
    out_path.write_text(cmake, encoding='utf-8')
    
    print(f"Generated {out_path}")
    print(f"  C sources:   {len(c)}")
    print(f"  ASM sources: {len(asm)}")
    print(f"  Headers:     {len(h)}")
    return 0


if __name__ == '__main__':
    exit(main())
```

---

## RTCD Header Generation (macOS ARM)

For macOS ARM64, we need RTCD headers that map everything to generic C.

**Approach:** Take SMP's x86_64 RTCD headers and regex replace all function mappings.

```python
# Pseudocode for generating generic RTCD headers
import re

def make_generic(rtcd_content: str) -> str:
    # Replace: #define vpx_foo vpx_foo_sse2
    # With:    #define vpx_foo vpx_foo_c
    content = re.sub(
        r'#define (\w+) \1_(?:sse2|sse3|ssse3|sse4_1|avx|avx2|avx512|mmx)',
        r'#define \1 \1_c',
        rtcd_content
    )
    
    # Remove RTCD_EXTERN function pointers (not needed for static dispatch)
    content = re.sub(
        r'RTCD_EXTERN .*?;',
        '',
        content
    )
    
    # Remove runtime init function bodies
    # ... etc
    
    return content
```

This can be done as a one-shot script when we get to Phase 1c.

---

## CMake Structure

### CMakeLists.txt (Skeleton)

```cmake
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
```

### cmake/VpxConfig.cmake

```cmake
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
```

### cmake/VpxAssembly.cmake

```cmake
# NASM/YASM setup for x86_64 assembly

if(NOT VPX_ARCH_X86_64)
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
```

---

## Testing

### Minimal Link Test (`test/version_test.c`)

```c
#include "vpx/vpx_codec.h"
#include <stdio.h>

int main(void) {
    printf("libvpx version: %s\n", vpx_codec_version_str());
    
    // Basic sanity check
    int version = vpx_codec_version();
    if (version == 0) {
        printf("ERROR: version returned 0\n");
        return 1;
    }
    
    printf("Version number: %d.%d.%d\n",
           (version >> 16) & 0xFF,
           (version >> 8) & 0xFF,
           version & 0xFF);
    
    return 0;
}
```

### CMake for Test

```cmake
# In CMakeLists.txt
if(PROJECT_IS_TOP_LEVEL)
    add_executable(version_test test/version_test.c)
    target_link_libraries(version_test PRIVATE vpx)
endif()
```

### Upstream Test Suite (Reference)

libvpx has a comprehensive GoogleTest-based test suite in `libvpx/test/`:
- **136+ test files** covering encode, decode, SIMD functions, etc.
- Uses **IVF files as test vectors** (same format as our use case)
- Native IVF parsing: `libvpx/test/ivf_video_source.h`
- End-to-end tests: `vp9_end_to_end_test.cc`, `vp8_datarate_test.cc`
- Decode tests: `decode_api_test.cc`, `test_vector_test.cc`

**We are NOT building the test suite** (requires GoogleTest integration, many additional sources).

However, the upstream tests serve as:
- Reference for how to use the codec API
- Validation that the codec works (can run with official build)
- Source of test vectors (`.ivf` files)

---

## CI Workflow (GitHub Actions)

```yaml
name: Build

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: windows-latest
            name: Windows x64 MSVC
          - os: ubuntu-latest
            name: Linux x64 GCC
          - os: macos-14
            name: macOS ARM64 Clang
    
    runs-on: ${{ matrix.os }}
    name: ${{ matrix.name }}
    
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: true
      
      - name: Install NASM (Windows)
        if: runner.os == 'Windows'
        run: choco install nasm -y
      
      - name: Install NASM (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get install -y nasm
      
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Test
        run: ./build/version_test
        if: runner.os != 'Windows'
      
      - name: Test (Windows)
        run: .\build\Release\version_test.exe
        if: runner.os == 'Windows'
```

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| NASM format issues on Linux | Medium | Medium | Test early, have fallback to disable ASM |
| Missing portable C sources on ARM | Medium | High | Start with x86 sources minus x86/ paths |
| RTCD headers need manual fixes | Medium | Medium | Script to generate, then manual fixup |
| Compile errors on specific files | High | Low | Fix them as they appear |
| Link errors from missing symbols | Medium | Medium | Check RTCD function mappings |

---

## Open Questions

None currently - all major decisions made!

---

## Checklist

### Phase 1a (Windows x64)
- [ ] Create `scripts/extract_sources.py`
- [ ] Run extraction, generate `cmake/VpxSources.cmake`
- [ ] Copy SMP config files to `config/win_x64/`
- [ ] Modify `vpx_config.h` (disable WebM IO, LibYUV)
- [ ] Create `CMakeLists.txt`
- [ ] Create `cmake/VpxConfig.cmake`
- [ ] Create `cmake/VpxAssembly.cmake`
- [ ] Create `test/version_test.c`
- [ ] Build with MSVC
- [ ] Run version_test.exe

### Phase 1b (Linux x64)
- [ ] Create `config/linux_x64/vpx_config.h`
- [ ] Copy other config files from win_x64
- [ ] Update NASM flags for elf64
- [ ] Build on Linux
- [ ] Fix any issues
- [ ] Run version_test

### Phase 1c (macOS ARM64)
- [ ] Create `cmake/VpxSourcesArm.cmake`
- [ ] Create `config/macos_arm64/vpx_config.h`
- [ ] Generate generic RTCD headers
- [ ] Update CMake for ARM path
- [ ] Build on Mac
- [ ] Fix any issues
- [ ] Run version_test

### Final
- [ ] Set up GitHub Actions CI
- [ ] Clean up TEMP files
- [ ] Tag v1.0.0

---

*Last updated: 2026-01-20*
*Based on libvpx v1.15.1 from ShiftMediaProject*