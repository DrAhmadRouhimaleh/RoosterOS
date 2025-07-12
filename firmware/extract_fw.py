#!/usr/bin/env python3
"""
extract_fw.py

Firmware extraction for UEFI/PEI volumes:
 - Detect UEFI Firmware Volume headers
 - Parse FV header and block map
 - Extract raw volume, FFS files, sections
 - Support Intel FFS, PEI capsules, raw blobs
 - CLI with logging, validation, and Python API
"""

import os
import sys
import argparse
import struct
import logging
import uuid
import lzma

# Constants
EFI_FV_SIG = b"_FVH"
UEFI_CAPSULE_SIG = b"EFI PART"
FVH_OFFSET = 0x40
ALIGN = 8

# Utility
def align_up(x, a):
    return (x + a - 1) & ~(a - 1)

# Data classes
class FirmwareVolume:
    def __init__(self, data, base_offset=0):
        self.data = data
        self.base = base_offset
        self.header_len = 0
        self.fv_len = 0
        self.block_map = []
        self.files = []

    def parse_header(self):
        # Locate FVH signature
        sig_off = self.data.find(EFI_FV_SIG, FVH_OFFSET)
        if sig_off < 0:
            raise ValueError("FV header not found")
        hdr = self.data[sig_off-0x40: sig_off+0x40]
        zero, fs_guid, fv_len, sig = struct.unpack_from("<16s16sQ4s", hdr, 0)
        if sig != EFI_FV_SIG:
            raise ValueError("Bad FV signature")
        self.fv_len = fv_len
        self.header_len = struct.unpack_from("<H", hdr, 0x28)[0]
        # Block map
        off = sig_off + 0x4C
        while True:
            num, blen = struct.unpack_from("<HH", self.data, off)
            off += 4
            if num == 0 or blen == 0:
                break
            self.block_map.append((num, blen))

        logging.info(f"FV parsed: length={fv_len}, header={self.header_len}, blocks={self.block_map}")

    def parse_files(self):
        ptr = self.header_len
        end = self.fv_len
        idx = 0
        while ptr + 24 < end:
            raw = self.data[ptr:ptr+24]
            guid = uuid.UUID(bytes_le=raw[:16])
            if guid.int == 0:
                break  # terminator
            size = int.from_bytes(raw[0x12:0x15], 'little')
            body = self.data[ptr:ptr+size]
            ffs = FfsFile(body, ptr)
            ffs.parse()
            self.files.append(ffs)
            ptr += align_up(size, ALIGN)
            idx += 1
        logging.info(f"Found {len(self.files)} FFS files")

    def extract(self, outdir):
        os.makedirs(outdir, exist_ok=True)
        # write full FV
        with open(os.path.join(outdir, "fv.bin"), "wb") as f:
            f.write(self.data[:self.fv_len])
        # extract files
        for i, ffs in enumerate(self.files):
            fname = f"ffs_{i:02d}_{ffs.guid}.bin"
            path = os.path.join(outdir, fname)
            with open(path, "wb") as f:
                f.write(ffs.raw)
            logging.debug(f"Extracted {path}")
            # extract sections
            for j, sec in enumerate(ffs.sections):
                sname = f"sec_{i:02d}_{j:02d}_{sec.type_name()}.bin"
                sp = os.path.join(outdir, sname)
                with open(sp, "wb") as sf:
                    sf.write(sec.payload)
                logging.debug(f"  → section {sp}")

class FfsFile:
    def __init__(self, raw, offset):
        self.raw = raw
        self.offset = offset
        self.guid = uuid.UUID(bytes_le=raw[:16])
        self.size = int.from_bytes(raw[0x12:0x15], 'little')
        self.sections = []

    def parse(self):
        ptr = 24
        while ptr + 4 <= self.size:
            sec = EfiSection(self.raw[ptr:], ptr)
            sec.parse()
            self.sections.append(sec)
            ptr += align_up(sec.size, ALIGN)

class EfiSection:
    def __init__(self, raw, offset):
        self.raw = raw
        self.offset = offset
        self.type = raw[0]
        self.size = int.from_bytes(raw[1:4], 'little')
        self.attrs = raw[4] if len(raw) > 4 else 0
        self.payload = b""

    def parse(self):
        body = self.raw[4:self.size]
        if self.type == 0x01:  # compression
            alg = body[0]
            if alg != 1:
                raise NotImplementedError("Unsupported compression")
            dsz = int.from_bytes(body[1:4], 'little')
            self.payload = lzma.decompress(body[4:])
        else:
            self.payload = body

    def type_name(self):
        return {
            0x01: "Comp",
            0x10: "PE32",
            0x11: "PIC",
            0x20: "Ver",
            0x24: "GUID"
        }.get(self.type, f"Sec{self.type:02X}")

def main():
    p = argparse.ArgumentParser(description="Extract UEFI FW volumes")
    p.add_argument("blob", help="input blob (FV, capsule, raw)")
    p.add_argument("-o", "--out", default="fw_out", help="output dir")
    p.add_argument("-v", "--verbose", action="store_true", help="debug logs")
    args = p.parse_args()

    lvl = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format="%(levelname)s: %(message)s", level=lvl)

    # Read blob
    with open(args.blob, "rb") as f:
        data = f.read()

    # Try parsing as FV
    fv = FirmwareVolume(data)
    try:
        fv.parse_header()
        fv.parse_files()
    except ValueError as e:
        logging.error(f"Not a valid FV: {e}")
        sys.exit(1)

    fv.extract(args.out)
    logging.info("Extraction complete.")

if __name__ == "__main__":
    main()
