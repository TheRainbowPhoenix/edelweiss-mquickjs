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
        # Match the native MQuickJS builder's C literal forms. Cover all
        # metadata types; numeric JSValues and ROM pointers must not change.
        string_header = (
            "  (JS_MTAG_STRING << 1) | (1 << JS_MTAG_BITS)"
            " | (1 << (JS_MTAG_BITS + 1)) | (0 << (JS_MTAG_BITS + 2))"
            ' | (4 << (JS_MTAG_BITS + 3)), /* "null" (offset=0) */\n'
        )
        source = (
            "static const uint32_t js_stdlib_table[] = {\n"
            "  /* atom_table */\n"
            + string_header
            + "  0x6c6c756e,\n" * 206
            + "  /* sorted atom table (offset=208) */\n"
            + "  JS_ROM_VALUE(0),\n"
            + "  JS_VALUE_ARRAY_HEADER(242),\n"
            + "  JS_MB_HEADER_DEF(JS_MTAG_OBJECT),\n"
            + "  (12 << 1) | (JS_PROP_SPECIAL << 30),\n"
            + "  /* float64 (offset=212) */\n"
            + "  JS_MB_HEADER_DEF(JS_MTAG_FLOAT64),\n"
            + "  0x00000000,\n"
            + "  0x7ff00000,\n"
            + "};\n"
        )
        converted = module.big_endian_rom(source)
        self.assertIn("(JS_MTAG_STRING << 28) | (1 << 27)"
                      " | (1 << 26) | (0 << 25) | 4,", converted)
        self.assertEqual(converted.count("  0x6e756c6c,\n"), 206)
        self.assertIn("  JS_ROM_VALUE(0),\n", converted)
        self.assertIn("  ((JS_MTAG_VALUE_ARRAY << 28) | 242),\n", converted)
        self.assertIn("  (JS_MTAG_OBJECT << 28),\n", converted)
        self.assertIn("  ((12 << 3) | JS_PROP_SPECIAL),\n", converted)
        self.assertIn("  (JS_MTAG_FLOAT64 << 28),\n", converted)
        self.assertIn("  0x7ff00000,\n  0x00000000,\n", converted)

    def test_reject_changed_upstream_layout(self):
        with self.assertRaisesRegex(ValueError, "atom layout"):
            module.big_endian_rom("static const uint32_t table[] = {0};")

if __name__ == "__main__":
    unittest.main()
