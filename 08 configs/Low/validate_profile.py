#!usrbinenv python3

validate_profile.py

Validate RoosterOS hardware profile TOML
  • Field existence and types
  • Cross-field constraints (min ≤ max)
  • gfx_min parsing (“VENDOR WxH”)
  • Package name regex


import argparse, sys, re
from typing import Any, Dict, List
import toml

# Allowed sets
EDITIONS = {low, medium, high, server}
ARCHES   = {x86, x86_64, arm, arm64, riscv}
PKG_RE   = re.compile(r^[a-z0-9-]+$)

def error(msg str)
    print(ERROR, msg, file=sys.stderr)

def validate(cfg Dict[str, Any]) - bool
    ok = True
    # edition
    ed = cfg.get(edition)
    if not isinstance(ed, str) or ed not in EDITIONS
        error(fedition='{ed}' invalid (must be one of {sorted(EDITIONS)}))
        ok = False

    # arch
    ar = cfg.get(arch)
    if not isinstance(ar, str) or ar not in ARCHES
        error(farch='{ar}' invalid (must be one of {sorted(ARCHES)}))
        ok = False

    # ram
    rmin = cfg.get(ram_min_mb)
    rmax = cfg.get(ram_max_mb)
    if not isinstance(rmin, int) or not isinstance(rmax, int)
        error(ram_min_mb and ram_max_mb must be integers)
        ok = False
    else
        if rmin  16 or rmax  32768 or rmin  rmax
            error(fram range invalid {rmin}–{rmax} (allowed 16–32768, min ≤ max))
            ok = False

    # cpu
    cmin = cfg.get(cpu_min_mhz)
    cmax = cfg.get(cpu_max_mhz)
    if not isinstance(cmin, int) or not isinstance(cmax, int)
        error(cpu_min_mhz and cpu_max_mhz must be integers)
        ok = False
    else
        if cmin  50 or cmax  5000 or cmin  cmax
            error(fcpu range invalid {cmin}–{cmax} (allowed 50–5000, min ≤ max))
            ok = False

    # gfx_min
    gfx = cfg.get(gfx_min)
    if not isinstance(gfx, str)
        error(gfx_min must be a string)
        ok = False
    else
        parts = gfx.split()
        if len(parts) != 2 or 'x' not in parts[1]
            error(fgfx_min='{gfx}' format invalid (expected 'VENDOR WxH'))
            ok = False
        else
            w,h = parts[1].split('x',1)
            try
                w,h = int(w), int(h)
                if w320 or h200
                    error(fgfx resolution too small {w}x{h} (min 320x200))
                    ok = False
            except
                error(fgfx_min resolution not integers '{parts[1]}')
                ok = False

    # disk
    dmin = cfg.get(disk_min_gb)
    if not isinstance(dmin, (int,float)) or dmin  1
        error(fdisk_min_gb='{dmin}' invalid (must be ≥1))
        ok = False

    # internet
    net = cfg.get(internet)
    if not isinstance(net, bool)
        error(finternet='{net}' must be boolean)
        ok = False

    # packages
    pkgs = cfg.get(packages)
    if not isinstance(pkgs, list) or not pkgs
        error(packages must be a non-empty list)
        ok = False
    else
        for p in pkgs
            if not isinstance(p, str) or not PKG_RE.match(p)
                error(fpackage '{p}' invalid (must match {PKG_RE.pattern}))
                ok = False

    return ok

def main()
    parser = argparse.ArgumentParser()
    parser.add_argument(profile, help=TOML profile path)
    args = parser.parse_args()

    try
        cfg = toml.load(args.profile)
    except Exception as e
        error(fFailed parsing TOML {e})
        sys.exit(1)

    if validate(cfg)
        print(Profile is valid.)
        sys.exit(0)
    else
        sys.exit(2)

if __name__ == __main__
    main()
