#!/usr/bin/env python3
"""Preserve SDK system includes in Cppcheck's compilation database import."""
import json
from pathlib import Path
import shlex
import subprocess
import sys


def normalize(entry, compiler_include):
    args = entry.get("arguments") or shlex.split(entry["command"])
    args = ["-I" if arg == "-isystem" else arg for arg in args]
    args.append("-I" + compiler_include)
    result = dict(entry, arguments=args)
    result.pop("command", None)
    return result


def main():
    source, destination = map(Path, sys.argv[1:])
    entries = json.loads(source.read_text())
    includes = {}
    output = []
    for entry in entries:
        args = entry.get("arguments") or shlex.split(entry["command"])
        compiler = args[0]
        if compiler not in includes:
            includes[compiler] = subprocess.check_output(
                [compiler, "-print-file-name=include"], text=True).strip()
        output.append(normalize(entry, includes[compiler]))
    destination.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
