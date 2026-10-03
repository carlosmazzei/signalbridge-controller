"""Regression tests for analysis configuration and fail-closed CI scripts."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("prepare_cppcheck", ROOT / "scripts/prepare_cppcheck.py")
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class AnalysisTools(unittest.TestCase):
    def test_preserve_quoted_system_include(self):
        entry = {"directory": "/repo", "file": "/repo/src/a.c",
                 "command": 'cc -isystem "/sdk with spaces" -c src/a.c'}
        result = prepare.normalize(entry, "/gcc/include")
        self.assertEqual(result["arguments"][1:3], ["-I", "/sdk with spaces"])
        self.assertNotIn("command", result)
        self.assertEqual(result["arguments"][-1], "-I/gcc/include")

    def test_analysis_scripts_reject_analyzer_errors(self):
        import os
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "build-release").mkdir()
            (root / "build-release/compile_commands.json").write_text("[]")
            for name in ("run_cppcheck.sh", "cppcheck_simple.sh", "prepare_cppcheck.py"):
                shutil.copy(ROOT / "scripts" / name, root / "scripts" / name)
            for name in (".cppcheck_config", ".cppcheck_suppressions", ".cppcheck_advisory", "misra.json", "misra.txt"):
                shutil.copy(ROOT / name, root / name)
            (root / "bin").mkdir()
            addon = root / "share/cppcheck/addons"
            addon.mkdir(parents=True)
            (addon / "misra.py").write_text("")
            fake = root / "bin/cppcheck"
            fake.write_text('#!/bin/sh\nif [ "$1" = "--version" ]; then echo "Cppcheck test"; exit 0; fi\n'
                            'for arg in "$@"; do if [ "$arg" = "--error-exitcode=2" ]; then exit 2; fi; done\nexit 0\n')
            fake.chmod(0o755)
            env = dict(os.environ, PATH=str(root / "bin") + os.pathsep + os.environ["PATH"])
            env.pop("CPPCHECK_COMPILE_COMMANDS", None)
            for script in ("run_cppcheck.sh", "cppcheck_simple.sh"):
                result = subprocess.run(["bash", str(root / "scripts" / script)],
                                        cwd=root, env=env, text=True, capture_output=True)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertNotIn("passed successfully", result.stdout)
                self.assertNotIn("analysis completed", result.stdout)


class PlacementTools(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import sys
        spec = importlib.util.spec_from_file_location("placement", ROOT / "scripts/check_placement.py")
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def test_uses_full_freertos_heap_and_object_span(self):
        symbols = self.module.parse_symbols("20004000 00020000 b ucHeap\n2001f000 00000004 b object\n")
        errors, _ = self.module.analyze(symbols, "object")
        self.assertTrue(any("FreeRTOS" in error for error in errors))
        self.assertEqual(self.module.analyze(symbols, "ucHeap")[0], [])
        self.assertTrue(self.module.overlaps(0x1ff0, 32, 0x2000, 0x3000))

    def test_readonly_flash_and_unaligned_bytes_are_valid(self):
        symbols = self.module.parse_symbols("10000101 00000003 r table\n20000001 00000001 b flag\n")
        self.assertEqual(self.module.analyze(symbols, "table")[0], [])
        self.assertEqual(self.module.analyze(symbols, "flag")[0], [])

    def test_pico_heap_markers(self):
        symbols = self.module.parse_symbols("20025000 B __end__\n20040000 B __HeapLimit\n20025010 00000004 b object\n")
        self.assertTrue(any("C heap" in error for error in self.module.analyze(symbols, "object")[0]))


if __name__ == "__main__":
    unittest.main()
