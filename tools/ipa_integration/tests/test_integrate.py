import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from integrate_ipa import (
    LC_SEGMENT_64, MH_MAGIC_64, MACH_HEADER_SIZE,
    dylib_command, inject_load_commands, load_command_strings, parse_header,
    rpath_command,
)


class IntegrationToolTests(unittest.TestCase):
    def build_macho(self):
        segment = struct.pack(
            "<II16sQQQQiiII",
            LC_SEGMENT_64, 72 + 80, b"__TEXT", 0, 0x4000, 0, 0x4000,
            5, 5, 1, 0)
        section = struct.pack(
            "<16s16sQQIIIIIIII",
            b"__text", b"__TEXT", 0x100004000, 16, 0x4000,
            0, 0, 0, 0, 0, 0, 0)
        header = struct.pack(
            "<IIIIIIII",
            MH_MAGIC_64, 0x0100000C, 0, 2, 1, 72 + 80, 0, 0)
        binary = bytearray(header + segment + section)
        binary.extend(b"\x00" * (0x4000 - len(binary)))
        binary.extend(b"\x90" * 16)
        return binary

    def test_injects_commands_without_shifting_sections(self):
        with tempfile.TemporaryDirectory() as work:
            path = os.path.join(work, "Arizona")
            with open(path, "wb") as handle:
                handle.write(self.build_macho())
            original_text = bytes(open(path, "rb").read()[0x4000:0x4010])

            result = inject_load_commands(path, path)
            self.assertEqual(result["ncmds_after"], 3)
            self.assertEqual(result["injected_bytes"] % 8, 0)
            data = open(path, "rb").read()
            self.assertEqual(data[0x4000:0x4010], original_text)
            header = parse_header(data)
            strings = load_command_strings(data, header)
            self.assertIn((0x0C, "@rpath/libNeoLoaderBootstrap.dylib"), strings)
            self.assertIn((0x8000001C, "@executable_path/Frameworks"), strings)

    def test_command_sizes_are_aligned(self):
        self.assertEqual(len(dylib_command("@rpath/libNeoLoaderBootstrap.dylib")) % 8, 0)
        self.assertEqual(len(rpath_command("@executable_path/Frameworks")) % 8, 0)


if __name__ == "__main__":
    unittest.main()
