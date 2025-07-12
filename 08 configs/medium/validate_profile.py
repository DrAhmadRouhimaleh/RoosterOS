#!/usr/bin/env python3
"""
validate_profile.py

Advanced TOML validator for RoosterOS hardware profiles.

Features:
  • Checks required keys and types
  • Cross-field constraints: min ≤ max, sensible ranges
  • Parses gfx_min:
      – "Vendor WxH" → width/height resolution
      – "Vendor Driver" → feature‐string
  • Default cpu_max_mhz = cpu_min_mhz if missing
  • Validates package names against regex
  • Rich error messages with dotted paths
"""

import argparse
import re
import sys
from typing import Any, Dict, List, Tuple

import toml

# Allowed sets
EDITIONS    = {"low", "medium", "high", "server"}
ARCHES      = {"x86", "x86_64", "arm", "arm64", "riscv"}
PKG_RE      = re.compile(r"^[a-z0-9][a-z0-9_\-]*$")
RES_RE      = re.compile(r"^(\d+)x(\d+)$")

# Error helper
def err(msg: str):
    print("ERROR:", msg, file=sys.stderr)

def parse_gfx(s: str) -> Tuple[Dict[str,Any], str]:
    parts = s.split(" ", 1)
    if len(parts) != 2:
        return {}, f"gfx_min '{s}' must be 'Vendor WxH' or 'Vendor Feature'"
    vendor, spec = parts
    m = RES_RE.match(spec)
    if m:
        w, h = map(int, m.groups())
        if w < 320 or h < 200:
            return {}, f"resolution {w}x{h} too small (min 320x200)"
        return {"vendor": vendor, "width": w, "height": h}, ""
    # treat as vendor + feature string
    feature = spec.strip()
    if not feature:
        return {}, f"gfx_min feature missing after vendor"
    return {"vendor": vendor, "feature": feature}, ""

def validate(cfg: Dict[str,Any]) -> bool:
    ok = True

    # edition
    ed = cfg.get("edition")
    if ed not in EDITIONS:
        err(f"edition='{ed}' invalid; must be one of {sorted(EDITIONS)}")
        ok = False

    # arch
    ar = cfg.get("arch")
    if ar not in ARCHES:
        err(f"arch='{ar}' invalid; must be one of {sorted(ARCHES)}")
        ok = False

    # ram
    rmin = cfg.get("ram_min_mb")
    rmax = cfg.get("ram_max_mb")
    if not isinstance(rmin, int) or not isinstance(rmax, int):
        err("ram_min_mb and ram_max_mb must be integers")
        ok = False
    else:
        if rmin < 16 or rmax > 65536 or rmin > rmax:
            err(f"ram range invalid: {rmin}–{rmax} (allowed 16–65536, min ≤ max)")
            ok = False

    # cpu
    cmin = cfg.get("cpu_min_mhz")
    cmax = cfg.get("cpu_max_mhz", cmin)
    if not isinstance(cmin, int):
        err("cpu_min_mhz must be integer")
        ok = False
    if "cpu_max_mhz" in cfg and not isinstance(cfg["cpu_max_mhz"], int):
        err("cpu_max_mhz must be integer if provided")
        ok = False
    if isinstance(cmin, int) and isinstance(cmax, int):
        if cmin < 100 or cmax > 10000 or cmin > cmax:
            err(f"cpu range invalid: {cmin}–{cmax} (allowed 100–10000, min ≤ max)")
            ok = False

    # gfx_min
    gfx = cfg.get("gfx_min")
    if not isinstance(gfx, str):
        err("gfx_min must be a string")
        ok = False
    else:
        parsed, gerr = parse_gfx(gfx)
        if gerr:
            err(gerr)
            ok = False

    # disk
    dmin = cfg.get("disk_min_gb")
    if not (isinstance(dmin, int) or isinstance(dmin, float)) or dmin < 1:
        err(f"disk_min_gb='{dmin}' invalid; must be ≥1")
        ok = False

    # internet
    net = cfg.get("internet")
    if not isinstance(net, bool):
        err("internet must be boolean")
        ok = False

    # packages
    pkgs = cfg.get("packages")
    if not isinstance(pkgs, list) or not pkgs:
        err("packages must be a non-empty list of strings")
        ok = False
    else:
        for p in pkgs:
            if not isinstance(p, str) or not PKG_RE.match(p):
                err(f"package '{p}' invalid; must match {PKG_RE.pattern}")
                ok = False

    return ok

def main():
    parser = argparse.ArgumentParser(description="Validate RoosterOS profile")
    parser.add_argument("file", help="TOML profile path")
    args = parser.parse_args()

    try:
        cfg = toml.load(args.file)
    except Exception as e:
        err(f"parsing TOML failed: {e}")
        sys.exit(1)

    if validate(cfg):
        print("✔ Profile valid")
        sys.exit(0)
    else:
        sys.exit(2)

if __name__ == "__main__":
    main()
