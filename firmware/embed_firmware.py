#!/usr/bin/env python3
"""
embed_firmware.py

Convert a binary blob into a C header for compile-time inclusion.
Features:
 - Add include guards
 - Select section placement (e.g., .rodata)
 - Optional compression (gzip)
 - Optional checksum array and runtime validation
"""

import os
import sys
import argparse
import gzip
import hashlib
from textwrap import wrap

TEMPLATE = """\
// Auto-generated from {blob_name}
// MD5: {md5}
// Size: {orig_size} bytes{comp_info}

#pragma once
#ifndef {guard}
#define {guard}

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

// Place in .rodata by default
#ifndef EMBED_SEC_ATTR
#define EMBED_SEC_ATTR __attribute__((section(".rodata"), aligned(16)))
#endif

// Original data length
static const size_t {name}_len = {orig_size};

// {comp_comment}
static const uint8_t {name}_data[] EMBED_SEC_ATTR = {{
{data_array}
}};

{checksum_defs}
#ifdef __cplusplus
}}
#endif

#endif // {guard}
"""

def compute_md5(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()

def compress_data(data: bytes, level: int=6) -> bytes:
    return gzip.compress(data, compresslevel=level)

def format_array(data: bytes, cols: int=12) -> str:
    hex_bytes = [f"0x{b:02X}" for b in data]
    lines = []
    for chunk in wrap(", ".join(hex_bytes), cols*6):
        lines.append("    " + chunk)
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(
        description="Embed a binary blob into a C header."
    )
    parser.add_argument("blob", help="Path to input binary blob")
    parser.add_argument("header", help="Path to output C header")
    parser.add_argument("-n", "--name", default="firmware_blob",
                        help="C symbol base name")
    parser.add_argument("-c", "--compress", action="store_true",
                        help="Gzip-compress blob before embedding")
    parser.add_argument("-l", "--level", type=int, default=6,
                        help="Gzip compression level (1-9)")
    parser.add_argument("-k", "--checksum", action="store_true",
                        help="Generate CRC32 checksum array and function")
    args = parser.parse_args()

    # Read blob
    try:
        with open(args.blob, "rb") as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: blob '{args.blob}' not found.", file=sys.stderr)
        sys.exit(1)

    blob_name = os.path.basename(args.blob)
    orig_size = len(data)
    md5 = compute_md5(data)

    comp_info = ""
    comp_comment = "Data stored uncompressed."
    if args.compress:
        cdata = compress_data(data, level=args.level)
        comp_info = f"\n// Compressed size: {len(cdata)} bytes (gzip level={args.level})"
        comp_comment = f"Data is gzip-compressed (level={args.level})."
        array_data = cdata
    else:
        array_data = data

    # Format data array
    data_array = format_array(array_data)
    
    # Checksum generation
    checksum_defs = ""
    if args.checksum:
        crc = hex(hashlib.crc32(data) & 0xFFFFFFFF)
        checksum_defs = f"""\
static const uint32_t {args.name}_crc32 = {crc};

static inline uint32_t verify_{args.name}_crc32(void) {{
    // Simple CRC32 check
    extern const uint8_t {args.name}_data[];
    extern const size_t  {args.name}_len;
    uint32_t acc = 0xFFFFFFFF;
    for (size_t i = 0; i < {args.name}_len; i++) {{
        acc = (acc >> 8) ^ crc32_table[(acc ^ {args.name}_data[i]) & 0xFF];
    }}
    return acc ^ 0xFFFFFFFF;
}}
"""
    # Prepare include guard
    guard = args.name.upper() + "_H"

    # Write header
    with open(args.header, "w") as h:
        h.write(TEMPLATE.format(
            blob_name=blob_name,
            md5=md5,
            orig_size=orig_size,
            comp_info=comp_info,
            guard=guard,
            name=args.name,
            comp_comment=comp_comment,
            data_array=data_array,
            checksum_defs=checksum_defs
        ))

if __name__ == "__main__":
    main()
