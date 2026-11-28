"""Regression tests for the SH3EB moving-GC header vs. pointer collision."""
import importlib.util
import pathlib
import unittest

HERE = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "patch_mquickjs_gc", HERE / "tools" / "patch_mquickjs_gc.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def js_is_ptr(val):
    # MQuickJS's JSValue pointer tag is 01 in the low two bits.
    return val & 3 == 1


def sh3eb_threaded_pointer(val):
    # The extra high-bit gate is only enabled on SH3EB:
    # every real threaded pointer is in P1/P2, with bit31 set.
    return js_is_ptr(val) and bool(val & 0x80000000)


class GCThreadingTests(unittest.TestCase):
    def test_be_object_header_triggers_original_bug(self):
        # gc_mark:0, mtag:JS_MTAG_OBJECT=1, class_id=2, extra_size=1
        header = 0x10200001
        self.assertTrue(js_is_ptr(header))  # old MQuickJS bug!
        self.assertFalse(sh3eb_threaded_pointer(header))
        self.assertEqual(header - 1, 0x10200000)  # observed TEA

    def test_all_big_endian_header_kinds(self):
        # Mark pass has cleared gc_mark. mtag occupies bits 30..28.
        for mtag in range(8):
            for payload in (0, 1, 2, 3, 15, 0x000fffff, 0x0fffffff):
                header = (mtag << 28) | payload
                self.assertFalse(sh3eb_threaded_pointer(header))

    def test_native_threaded_pointer(self):
        # SH3EB root references live in the normal P1/P2 RAM area.
        for val in (0x8c700001, 0x8c8f1235, 0xa0001001):
            self.assertTrue(sh3eb_threaded_pointer(val))
        self.assertFalse(sh3eb_threaded_pointer(0x8c800000))

    def test_upstream_patch_fails_closed(self):
        source = ("static void gc_update_threaded_pointers(JSContext *ctx,"
                  " void *ptr, void *new_ptr)\n{\n"
                  + module.OLD + "        for (;;) {}\n    }\n}\n")
        result = module.patch(source)
        self.assertEqual(result.count("0x80000000"), 1)
        self.assertIn("JS_IsPtr(val)", result)
        with self.assertRaisesRegex(ValueError, "changed"):
            module.patch("unrelated source")
        with self.assertRaisesRegex(ValueError, "changed"):
            module.patch(source * 2)


if __name__ == "__main__":
    unittest.main()
