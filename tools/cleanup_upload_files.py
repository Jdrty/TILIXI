#!/usr/bin/env python3
import os
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPLOAD_DIR = os.path.join(ROOT, "upload")


def should_skip(path):
    name = os.path.basename(path)
    if name.endswith(".cpp") or name.endswith(".h"):
        return True
    return False


def main():
    if not os.path.isdir(UPLOAD_DIR):
        print(f"upload dir not found: {UPLOAD_DIR}")
        return 1

    removed = 0
    for root, _, files in os.walk(UPLOAD_DIR):
        for filename in files:
            full = os.path.join(root, filename)
            if should_skip(full):
                continue
            try:
                os.remove(full)
                removed += 1
            except OSError as exc:
                print(f"Failed to remove {full}: {exc}")
                return 1

    print(f"Removed {removed} file(s) from upload folder.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

