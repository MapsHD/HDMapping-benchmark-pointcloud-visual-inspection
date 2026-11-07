#!/usr/bin/env python3

import hashlib
import sys
from pathlib import Path

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

EXIT_CODE_IDENTICAL = 0
EXIT_CODE_DIFFERENT = 1
EXIT_CODE_ERROR = 2


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def pixel_report(a: Path, b: Path) -> str:
    ia, ib = Image.open(a), Image.open(b)
    if ia.size != ib.size or ia.mode != ib.mode:
        return f"{ia.size} {ia.mode} vs {ib.size} {ib.mode}"
    pa, pb = np.asarray(ia), np.asarray(ib)
    delta = np.maximum(pa, pb) - np.minimum(pa, pb)
    changed = delta.any(axis=-1) if delta.ndim == 3 else delta > 0
    return f"{int(changed.sum())} of {changed.size} pixels differ, max channel difference {int(delta.max())}"


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {Path(sys.argv[0]).name} <old output dir> <new output dir>")
        return EXIT_CODE_ERROR

    old_dir, new_dir = Path(sys.argv[1]), Path(sys.argv[2])
    for directory in (old_dir, new_dir):
        if not directory.is_dir():
            print(f"ERROR: {directory} is not a directory")
            return EXIT_CODE_ERROR

    old = {p.name for p in old_dir.glob("*.png")}
    new = {p.name for p in new_dir.glob("*.png")}

    problems = 0
    for name in sorted(old - new):
        print(f"only in {old_dir}: {name}")
        problems += 1
    for name in sorted(new - old):
        print(f"only in {new_dir}: {name}")
        problems += 1

    identical = 0
    for name in sorted(old & new):
        if digest(old_dir / name) == digest(new_dir / name):
            identical += 1
            continue
        problems += 1
        print(f"{name}: {pixel_report(old_dir / name, new_dir / name)}")

    print(f"{identical} identical, {problems} differing or missing")
    return EXIT_CODE_DIFFERENT if problems else EXIT_CODE_IDENTICAL


if __name__ == "__main__":
    sys.exit(main())
