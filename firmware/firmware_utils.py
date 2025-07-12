#!/usr/bin/env python3
"""
firmware_utils.py

Library to parse a UEFI Firmware Volume (FV), extract FFS files and their sections.
Supports LZMA-compressed sections (EFI_COMPRESSION_ALGORYTHM_LZMA).
"""

import struct
import uuid
import lzma
import logging

EFI_FV_SIGNATURE = b"_FVH"
EFI_SECTION_COMPR = 0x01  # Compression section


def align_up(x, align):
    return (x + align - 1) & ~(align - 1)


class FirmwareVolume:
    def __init__(self, data: bytes):
        self.data = data
        self.header_length = 0
        self.fv_length = 0
        self.block_map = []
        self.files = []

    def parse_header(self):
        # ZeroVector[16], FileSystemGuid[16], FvLength[8], Signature[4], ...
        hdr = self.data[:0x30]
        zero_vec, fs_guid, fv_len, sig = struct.unpack_from("<16s16sQ4s", hdr, 0)
        if sig != EFI_FV_SIGNATURE:
            raise ValueError("Bad FV signature")
        self.fv_length = fv_len
        # HeaderLength @ offset 0x28, uint16_t
        self.header_length = struct.unpack_from("<H", hdr, 0x28)[0]
        # BlockMap starts at offset 0x30; entries: UINT16 NumBlocks, UINT16 BlockLen
        offset = 0x30
        while True:
            num, blen = struct.unpack_from("<HH", self.data, offset)
            offset += 4
            if num == 0 or blen == 0:
                break
            self.block_map.append((num, blen))
        logging.info(f"FV header: length={self.fv_length}, hdr_len={self.header_length}, blocks={self.block_map}")

    def parse_files(self):
        ptr = self.header_length
        end = self.fv_length
        while ptr + 24 < end:
            f = FfsFile(self.data[ptr:], ptr)
            if f.is_terminator():
                break
            f.parse()
            self.files.append(f)
            ptr += f.size_padded()
        logging.info(f"Found {len(self.files)} FFS files")

    def parse(self):
        self.parse_header()
        self.parse_files()


class FfsFile:
    def __init__(self, data: bytes, fv_offset: int):
        self._raw = data
        self._fv_offset = fv_offset
        self.guid = None
        self.type = 0
        self.attributes = 0
        self.size = 0
        self.state = 0
        self.data = b""
        self.sections = []

    def is_terminator(self):
        # A zeroed GUID indicates end-of-volume file
        return self._raw[:16] == b"\x00" * 16

    def parse_header(self):
        name = self._raw[:16]
        self.guid = uuid.UUID(bytes_le=name)
        self.type, self.attributes = struct.unpack_from("<BB", self._raw, 16)
        size_bytes = self._raw[18:21]  # 3-byte little-endian
        self.size = size_bytes[0] | (size_bytes[1] << 8) | (size_bytes[2] << 16)
        self.state = self._raw[21]
        logging.debug(f"FFS File @0x{self._fv_offset:X}: GUID={self.guid}, type=0x{self.type:X}, size={self.size}")

    def parse_sections(self):
        offset = 24
        end = self.size
        while offset + 4 <= end:
            sec = EfiSection(self._raw[offset:end], offset)
            sec.parse()
            self.sections.append(sec)
            offset += sec.size_padded()
        logging.debug(f"  → {len(self.sections)} sections")

    def parse(self):
        self.parse_header()
        self.data = self._raw[24:self.size]
        self.parse_sections()

    def size_padded(self, align=8):
        return align_up(self.size, align)


class EfiSection:
    def __init__(self, data: bytes, file_offset: int):
        self._raw = data
        self._file_offset = file_offset
        self.type = 0
        self.size = 0
        self.attrs = 0
        self.payload = b""

    def parse(self):
        # Common header: UINT8 Type, UINT24 Size, UINT8 Attributes
        hdr = self._raw[:4]
        self.type = hdr[0]
        self.size = hdr[1] | (hdr[2] << 8) | (hdr[3] << 16)
        self.attrs = self._raw[4] if len(self._raw) > 4 else 0
        body = self._raw[4:self.size]
        if self.type == EFI_SECTION_COMPR and self.attrs == 0x01:
            # Compression section: UINT8 AlgID, UINT24 DecompSz, followed by data
            alg, dsz = body[0], body[1] | (body[2] << 8) | (body[3] << 16)
            comp_data = body[4:]
            if alg != 0x01:
                raise NotImplementedError("Only LZMA compression supported")
            self.payload = lzma.decompress(comp_data)
            logging.debug(f"    Section@0x{self._file_offset:X}: compressed LZMA {len(comp_data)}→{len(self.payload)}")
        else:
            self.payload = body
            logging.debug(f"    Section@0x{self._file_offset:X}: type={self.type}, size={len(self.payload)}")

    def size_padded(self, align=4):
        return align_up(self.size, align)

    def type_name(self):
        names = {
            0x01: "Compression",
            0x10: "PE32",
            0x11: "PIC",
            0x20: "Version",
            0x24: "GUID-defined",
        }
        return names.get(self.type, f"Unknown(0x{self.type:X})")

    def get_data(self) -> bytes:
        return self.payload
