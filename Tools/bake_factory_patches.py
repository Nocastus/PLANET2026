"""
Bake the factory patch bank into ISHTAR.

Scans FactoryPatches/<NN Name>.md (flat - the 2-digit filename prefix sets
the bank order; 01 is the patch a fresh instance opens with) and generates
Source/FactoryPatchData.h containing every patch as an escaped C string
literal. Rebuild the plugin after running this and the bank ships inside
the binary - no files, no installer steps, read-only by construction.

Usage:  py Tools/bake_factory_patches.py
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "FactoryPatches")
OUT_PATH = os.path.join(ROOT, "Source", "FactoryPatchData.h")


def c_escape(text):
    """Escape UTF-8 text as a C string literal body, split into safe chunks.
    Octal escapes are always 3 digits, so a following digit can't extend them."""
    out = []
    for b in text.encode("utf-8"):
        c = chr(b)
        if c == "\\":
            out.append("\\\\")
        elif c == '"':
            out.append('\\"')
        elif c == "\n":
            out.append("\\n")
        elif c == "\r":
            continue
        elif c == "\t":
            out.append("\\t")
        elif 32 <= b <= 126:
            out.append(c)
        else:
            out.append("\\%03o" % b)
    s = "".join(out)
    # split into adjacent literals well under MSVC's single-literal limit
    chunks = []
    limit = 4000
    while s:
        cut = limit
        # never split in the middle of an escape sequence
        while cut < len(s) and s[max(0, cut - 4):cut].count("\\") % 2 == 1:
            cut += 1
        chunks.append(s[:cut])
        s = s[cut:]
    return "\n        ".join('"%s"' % c for c in chunks)


def main():
    if not os.path.isdir(SRC_DIR):
        print(f"ERROR: {SRC_DIR} not found")
        return 1

    entries = []
    for fn in sorted(os.listdir(SRC_DIR)):
        if not fn.lower().endswith(".md") or fn.lower() == "readme.md":
            continue
        with open(os.path.join(SRC_DIR, fn), "r", encoding="utf-8") as fh:
            content = fh.read()
        entries.append(("Factory", fn, content))

    if not entries:
        print("ERROR: no .md patches found under FactoryPatches/")
        return 1

    lines = []
    ap = lines.append
    ap("// AUTO-GENERATED FILE - DO NOT EDIT BY HAND.")
    ap("// Source of truth: FactoryPatches/<NN Name>.md (prefix = bank order, 01 loads at startup)")
    ap("// Regenerate with:  py Tools/bake_factory_patches.py   then rebuild.")
    ap("#pragma once")
    ap("")
    ap("namespace FactoryPatchData")
    ap("{")
    ap("    struct Entry")
    ap("    {")
    ap("        const char* category;   // from the FactoryPatches subfolder name")
    ap("        const char* markdown;   // full patch file contents (UTF-8)")
    ap("    };")
    ap("")
    ap("    static const Entry entries[] =")
    ap("    {")
    for category, fn, content in entries:
        ap(f"        // {category}/{fn}")
        ap(f"        {{ {c_escape(category)},")
        ap(f"        {c_escape(content)} }},")
        ap("")
    ap("    };")
    ap("")
    ap("    static constexpr int numEntries = (int) (sizeof (entries) / sizeof (entries[0]));")
    ap("}")

    with open(OUT_PATH, "w", encoding="ascii", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")

    print(f"Baked {len(entries)} patch(es) into {OUT_PATH}:")
    for category, fn, _ in entries:
        print(f"  [{category}] {fn}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
