#!/usr/bin/env python3
"""
parse_uefi.py

Command-line tool to unpack a UEFI firmware volume blob into
individual FFS files and sections.
"""

import os
import sys
import argparse
import logging
from firmware_utils import FirmwareVolume

def main():
    parser = argparse.ArgumentParser(
        description="Extract FFS files & sections from UEFI blob"
    )
    parser.add_argument("blob", help="path to uefi_blob.bin")
    parser.add_argument("-o", "--outdir", default="fv_out",
                        help="directory to write extracted files")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="verbose logging")
    args = parser.parse_args()

    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(format="%(levelname)s: %(message)s", level=level)

    # Read the blob
    with open(args.blob, "rb") as f:
        data = f.read()

    fv = FirmwareVolume(data)
    try:
        fv.parse()
    except Exception as e:
        logging.error(f"Failed to parse FV: {e}")
        sys.exit(1)

    os.makedirs(args.outdir, exist_ok=True)

    for idx, ffs in enumerate(fv.files):
        # Dump raw file
        fname = f"file_{idx:02d}_{ffs.guid}.ffs"
        path = os.path.join(args.outdir, fname)
        with open(path, "wb") as of:
            of.write(ffs._raw[:ffs.size])
        logging.info(f"Extracted FFS: {path}")

        # Dump sections
        for sidx, sec in enumerate(ffs.sections):
            secname = f"file_{idx:02d}_sec_{sidx:02d}_{sec.type_name()}.bin"
            spath = os.path.join(args.outdir, secname)
            with open(spath, "wb") as sf:
                sf.write(sec.get_data())
            logging.info(f"  └─ section: {spath}")

    logging.info("Extraction complete.")

if __name__ == "__main__":
    main()
