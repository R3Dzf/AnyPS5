import base64
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPLAY = Path(sys.argv.pop(1)).resolve()
REQUEST = (
    "NVNQQQ0AAAAAABAAAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAIAAAAAAAAAAAAAAAAAAAAAEB"
    "AAAAAQAAAAEAAAAAAAAAAAAAAAEAAAAAAAEAAAAAAAAAADAAAAAAAAAIAAAAAAAAAAECAwQFBgcIAAAgQAAA"
    "AwEAIAAAAAAAAAAGAAAAAAAAAAEAAAALAAAAPQAAAEAAAABBAAAAPwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
)


class ShaderReplayIoTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="anyps5-shader-replay-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.request = self.directory / "valid.req"
        self.request.write_text(REQUEST, encoding="ascii")

    def run_replay(self, *arguments):
        result = subprocess.run(
            [str(REPLAY), *map(str, arguments)],
            cwd=self.directory,
            env={**os.environ, "ANYPS5_NO_SHADER_CACHE": "1"},
            capture_output=True,
            text=True,
            timeout=20,
        )
        return result.returncode, result.stdout + result.stderr

    def test_complete_outputs(self):
        status, output = self.run_replay("--spv", "--code", "--mem", self.request)
        self.assertEqual(status, 0, output)
        self.assertIn("recompiled:", output)
        spirv = (self.directory / "valid.req.spv").read_bytes()
        self.assertGreater(len(spirv), 20)
        self.assertEqual(len(spirv) % 4, 0)
        self.assertEqual(struct.unpack_from("<I", spirv)[0], 0x07230203)
        self.assertEqual((self.directory / "valid.req.code").read_bytes(), struct.pack("<I", 0xBF810000))
        self.assertEqual((self.directory / "mem_1000_3000.bin").read_bytes(), bytes(range(1, 9)))

    def test_output_open_failures(self):
        for option, filename in (
            ("--spv", "valid.req.spv"),
            ("--code", "valid.req.code"),
            ("--mem", "mem_1000_3000.bin"),
        ):
            with self.subTest(option=option):
                path = self.directory / filename
                path.mkdir()
                status, output = self.run_replay(option, self.request)
                self.assertEqual(status, 1, output)
                self.assertIn("could not open output", output)
                self.assertTrue(path.is_dir())
                path.rmdir()
                status, output = self.run_replay(option, self.request)
                self.assertEqual(status, 0, output)
                self.assertGreater(path.stat().st_size, 0)

    @unittest.skipUnless(sys.platform.startswith("linux"), "/dev/full is Linux-only")
    def test_output_close_failure(self):
        path = self.directory / "valid.req.spv"
        path.symlink_to("/dev/full")
        status, output = self.run_replay("--spv", self.request)
        self.assertEqual(status, 1, output)
        self.assertIn("could not write complete output", output)
        self.assertFalse(path.exists())
        status, output = self.run_replay("--spv", self.request)
        self.assertEqual(status, 0, output)
        self.assertGreater(path.stat().st_size, 20)

    def test_missing_input(self):
        status, output = self.run_replay(self.directory / "missing.req")
        self.assertEqual(status, 1, output)
        self.assertIn("could not open request file", output)

    def test_truncated_request(self):
        self.request.write_text(REQUEST[:32], encoding="ascii")
        status, output = self.run_replay(self.request)
        self.assertEqual(status, 1, output)
        self.assertIn("truncated data", output)

    def test_unsupported_instruction(self):
        raw = base64.b64decode(REQUEST)
        code = struct.pack("<I", 0xBF810000)
        self.assertEqual(raw.count(code), 1)
        self.request.write_text(base64.b64encode(raw.replace(code, b"\xff" * 4)).decode("ascii"), encoding="ascii")
        status, output = self.run_replay("--spv", self.request)
        self.assertEqual(status, 1, output)
        self.assertIn("unknown RDNA instruction family", output)
        self.assertFalse((self.directory / "valid.req.spv").exists())

    def test_options_without_request(self):
        status, output = self.run_replay("--spv", "--mem")
        self.assertEqual(status, 2, output)
        self.assertIn("no request files supplied", output)

    @unittest.skipUnless(sys.platform == "win32", "Windows runtime bundle")
    def test_windows_runtime_bundle(self):
        bundle = self.directory / "replay"
        bundle.mkdir()
        for name in (REPLAY.name, "libc.prx", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
            shutil.copy2(REPLAY.parent / name, bundle / name)
        environment = {**os.environ, "ANYPS5_NO_SHADER_CACHE": "1"}
        environment["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
        result = subprocess.run(
            [str(bundle / REPLAY.name), "--spv", str(self.request)],
            cwd=self.directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertGreater((self.directory / "valid.req.spv").stat().st_size, 20)


if __name__ == "__main__":
    unittest.main()
