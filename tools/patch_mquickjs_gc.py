#!/usr/bin/env python3
"""Patch MQuickJS's threaded moving GC for 32-bit big-endian SuperH.

The Jonkers algorithm temporarily replaces each heap object header with
a linked list of JSValue* ref slots. Crucially, the *last* link is the
original object's header word. On SH3EB, an ordinary JSObject header
with class_id=2 and extra_size=1 equals 0x10200001; low two bits look
like a tagged JS pointer, but 0x10200000 is not mapped.

Both the initial test and EVERY link in the unthreading loop must use
a predicate that distinguishes P1/P2 pointers (bit31=1) from object
headers (bit31=0). Otherwise the first GC patch works once, then
fails after the first referenced object when the loop reaches the
original header (observed 040/PC=8c8046e4/TEA=10200000).

Keep the workaround target-specific; fail closed if upstream changes.
"""
from __future__ import annotations

import pathlib
import sys

OLD = """static void gc_update_threaded_pointers(JSContext *ctx,
                                        void *ptr, void *new_ptr)
{
    JSValue val, *pv;

    val = *(JSValue *)ptr;
    if (JS_IsPtr(val)) {
        /* update the threaded pointers to the node 'ptr' and
           unthread it. */
        for(;;) {
            pv = js_value_to_pval(ctx, val);
            val = *pv;
            *pv = JS_VALUE_FROM_PTR(new_ptr);
            if (!JS_IsPtr(val))
                break;
        }
        *(JSValue *)ptr = val;
    }
}
"""

NEW = """/* The original header of a JS heap object terminates the threaded
   pointer list. On SH3EB its low bits can look like JSValue's '01'
   pointer tag. The actual JSValue* link addresses for this platform
   are in P1/P2 RAM (>= 0x80000000). Check at EVERY link: it is not
   sufficient to check only the first element before the loop. */
static BOOL gc_is_thread_link(JSValue val)
{
    if (!JS_IsPtr(val))
        return FALSE;
#if defined(__sh__) && defined(__BYTE_ORDER__) && \\
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && (UINTPTR_MAX == UINT32_MAX)
    if (!(val & UINT32_C(0x80000000)))
        return FALSE;
#endif
    return TRUE;
}

static void gc_update_threaded_pointers(JSContext *ctx,
                                        void *ptr, void *new_ptr)
{
    JSValue val, *pv;

    val = *(JSValue *)ptr;
    if (gc_is_thread_link(val)) {
        /* update the threaded pointers to the node 'ptr' and
           unthread it. */
        for(;;) {
            pv = js_value_to_pval(ctx, val);
            val = *pv;
            *pv = JS_VALUE_FROM_PTR(new_ptr);
            if (!gc_is_thread_link(val))
                break;
        }
        *(JSValue *)ptr = val;
    }
}
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
