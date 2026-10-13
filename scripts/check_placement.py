#!/usr/bin/env python3
"""Check ELF object ranges against the RP2040 linker allocations.

This checks addresses, not C volatile/atomic semantics, races, or runtime heap
usage. Scratch RAM is ordinary SRAM; RP2040 has no data-cache coherence issue.
"""
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class Symbol:
    name: str
    address: int
    size: int
    kind: str


def parse_symbols(output):
    symbols = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) not in (3, 4):
            continue
        try:
            address = int(parts[0], 16)
            size = int(parts[1], 16) if len(parts) == 4 else 0
        except ValueError:
            continue
        symbols[parts[-1]] = Symbol(parts[-1], address, size, parts[-2])
    return symbols


def overlaps(address, size, start, end):
    return size > 0 and address < end and start < address + size


def analyze(symbols, name):
    if name not in symbols:
        return [f"Symbol {name!r} not found"], []
    symbol = symbols[name]
    errors, notes = [], []
    if symbol.size == 0:
        return [], ["Linker marker or unknown object size; no span check possible."]
    end = symbol.address + symbol.size
    if symbol.kind.lower() in ("b", "d", "s") and not (0x20000000 <= symbol.address < end <= 0x20042000):
        errors.append("Writable object is outside RP2040 SRAM")

    ranges = []
    heap_base = symbols.get("__HeapBase") or symbols.get("__end__") or symbols.get("_heap_start")
    heap_limit = symbols.get("__HeapLimit") or symbols.get("_heap_end")
    if heap_base and heap_limit:
        ranges.append(("C heap", heap_base.address, heap_limit.address, None))
    for lower, upper, label, owner in (
        ("__StackBottom", "__StackTop", "core 0 stack", None),
        ("__StackOneBottom", "__StackOneTop", "core 1 stack", "core1_stack"),
    ):
        if lower in symbols and upper in symbols:
            ranges.append((label, symbols[lower].address, symbols[upper].address, owner))
    heap = symbols.get("ucHeap")
    if heap and heap.size:
        ranges.append(("FreeRTOS heap", heap.address, heap.address + heap.size, "ucHeap"))
    for label, start, limit, owner in ranges:
        if name != owner and overlaps(symbol.address, symbol.size, start, limit):
            errors.append(f"Object overlaps {label}: 0x{start:08x}–0x{limit:08x}")
    if not heap_base or not heap_limit:
        notes.append("C heap boundaries unavailable; that check was skipped.")
    if not heap or not heap.size:
        notes.append("FreeRTOS heap size unavailable; that check was skipped.")
    return errors, notes


def main():
    if len(sys.argv) != 3:
        print("Usage: check_placement.py <elf_file> <symbol_name>", file=sys.stderr)
        return 2
    result = subprocess.run(["arm-none-eabi-nm", "-S", sys.argv[1]], text=True, capture_output=True, check=True)
    symbols = parse_symbols(result.stdout)
    errors, notes = analyze(symbols, sys.argv[2])
    symbol = symbols.get(sys.argv[2])
    if symbol:
        print(f"{symbol.name}: address=0x{symbol.address:08x}, size={symbol.size}, type={symbol.kind}")
    for error in errors:
        print("ERROR:", error)
    for note in notes:
        print("NOTE:", note)
    print("Address checks only; this does not establish concurrency or runtime memory safety.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
