"""
Turns the addresses on a -vkdiag screen (or in Diag.txt) back into function names.

    python diag_resolve.py <base> <addr> [addr ...] [-region P|E|J|K]

<base> is the "base" value printed by the diagnostic build: the runtime address of
Pulsar::Diag::Start. Code.pul is loaded wherever the heap puts it, so every other address is
resolved relative to that. Addresses that are not inside Code.pul belong to the game itself.
"""
import os
import sys

ANCHOR = "Start__Q26Pulsar4DiagFv"


def load_map(path):
    symbols = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) != 3:
                continue
            try:
                symbols.append((int(parts[0], 16), int(parts[1], 16), parts[2]))
            except ValueError:
                continue
    symbols.sort()
    return symbols


def main():
    args = sys.argv[1:]
    region = "P"
    if "-region" in args:
        i = args.index("-region")
        region = args[i + 1].upper()
        del args[i:i + 2]
    if len(args) < 2:
        print(__doc__)
        return 1

    here = os.path.dirname(os.path.abspath(__file__))
    symbols = load_map(os.path.join(here, "build", "diag", f"Code.{region}.map"))
    anchor = next(offset for offset, _, name in symbols if name == ANCHOR)
    text = int(args[0], 16) - anchor
    end = max(offset + size for offset, size, _ in symbols)

    for raw in args[1:]:
        address = int(raw, 16)
        offset = address - text
        if not 0 <= offset < end:
            print(f"{address:08X}  game code (not Code.pul)")
            continue
        best = None
        for start, size, name in symbols:
            if start <= offset < start + max(size, 1):
                best = (start, name)
        if best is None:
            print(f"{address:08X}  Code.pul +{offset:X} (no symbol)")
        else:
            print(f"{address:08X}  {best[1]} +0x{offset - best[0]:X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
