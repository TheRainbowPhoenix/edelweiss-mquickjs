"""Check host-generated MQuickJS ROM data is corrected for the SH big-endian ABI."""
import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location(
    "qjs_codegen", pathlib.Path(__file__).resolve().parents[1] / "tools" / "make_stdlib.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ROMEndianTests(unittest.TestCase):
    def test_string_word_and_double_pair(self):
        # Build a representative builder stream, including a numeric JSValue
        # between the strings and the floats; it must NOT be swapped.
        source = (
            "static const uint32_t js_stdlib_table[] = {\n"
            "  /* atom_table */\n"
            '  0x6c6c756e, /* atom bytes are little endian in the builder */\n'
            + "  0x6c6c756e,\n" * 205
            + "  /* sorted atom table (offset=207) */\n"
            + "  JS_ROM_VALUE(0),\n"
            + "  /* float64 (offset=210) */\n"
            + "  JS_MB_HEADER_DEF(JS_MTAG_FLOAT64),\n"
            + "  0x00000000,\n"
            + "  0x7ff00000,\n"
            + "};\n"
        )
        # The upstream-generated words don't have trailing inline comments.
        source = source.replace(
            '  0x6c6c756e, /* atom bytes are little endian in the builder */',
            '  0x6c6c756e,'
        )
        converted = module.big_endian_rom(source)
        self.assertEqual(converted.count("  0x6e756c6c,\n"), 206)
        self.assertIn("  JS_ROM_VALUE(0),\n", converted)
        self.assertIn("  0x7ff00000,\n  0x00000000,\n", converted)

    def test_reject_changed_upstream_layout(self):
        with self.assertRaisesRegex(ValueError, "atom layout"):
            module.big_endian_rom("static const uint32_t table[] = {0};")

if __name__ == "__main__":
    unittest.main()
