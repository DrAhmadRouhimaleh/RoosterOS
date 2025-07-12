#!/usr/bin/env python3
"""
validate_config.py

Advanced TOML validator for RoosterOS profiles.

Features:
  • Declarative, nested schema with types, ranges, enums
  • Cross-field and conditional validations
  • Detailed, path-specific error reporting
  • Optional JSON-schema export
  • CLI flags: --schema, --verbose
"""

import argparse
import ipaddress
import json
import re
import sys
from typing import Any, Dict, List, Tuple, Union

import toml

# -----------------------------------------------------------------------------
# Schema Definition
# -----------------------------------------------------------------------------
SchemaType = Union[type, Tuple[type, ...], str]  # 'ipv4', 'cidr'

SCHEMA: Dict[str, Dict[str, Any]] = {
    "edition": {
        "type": str,
        "required": True,
        "enum": ["full", "minimal", "server", "desktop"],
        "help": "OS edition",
    },
    "arch": {
        "type": str,
        "required": True,
        "enum": ["x86", "x86_64", "arm", "riscv"],
        "help": "Target architecture",
    },
    "ram_min_mb": {
        "type": int,
        "required": True,
        "min": 16,
        "max": 65536,
        "help": "Minimum RAM (MB)",
    },
    "ram_max_mb": {
        "type": int,
        "required": True,
        "min_key": "ram_min_mb",
        "max": 65536,
        "help": "Maximum RAM (MB), ≥ ram_min_mb",
    },
    "cpu_min_mhz": {
        "type": int,
        "required": True,
        "min": 100,
        "max_key": "cpu_max_mhz",
        "help": "Minimum CPU freq (MHz)",
    },
    "cpu_max_mhz": {
        "type": int,
        "required": False,
        "min_key": "cpu_min_mhz",
        "max": 10000,
        "default": None,
        "help": "Maximum CPU freq (MHz), ≥ cpu_min_mhz",
    },
    "disk_min_gb": {
        "type": (int, float),
        "required": True,
        "min": 1,
        "help": "Minimum disk size (GB)",
    },
    "gfx_min": {
        "type": str,
        "required": True,
        "pattern": r"^[A-Za-z0-9]+ (?:\d+x\d+|[A-Za-z0-9\-]+)$",
        "help": "Graphics minimum, e.g. 'Vendor 800x600' or 'Vendor-Driver'",
    },
    "internet": {
        "type": bool,
        "required": True,
        "help": "Internet required",
    },
    "packages": {
        "type": list,
        "required": True,
        "schema": {
            "_self": {
                "type": str,
                "pattern": r"^[a-z0-9][a-z0-9_\-]*$",
                "help": "Package name",
            }
        },
        "help": "List of required packages",
    },
    "network": {
        "type": dict,
        "required": False,
        "schema": {
            "enabled": {"type": bool, "required": True},
            "type": {
                "type": str,
                "required": True,
                "enum": ["dhcp", "static"],
            },
            "ip":      {"type": "ipv4", "required": False},
            "gateway": {"type": "ipv4", "required": False},
        },
        "help": "Optional network settings",
    },
}

# -----------------------------------------------------------------------------
# Validators
# -----------------------------------------------------------------------------
def validate_value(
    path: str,
    val: Any,
    rule: Dict[str, Any],
    cfg: Dict[str, Any],
) -> List[str]:
    errs: List[str] = []
    expected = rule["type"]
    types: Tuple[type, ...]
    if isinstance(expected, tuple):
        types = expected
    elif isinstance(expected, str):
        types = ()
    else:
        types = (expected,)

    # 1. Type or special checks
    if expected == "ipv4":
        if not isinstance(val, str):
            errs.append(f"{path}: expected IPv4 string, got {type(val).__name__}")
        else:
            try:
                ipaddress.IPv4Address(val)
            except Exception:
                errs.append(f"{path}: invalid IPv4 '{val}'")
        return errs

    if not isinstance(val, types):
        errs.append(f"{path}: expected {types}, got {type(val).__name__}")
        return errs

    # 2. Enum
    if "enum" in rule and val not in rule["enum"]:
        errs.append(f"{path}: '{val}' not in {rule['enum']}")

    # 3. Pattern (regex)
    if "pattern" in rule:
        if not re.match(rule["pattern"], str(val)):
            errs.append(f"{path}: '{val}' does not match /{rule['pattern']}/")

    # 4. Range: literal min/max or keys
    if isinstance(val, (int, float)):
        if "min" in rule and val < rule["min"]:
            errs.append(f"{path}: {val} < {rule['min']}")
        if "max" in rule and val > rule["max"]:
            errs.append(f"{path}: {val} > {rule['max']}")
        # Cross-field min_key / max_key
        if "min_key" in rule:
            other = cfg.get(rule["min_key"])
            if isinstance(other, (int, float)) and val < other:
                errs.append(f"{path}: {val} < {rule['min_key']} ({other})")
        if "max_key" in rule:
            other = cfg.get(rule["max_key"])
            if isinstance(other, (int, float)) and val > other:
                errs.append(f"{path}: {val} > {rule['max_key']} ({other})")

    # 5. Nested dict
    if isinstance(val, dict) and "schema" in rule:
        for k, sub in rule["schema"].items():
            subpath = f"{path}.{k}"
            if sub.get("required", False) and k not in val:
                errs.append(f"{subpath}: missing")
            if k in val:
                errs += validate_value(subpath, val[k], sub, val)

    # 6. List
    if isinstance(val, list) and "schema" in rule:
        subschema = rule["schema"]["_self"]
        for idx, item in enumerate(val):
            item_path = f"{path}[{idx}]"
            errs += validate_value(item_path, item, subschema, cfg)

    return errs

def validate(cfg: Dict[str, Any]) -> Tuple[bool, List[str]]:
    errs: List[str] = []
    # Top-level required
    for key, rule in SCHEMA.items():
        if rule.get("required", False) and key not in cfg:
            errs.append(f"{key}: missing required")

    # Defaults
    for key, rule in SCHEMA.items():
        if key not in cfg and "default" in rule:
            cfg[key] = rule["default"]

    for key, val in cfg.items():
        if key not in SCHEMA:
            errs.append(f"{key}: unexpected")
            continue
        errs += validate_value(key, val, SCHEMA[key], cfg)

    return (not errs, errs)

# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------
def export_schema():
    def norm(r: Dict[str, Any]) -> Dict[str, Any]:
        out = {k: v for k, v in r.items() if k != "schema"}
        if "schema" in r:
            out["schema"] = {k: norm(v) for k, v in r["schema"].items()}
        return out

    print(json.dumps({k: norm(v) for k, v in SCHEMA.items()}, indent=2))
    sys.exit(0)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("file")
    p.add_argument("--schema", action="store_true")
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()

    if args.schema:
        export_schema()

    try:
        cfg = toml.load(args.file)
    except Exception as e:
        print(f"Parse error: {e}", file=sys.stderr)
        sys.exit(2)

    ok, errs = validate(cfg)
    if not ok:
        print("CONFIG INVALID:", file=sys.stderr)
        for e in errs:
            print(" -", e, file=sys.stderr)
        sys.exit(1)

    print("CONFIG VALID.")
    if args.verbose:
        print(json.dumps(cfg, indent=2))
    sys.exit(0)

if __name__ == "__main__":
    main()
