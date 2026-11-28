#!/usr/bin/env python3
"""Patch the upstream MQuickJS threaded GC for the EX-word SH3EB ABI.

On big-endian SH, the first word of an ordinary heap object contains
gc_mark/mtag in the HIGH bits and class_id/extra_size in lower bits.
For example, JSObject(class_id=2, extra_size=1) has 0x10200001.

MQuickJS's Jonkers compactor sees any value with bits 0..1 = 01 as a
threaded JS pointer. It therefore mistakes 0x10200001 for a pointer
and dereferences 0x10200000, raising a 040 TLB miss.

This workaround is specific to EX-word SH3EB: genuine GC reference
slots are in P1/P2 memory (bit 31 set), while all normal marked-clean
GC block headers have their top bit clear. The mark pass always resets
gc_mark before gc_compact_heap() walks the heap.

Fail closed if the upstream code being patched ever changes.
"""
from __future__ import annotations

import pathlib
import sys

OLD = """    val = *(JSValue *)ptr;
    if (JS_IsPtr(val)) {
        /* update the threaded pointers to the node 'ptr' and
           unthread it. */
"""

NEW = """    val = *(JSValue *)ptr;
#if defined(__sh__) && defined(__BYTE_ORDER__) && \\
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && (UINTPTR_MAX == UINT32_MAX)
    /* On SH3EB, ordinary GC headers may end in '01'. For example,
       0x10200001 = object, class_id=2, extra_size=1.
       JS_IsPtr(0x10200001) is TRUE even though the word is a
       header, not a threaded pointer. Forwarded pointer slots in
       this EX-word resident live in P1/P2 (bit 31 set).
       gc_mark was cleared before this compaction pass. */
    if (!(val & UINT32_C(0x80000000)))
        return;
#endif
    if (JS_IsPtr(val)) {
        /* update the threaded pointers to the node 'ptr' and
           unthread it. */
"""

def patch(source: str) -> str:
    if source.count(OLD) != 1:
        raise ValueError("MQuickJS threaded-pointer implementation changed; inspect manually")
    return source.replace(OLD, NEW, 1)

def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: patch_mquickjs_gc.py <upstream/mquickjs.c> <output.c>")
    source = pathlib.Path(sys.argv[1]).read_text()
    output = pathlib.Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(patch(source))

if __name__ == "__main__":
    main()
