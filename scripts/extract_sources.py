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
    ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}

    c_sources = []
    asm_sources = []
    headers = []

    for item in tree.findall(".//m:ClCompile", ns):
        path = item.get("Include")
        # Convert Windows paths to CMake-friendly format
        path = path.replace("\\", "/").replace("../", "libvpx/")
        # Skip SMP-specific files (we'll handle config separately)
        if (
            path.startswith("libvpx/SMP/")
            or "vpx_config.c" in path
            or "dce_defs.c" in path
        ):
            continue
        c_sources.append(path)

    for item in tree.findall(".//m:YASM", ns):
        path = item.get("Include")
        path = path.replace("\\", "/").replace("../", "libvpx/")
        # Check for x64-only files (we only support x64)
        excluded = item.find("m:ExcludedFromBuild", ns)
        if excluded is not None:
            cond = excluded.get("Condition", "")
            if "Win32" in cond:
                # x64 only - include it
                pass
            else:
                # Some other exclusion - skip for safety
                continue
        asm_sources.append(path)

    for item in tree.findall(".//m:ClInclude", ns):
        path = item.get("Include")
        path = path.replace("\\", "/").replace("../", "libvpx/")
        # Skip SMP-specific headers
        if (
            path.startswith("libvpx/SMP/")
            or "vpx_config.h" in path
            or "vpx_version.h" in path
            or "_rtcd.h" in path
        ):
            continue
        headers.append(path)

    return sorted(c_sources), sorted(asm_sources), sorted(headers)


def generate_cmake(c_sources: list, asm_sources: list, headers: list) -> str:
    """Generate VpxSources.cmake content"""
    lines = [
        "# Auto-generated from libvpx/SMP/libvpx_files.props",
        "# Do not edit manually - re-run scripts/extract_sources.py",
        "",
        "set(VPX_C_SOURCES",
    ]
    for src in c_sources:
        lines.append(f"    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}")
    lines.append(")")
    lines.append("")

    lines.append("set(VPX_X86_ASM_SOURCES")
    for src in asm_sources:
        lines.append(f"    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}")
    lines.append(")")
    lines.append("")

    lines.append("set(VPX_HEADERS")
    for src in headers:
        lines.append(f"    ${{CMAKE_CURRENT_SOURCE_DIR}}/{src}")
    lines.append(")")

    return "\n".join(lines)


def main():
    root = Path(__file__).parent.parent
    props = root / "libvpx" / "SMP" / "libvpx_files.props"

    if not props.exists():
        print(f"Error: {props} not found. Is the submodule initialised?")
        return 1

    c, asm, h = parse_props(props)
    cmake = generate_cmake(c, asm, h)

    out_path = root / "cmake" / "VpxSources.cmake"
    out_path.parent.mkdir(exist_ok=True)
    out_path.write_text(cmake, encoding="utf-8")

    print(f"Generated {out_path}")
    print(f"  C sources:   {len(c)}")
    print(f"  ASM sources: {len(asm)}")
    print(f"  Headers:     {len(h)}")
    return 0


if __name__ == "__main__":
    exit(main())
