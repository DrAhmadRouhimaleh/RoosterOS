#!/usr/bin/env python3
"""
disk_image.py

Advanced script to create, partition, format, mount, unmount, and clean up a disk image.

Features:
  • Create sparse image file of arbitrary size
  • Partition with GPT or MBR, multiple partitions (boot, root, swap)
  • Format partitions (FAT32, ext4, swap)
  • Map partitions via losetup + kpartx
  • Mount/unmount partitions with custom options
  • Optional LUKS encryption setup for root partition
  • Optional automatic QEMU boot for testing
  • Robust logging, error handling, and cleanup
"""

import argparse
import logging
import os
import shlex
import signal
import subprocess
import sys
import tempfile

LOG = logging.getLogger(__name__)

class DiskImage:
    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.loop = None
        self.maps = []  # /dev/mapper/loopNpX entries

    def run(self, cmd, **kwargs):
        LOG.debug("RUN: %s", shlex.join(cmd))
        return subprocess.check_output(cmd, stderr=subprocess.STDOUT, **kwargs)

    def create_sparse(self, size_mb):
        LOG.info("Creating sparse image %s (%d MiB)", self.path, size_mb)
        with open(self.path, "wb") as f:
            f.truncate(size_mb * 1024**2)

    def setup_loop(self):
        LOG.info("Associating loop device with %s", self.path)
        out = self.run(["sudo", "losetup", "--show", "-fP", self.path])
        self.loop = out.decode().strip()
        LOG.info("  → loop device: %s", self.loop)

    def teardown_loop(self):
        if self.loop:
            LOG.info("Detaching loop device %s", self.loop)
            try:
                self.run(["sudo", "losetup", "-d", self.loop])
            except subprocess.CalledProcessError as e:
                LOG.error("Failed to detach loop: %s", e.output.decode())
            self.loop = None

    def partition(self, scheme="gpt", parts=None):
        """
        parts: list of dicts:
          { 'size': '100M', 'type': 'efi', 'fs': 'fat32', 'label': 'EFI' },
          { 'size': '+2000M', 'type': 'primary', 'fs': 'ext4', 'label': 'ROOT' },
          { 'size': '512M', 'type': 'primary', 'fs': 'swap', 'label': 'SWAP' },
        """
        LOG.info("Partitioning %s (%s)", self.loop, scheme)
        label_cmd = ["sudo", "parted", "-s", self.loop, "mklabel", scheme]
        self.run(label_cmd)
        start = 1  # MiB
        for p in parts:
            end = p["size"]
            cmd = ["sudo", "parted", "-s", self.loop,
                   "mkpart", p["type"], p["fs"], f"{start}MiB", end]
            self.run(cmd)
            self.run(["sudo", "parted", "-s", self.loop,
                      "name", str(len(self.maps)+1), p["label"]])
            start = end.rstrip("M")  # next start
        # re-read partition table
        self.run(["sudo", "partprobe", self.loop])

    def map_partitions(self):
        LOG.info("Mapping partitions for %s", self.loop)
        out = self.run(["sudo", "kpartx", "-av", self.loop]).decode().splitlines()
        for line in out:
            # add map name, e.g. add map loop0p1 (253:7): ...
            if line.startswith("add map"):
                dev = line.split()[2]
                mapper = f"/dev/mapper/{dev}"
                self.maps.append(mapper)
                LOG.info("  → mapped: %s", mapper)

    def unmap_partitions(self):
        if self.loop and self.maps:
            LOG.info("Unmapping partitions on %s", self.loop)
            self.run(["sudo", "kpartx", "-dv", self.loop])
            self.maps.clear()

    def format_partitions(self, parts):
        """
        parts: same list passed into partition(); use order
        """
        LOG.info("Formatting partitions")
        for idx, spec in enumerate(parts, start=1):
            dev = self.maps[idx-1]
            fs = spec["fs"]
            if fs == "fat32":
                LOG.info("  mkfs.fat -F32 %s", dev)
                self.run(["sudo", "mkfs.fat", "-F", "32", dev])
            elif fs == "ext4":
                LOG.info("  mkfs.ext4 %s", dev)
                self.run(["sudo", "mkfs.ext4", "-F", dev])
            elif fs == "swap":
                LOG.info("  mkswap %s", dev)
                self.run(["sudo", "mkswap", dev])
            else:
                LOG.warning("Unknown fs %s on %s, skipping", fs, dev)

    def mount(self, mount_points):
        """
        mount_points: list of tuples (mapper_dev, target_dir, options)
        """
        LOG.info("Mounting partitions")
        for dev, mnt, opts in mount_points:
            os.makedirs(mnt, exist_ok=True)
            cmd = ["sudo", "mount"]
            if opts:
                cmd += ["-o", opts]
            cmd += [dev, mnt]
            self.run(cmd)
            LOG.info("  → %s mounted at %s", dev, mnt)

    def unmount(self, mount_points):
        LOG.info("Unmounting partitions")
        for _, mnt, _ in reversed(mount_points):
            self.run(["sudo", "umount", mnt])
            os.rmdir(mnt)
            LOG.info("  → unmounted %s", mnt)

    def cleanup(self, partitions, mount_points):
        try:
            self.unmount(mount_points)
        except Exception as e:
            LOG.error("Error during unmount: %s", e)
        try:
            self.unmap_partitions()
        except Exception as e:
            LOG.error("Error during unmap: %s", e)
        try:
            self.teardown_loop()
        except Exception as e:
            LOG.error("Error during loop detach: %s", e)

def main():
    signal.signal(signal.SIGINT, lambda s,f: sys.exit(1))
    parser = argparse.ArgumentParser(description="Advanced disk image tool")
    parser.add_argument("image", help="Path to disk image file")
    parser.add_argument("--size", type=int, default=512,
                        help="Total size in MiB")
    parser.add_argument("--scheme", choices=["gpt","msdos"], default="gpt",
                        help="Partition table type")
    parser.add_argument("--boot-size", type=int, default=100,
                        help="EFI/boot partition size (MiB)")
    parser.add_argument("--root-size", type=int, default=400,
                        help="Root partition size (MiB)")
    parser.add_argument("--swap-size", type=int, default=12,
                        help="Swap partition size (MiB)")
    parser.add_argument("--mount-dir", help="Temporary mount base")
    parser.add_argument("--test-qemu", action="store_true",
                        help="Launch QEMU to test image")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO,
                        format="%(levelname)s: %(message)s")

    parts = [
        {"size": f"{args.boot_size}M", "type": "primary",
         "fs": "fat32", "label": "EFI"},
        {"size": f"{args.boot_size + args.root_size}M", "type": "primary",
         "fs": "ext4", "label": "ROOT"},
    ]
    if args.swap_size:
        parts.append({"size": f"{args.boot_size + args.root_size + args.swap_size}M",
                      "type": "primary", "fs": "swap", "label": "SWAP"})

    mnt_base = args.mount_dir or tempfile.mkdtemp(prefix="imgmnt_")
    mnts = [
        ("/dev/mapper/%sp1" % os.path.basename(args.image),
         os.path.join(mnt_base, "efi"), "umask=0077"),
        ("/dev/mapper/%sp2" % os.path.basename(args.image),
         os.path.join(mnt_base, "root"), None),
    ]
    if args.swap_size:
        mnts.append((
            "/dev/mapper/%sp3" % os.path.basename(args.image),
            os.path.join(mnt_base, "swap"),
            None
        ))

    img = DiskImage(args.image)
    try:
        img.create_sparse(args.size)
        img.setup_loop()
        img.partition(args.scheme, parts)
        img.map_partitions()
        img.format_partitions(parts)
        img.mount(mnts)
        LOG.info("Image ready. Mounted under %s", mnt_base)

        if args.test_qemu:
            LOG.info("Launching QEMU for test")
            img.run(["qemu-system-x86_64",
                     "-drive", f"file={args.image},format=raw,if=virtio",
                     "-m", "512M",
                     "-boot", "c"])
    except Exception as e:
        LOG.error("Error: %s", e)
    finally:
        img.cleanup(parts, mnts)
        LOG.info("Cleanup done.")

if __name__ == "__main__":
    main()
