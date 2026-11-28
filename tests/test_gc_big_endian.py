"""Regression tests for big-endian headers colliding with JSValue pointer tags.

The first device patch only checked the entry value and still chased
0x10200001 inside the linked-list loop, resulting in:
PC=8c8046e4 (mov.l @r2,r1), TEA=10200000.
"""
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
    return val & 3 == 1


def sh3eb_threaded_pointer(val):
    return js_is_ptr(val) and bool(val & 0x80000000)


def unthread(initial, slots, is_link):
    """Faithful model of the two pointer tests in gc_update_threaded_pointers.

    slots maps numeric SH addresses to their old JSValue content. Treat an
    invalid SH address like the hardware's TLB miss.
    """
    value = initial
    if is_link(value):
        while True:
            address = value - 1
            if address not in slots:
                raise MemoryError(f"TLB miss read at {address:08x}")
            old = slots[address]
            slots[address] = 0x8c701001
            value = old
            if not is_link(value):
                break
    return value


class GCThreadingTests(unittest.TestCase):
    def test_object_header_collision(self):
        header = 0x10200001
        self.assertTrue(js_is_ptr(header))
        self.assertFalse(sh3eb_threaded_pointer(header))
        self.assertEqual(header - 1, 0x10200000)

    def test_normal_header_types_have_high_bit_clear(self):
        # Every 32-bit SH3EB header after gc_mark is cleared has bit31=0.
        for mtag in range(8):
            for payload in (0, 1, 2, 3, 15, 0x000fffff, 0x0fffffff):
                header = (mtag << 28) | payload
                self.assertFalse(sh3eb_threaded_pointer(header))

    def test_real_p1_p2_root_links(self):
        for val in (0x8c700001, 0x8c8f1235, 0xa0001001):
            self.assertTrue(sh3eb_threaded_pointer(val))
        self.assertFalse(sh3eb_threaded_pointer(0x8c800000))

    def test_one_root_ends_with_original_header(self):
        # This crashes the unpatched MQuickJS loop *after* following one
        # valid root pointer, which explains why the first patch was incomplete.
        reference = {0x8c700100: 0x10200001}
        with self.assertRaisesRegex(MemoryError, "10200000"):
            unthread(0x8c700101, reference.copy(), js_is_ptr)

        updated = reference.copy()
        header = unthread(0x8c700101, updated, sh3eb_threaded_pointer)
        self.assertEqual(header, 0x10200001)
        self.assertEqual(updated[0x8c700100], 0x8c701001)

    def test_multiple_roots_end_with_original_header(self):
        refs = {0x8c700100: 0x8c700201, 0x8c700200: 0x10200001}
        updated = refs.copy()
        self.assertEqual(unthread(0x8c700101, updated, sh3eb_threaded_pointer),
                         0x10200001)
        self.assertEqual(updated,
                         {0x8c700100: 0x8c701001, 0x8c700200: 0x8c701001})

    def test_patch_guards_every_link_and_fails_closed(self):
        source = "before\n" + module.OLD + "after\n"
        result = module.patch(source)
        self.assertEqual(result.count("gc_is_thread_link(val)"), 2)
        self.assertNotIn("if (!JS_IsPtr(val))\n                break;", result)
        self.assertIn("UINT32_C(0x80000000)", result)
        self.assertEqual(result.count("static void gc_update_threaded_pointers"), 1)
        with self.assertRaisesRegex(ValueError, "changed"):
            module.patch("unrelated source")
        with self.assertRaisesRegex(ValueError, "changed"):
            module.patch(source * 2)


if __name__ == "__main__":
    unittest.main()
