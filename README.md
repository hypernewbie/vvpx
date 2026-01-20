# Vvpx

Unofficial CMake build wrapper for libvpx, the WebM VP8/VP9 SDK.

Uses [ShiftMediaProject's libvpx fork](https://github.com/ShiftMediaProject/libvpx) as a submodule, which is actively maintained and provides pre-configured source lists and RTCD headers.

## Why This Exists

libvpx's official build system uses a custom `configure` script and Makefiles. This project wraps it in CMake for easy integration into CMake-based projects.

## Supported Platforms

| Platform | Architecture | Compiler | SIMD Optimisations |
|----------|-------------|----------|-------------------|
| Windows | x64 | MSVC | SSE2/SSE3/SSSE3/SSE4.1/AVX/AVX2 via NASM |
| Linux | x64 | GCC/Clang | SSE2/SSE3/SSSE3/SSE4.1/AVX/AVX2 via NASM |
| macOS | ARM64 (Apple Silicon) | Clang | Generic C only (no SIMD) |

## Explicitly Not Supported

These platforms/configurations are **intentionally excluded**:

- **32-bit x86** - It's 2026. 64-bit only.
- **macOS Intel** - Apple Silicon only. Intel Macs are EOL.
- **ARM Linux** - Not tested. May work with the generic C path.
- **Windows ARM** - Not tested. May work with the generic C path.
- **MIPS/PowerPC/LoongArch** - Not supported.

## Features

All VP8/VP9 codec features are enabled:
- VP8 encoder & decoder
- VP9 encoder & decoder
- VP9 high bit depth (10/12-bit)
- Multithreading
- Postprocessing
- Temporal denoising

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `OFF` | Build shared library instead of static |

## Future Work

### ARM NEON Optimisations (macOS)

Currently macOS ARM64 uses generic C implementations for all DSP functions. This works but is not optimised.

Future improvement: Extract ARM NEON source lists from the original `.mk` files and generate proper ARM RTCD headers. This would enable SIMD optimisations on Apple Silicon.

The NEON intrinsics are implemented in `.c` files (not assembly), so this is purely a source list extraction task.

## Implementation Notes

Notes for future maintenance:

### SMP-Specific Files

ShiftMediaProject's `SMP/` folder contains some custom files we need to be aware of:

- **`dce_defs.c`** - Dead Code Elimination workaround. May be needed if we hit linker errors about undefined symbols. SMP uses this to ensure certain global initialisers run.

- **`vpx_config.c`** - Defines `vpx_codec_build_config()` which returns a string describing build options. We create our own version or copy from SMP.

- **`vpx_config.asm`** - Assembly version of config defines for NASM/YASM. Required for x86_64 assembly to compile.

### libvpx Test Suite

libvpx has a comprehensive GoogleTest-based test suite in `libvpx/test/`:
- 136+ test files covering encode, decode, SIMD functions, etc.
- Uses IVF files as test vectors (same format we use!)
- Native IVF parsing via `ivf_video_source.h`

Building the test suite is out of scope for this wrapper, but the upstream tests can validate the codec is working correctly if needed.

### IVF Container Format

This wrapper is designed for use with IVF container format (not WebM). IVF is libvpx's native test format:
- Simple header + raw VP8/VP9 frames
- ffmpeg can convert to IVF: `ffmpeg -i input.mp4 -c:v libvpx-vp9 output.ivf`
- No WebM IO dependency required

## Credits

- [libvpx](https://chromium.googlesource.com/webm/libvpx/) - The WebM Project
- [ShiftMediaProject](https://github.com/ShiftMediaProject/libvpx) - Pre-configured MSVC build files

## Licence

libvpx is BSD-licensed. See `libvpx/LICENSE` for details.


