#!/usr/bin/env python3
import os
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPLOAD_DIR = os.path.join(ROOT, "upload")
OUTPUT_HEADER = os.path.join(ROOT, "src", "filesystem", "upload", "upload_payload.h")


def should_skip(path):
    name = os.path.basename(path)
    if name.endswith(".cpp") or name.endswith(".h"):
        return True
    return False


def collect_files():
    files = []
    for root, _, filenames in os.walk(UPLOAD_DIR):
        for filename in filenames:
            full = os.path.join(root, filename)
            if should_skip(full):
                continue
            rel = os.path.relpath(full, UPLOAD_DIR)
            files.append((full, rel.replace(os.sep, "/")))
    return files


def sanitize_identifier(path):
    name = path.replace("/", "_").replace(".", "_").replace("-", "_")
    return "upload_" + "".join(ch if ch.isalnum() or ch == "_" else "_" for ch in name)


def main():
    if not os.path.isdir(UPLOAD_DIR):
        print(f"upload dir not found: {UPLOAD_DIR}")
        return 1

    files = collect_files()
    if not files:
        with open(OUTPUT_HEADER, "w", encoding="utf-8") as handle:
            handle.write("#pragma once\n")
            handle.write("#include <stddef.h>\n")
            handle.write("#include <stdint.h>\n\n")
            handle.write("typedef struct {\n")
            handle.write("    const char *path;\n")
            handle.write("    const uint8_t *data;\n")
            handle.write("    size_t size;\n")
            handle.write("} upload_file_t;\n\n")
            handle.write("static const upload_file_t upload_files[] = {};\n")
            handle.write("static const size_t upload_file_count = 0;\n")
        print("No files to package.")
        return 0

    with open(OUTPUT_HEADER, "w", encoding="utf-8") as handle:
        handle.write("#pragma once\n")
        handle.write("#include <stddef.h>\n")
        handle.write("#include <stdint.h>\n\n")
        handle.write("typedef struct {\n")
        handle.write("    const char *path;\n")
        handle.write("    const uint8_t *data;\n")
        handle.write("    size_t size;\n")
        handle.write("} upload_file_t;\n\n")

        for full, rel in files:
            ident = sanitize_identifier(rel)
            with open(full, "rb") as f_in:
                data = f_in.read()
            handle.write(f"static const uint8_t {ident}[] = {{\n")
            for i in range(0, len(data), 12):
                chunk = data[i:i + 12]
                hexes = ", ".join(f"0x{b:02x}" for b in chunk)
                handle.write(f"    {hexes},\n")
            handle.write("};\n\n")

        handle.write("static const upload_file_t upload_files[] = {\n")
        for full, rel in files:
            ident = sanitize_identifier(rel)
            handle.write(f"    {{\"{rel}\", {ident}, sizeof({ident})}},\n")
        handle.write("};\n\n")
        handle.write(f"static const size_t upload_file_count = {len(files)};\n")

    print(f"Packaged {len(files)} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())

