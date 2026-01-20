#!/usr/bin/env python3
"""
Extract source lists from SMP/libvpx_files.props
Generates cmake/VpxSources.cmake (x64) and cmake/VpxSourcesArm.cmake (Generic C)
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
        if not path: continue
        # Convert Windows paths to CMake-friendly format
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Skip SMP-specific files (we'll handle config separately)
        if path.startswith('libvpx/SMP/') or 'vpx_config.c' in path or 'dce_defs.c' in path:
            continue
        c_sources.append(path)
    
    for item in tree.findall('.//m:YASM', ns):
        path = item.get('Include')
        if not path: continue
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Check for x64-only files (we only support x64 for Windows/Linux)
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
        if not path: continue
        path = path.replace('\\', '/').replace('../', 'libvpx/')
        # Skip SMP-specific headers
        if path.startswith('libvpx/SMP/') or 'vpx_config.h' in path or 'vpx_version.h' in path or '_rtcd.h' in path:
            continue
        headers.append(path)
    
    return sorted(c_sources), sorted(asm_sources), sorted(headers)

def filter_arm_sources(c_sources: list) -> list:
    """Filter C sources for ARM (Generic C only)"""
    arm_sources = []
    for src in c_sources:
        # Exclude x86-specific C files (intrinsics)
        if '/x86/' in src:
            continue
        # Exclude ARM-specific NEON files (intrinsics) - we are doing generic C for now
        if '/arm/' in src:
            continue
        # Exclude MIPS etc (shouldn't be in SMP list but good to be safe)
        if '/mips/' in src or '/ppc/' in src:
            continue
        arm_sources.append(src)
    return arm_sources

def generate_cmake(c_sources: list, asm_sources: list, headers: list, arm_sources: list) -> tuple[str, str]:
    """Generate CMake content for both x64 and ARM"""
    
    # x64 Content
    lines_x64 = [
        '# Auto-generated from libvpx/SMP/libvpx_files.props',
        '# Do not edit manually - re-run scripts/extract_sources.py',
        '',
        'set(VPX_C_SOURCES',
    ]
    for src in c_sources:
        lines_x64.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines_x64.append(')')
    lines_x64.append('')
    
    lines_x64.append('set(VPX_X86_ASM_SOURCES')
    for src in asm_sources:
        lines_x64.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines_x64.append(')')
    lines_x64.append('')
    
    lines_x64.append('set(VPX_HEADERS')
    for src in headers:
        lines_x64.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines_x64.append(')')
    
    # ARM Content (Generic C)
    lines_arm = [
        '# Auto-generated from libvpx/SMP/libvpx_files.props (Filtered for Generic C)',
        '# Do not edit manually - re-run scripts/extract_sources.py',
        '',
        'set(VPX_C_SOURCES_ARM',
    ]
    for src in arm_sources:
        lines_arm.append(f'    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}')
    lines_arm.append(')')
    lines_arm.append('')
    
    lines_arm.append('set(VPX_HEADERS_ARM')
    lines_arm.append('    ${VPX_HEADERS}')
    lines_arm.append(')')

    return '\n'.join(lines_x64), '\n'.join(lines_arm)

def main():
    root = Path(__file__).parent.parent
    props = root / 'libvpx' / 'SMP' / 'libvpx_files.props'
    
    if not props.exists():
        print(f'Error: {props} not found. Is the submodule initialised?')
        return 1
    
    c, asm, h = parse_props(props)
    c_arm = filter_arm_sources(c)
    
    cmake_x64, cmake_arm = generate_cmake(c, asm, h, c_arm)
    
    out_x64 = root / 'cmake' / 'VpxSources.cmake'
    out_x64.parent.mkdir(exist_ok=True)
    out_x64.write_text(cmake_x64, encoding='utf-8')
    
    out_arm = root / 'cmake' / 'VpxSourcesArm.cmake'
    out_arm.write_text(cmake_arm, encoding='utf-8')
    
    print(f'Generated {out_x64}')
    print(f'  C sources:   {len(c)}')
    print(f'  ASM sources: {len(asm)}')
    print(f'  Headers:     {len(h)}')
    
    print(f'Generated {out_arm}')
    print(f'  ARM C sources: {len(c_arm)}')
    return 0

if __name__ == '__main__':
    exit(main())