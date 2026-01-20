#!/usr/bin/env python3
"""
Generate Generic C RTCD headers for ARM from x86_64 headers.
"""
import re
from pathlib import Path

def process_header(content: str) -> str:
    lines = content.splitlines()
    out_lines = []
    
    opt_suffixes = ['sse2', 'ssse3', 'sse4_1', 'avx', 'avx2', 'avx512', 'mmx']
    defined_macros = set()
    
    in_setup_function = False
    
    for line in lines:
        line_strip = line.strip()
        
        # 0. Handle setup_rtcd_internal block
        if 'static void setup_rtcd_internal(void)' in line:
            in_setup_function = True
            out_lines.append(line)
            out_lines.append('{')
            out_lines.append('}')
            continue
            
        if in_setup_function:
            if line_strip == '}':
                in_setup_function = False
            continue

        # 1. Skip optimized function declarations
        is_optimized_decl = False
        for suf in opt_suffixes:
            if f'_{suf}(' in line:
                is_optimized_decl = True
                break
        if is_optimized_decl:
            continue
            
        # 2. Handle RTCD_EXTERN function pointers
        if 'RTCD_EXTERN' in line and '(*' in line:
            m = re.search(r'\(\*(\w+)\)', line)
            if m:
                func_name = m.group(1)
                if func_name not in defined_macros:
                    out_lines.append(f'#define {func_name} {func_name}_c')
                    defined_macros.add(func_name)
            continue
            
        # 3. Rewrite macros
        if line_strip.startswith('#define'):
            pattern = r'#define\s+(\w+)\s+(\w+)(?:_(?:' + '|'.join(opt_suffixes) + r'))\b'
            m = re.match(pattern, line_strip)
            if m:
                name = m.group(1)
                target_base = m.group(2)
                if name not in defined_macros:
                    out_lines.append(f'#define {name} {target_base}_c')
                    defined_macros.add(name)
                continue
        
        # 4. Remove include of non-existent headers
        if '#include' in line and 'vpx_ports/x86.h' in line:
            continue
        if '#include' in line and 'x86/' in line:
            continue

        out_lines.append(line)
        
    return '\n'.join(out_lines)

def main():
    root = Path(__file__).parent.parent
    in_dir = root / 'libvpx' / 'SMP' / 'x86_64'
    out_dir = root / 'config' / 'macos_arm64'
    out_dir.mkdir(parents=True, exist_ok=True)
    
    headers = ['vp8_rtcd.h', 'vp9_rtcd.h', 'vpx_dsp_rtcd.h', 'vpx_scale_rtcd.h']
    
    for h in headers:
        in_path = in_dir / h
        if not in_path.exists():
            print(f"Warning: {in_path} not found")
            continue
            
        content = in_path.read_text(encoding='utf-8')
        new_content = process_header(content)
        
        header_comment = [
            "/*",
            " * Auto-generated Generic C RTCD header for ARM64",
            " * Generated from x86_64 RTCD headers by scripts/gen_rtcd_arm.py",
            " */",
            ""
        ]
        
        out_path = out_dir / h
        out_path.write_text('\n'.join(header_comment) + new_content, encoding='utf-8')
        print(f"Generated {out_path}")

if __name__ == '__main__':
    main()