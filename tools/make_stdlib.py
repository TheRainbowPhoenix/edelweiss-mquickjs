#!/usr/bin/env python3
"""Adapt upstream MQuickJS' generated ROM stdlib without vendoring the engine.

Keep its ES5 core, but omit the host REPL's file loading, timers, and native
Date/console clock integrations. Supply a tiny object of screen/input bindings.
Generate 32-bit atoms and stdlib for SH-4 even on a 64-bit host compiler.
"""
import pathlib
import re
import subprocess
import sys

GINT_PROPS = """
static const JSPropDef exword_gint_props[] = {
    JS_CFUNC_DEF("dclear", 1, js_gint_dclear),
    JS_CFUNC_DEF("dtext", 4, js_gint_dtext),
    JS_CFUNC_DEF("drect", 5, js_gint_drect),
    JS_CFUNC_DEF("drect_border", 7, js_gint_drect_border),
    JS_CFUNC_DEF("dpixel", 3, js_gint_dpixel),
    JS_CFUNC_DEF("dline", 5, js_gint_dline),
    JS_CFUNC_DEF("dhline", 2, js_gint_dhline),
    JS_CFUNC_DEF("dvline", 2, js_gint_dvline),
    JS_CFUNC_DEF("dcircle", 5, js_gint_dcircle),
    JS_CFUNC_DEF("dellipse", 6, js_gint_dellipse),
    JS_CFUNC_DEF("dpoly", 3, js_gint_dpoly),
    JS_CFUNC_DEF("image_rgb565", 3, js_gint_image_rgb565),
    JS_CFUNC_DEF("dimage", 3, js_gint_dimage),
    JS_CFUNC_DEF("dsubimage", 7, js_gint_dsubimage),
    JS_CFUNC_DEF("dgetpixel", 2, js_gint_dgetpixel),
    JS_CFUNC_DEF("dwindow_set", 4, js_gint_dwindow_set),
    JS_CFUNC_DEF("dwindow_get", 0, js_gint_dwindow_get),
    JS_CFUNC_DEF("dupdate", 0, js_gint_dupdate),
    JS_CFUNC_DEF("getkey", 0, js_gint_getkey),
    JS_CFUNC_DEF("getkeyevent", 1, js_gint_getkeyevent),
    JS_CFUNC_DEF("pollevent", 0, js_gint_pollevent),
    JS_CFUNC_DEF("keydown", 1, js_gint_keydown),
    JS_CFUNC_DEF("keypressed", 1, js_gint_keypressed),
    JS_CFUNC_DEF("keyreleased", 1, js_gint_keyreleased),
    JS_CFUNC_DEF("clearevents", 0, js_gint_clearevents),
    JS_CFUNC_DEF("cleareventflips", 0, js_gint_cleareventflips),
    JS_CFUNC_DEF("wait", 1, js_gint_wait),
    JS_CFUNC_DEF("width", 0, js_gint_width),
    JS_CFUNC_DEF("height", 0, js_gint_height),
    JS_CFUNC_DEF("rgb", 3, js_gint_rgb),
    JS_CFUNC_DEF("C_RGB", 3, js_gint_C_RGB),
    JS_PROP_DOUBLE_DEF("DWIDTH", 528, 0),
    JS_PROP_DOUBLE_DEF("DHEIGHT", 320, 0),
    JS_PROP_DOUBLE_DEF("C_WHITE", 65535, 0),
    JS_PROP_DOUBLE_DEF("C_BLACK", 0, 0),
    JS_PROP_DOUBLE_DEF("C_RED", 63488, 0),
    JS_PROP_DOUBLE_DEF("C_GREEN", 2016, 0),
    JS_PROP_DOUBLE_DEF("C_BLUE", 31, 0),
    JS_PROP_DOUBLE_DEF("C_NONE", -1, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_NONE", 0, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_DOWN", 1, 0),
    /* gint-exword/include/gint/keyboard.h physical event enum. */
    JS_PROP_DOUBLE_DEF("KEYEV_UP", 2, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_HOLD", 3, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_TOUCH_DOWN", 5, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_TOUCH_DRAG", 6, 0),
    JS_PROP_DOUBLE_DEF("KEYEV_TOUCH_UP", 7, 0),
    JS_PROP_DOUBLE_DEF("UP", 1, 0),
    JS_PROP_DOUBLE_DEF("DOWN", 2, 0),
    JS_PROP_DOUBLE_DEF("LEFT", 3, 0),
    JS_PROP_DOUBLE_DEF("RIGHT", 4, 0),
    JS_PROP_DOUBLE_DEF("OK", 5, 0),
    JS_PROP_DOUBLE_DEF("BACK", 6, 0),
    JS_PROP_DOUBLE_DEF("REFRESH", 7, 0),
    JS_PROP_DOUBLE_DEF("TOUCH", 8, 0),
    JS_PROP_DOUBLE_DEF("HOME", 9, 0),
    /* Physical keycodes from gint-exword/include/gint/keycodes.h.
       Legacy UP/DOWN/OK above still refer to abstract EDL actions. */
    JS_PROP_DOUBLE_DEF("KEY_LEFT", 133, 0),
    JS_PROP_DOUBLE_DEF("KEY_UP", 134, 0),
    JS_PROP_DOUBLE_DEF("KEY_EXIT", 116, 0),
    JS_PROP_DOUBLE_DEF("KEY_DOWN", 117, 0),
    JS_PROP_DOUBLE_DEF("KEY_RIGHT", 118, 0),
    JS_PROP_DOUBLE_DEF("KEY_EXE", 21, 0),
    JS_PROP_DOUBLE_DEF("KEY_2", 34, 0),
    JS_PROP_DOUBLE_DEF("KEY_5", 50, 0),
    JS_PROP_DOUBLE_DEF("KEY_8", 66, 0),
    JS_PROP_DOUBLE_DEF("KEY_DEL", 68, 0),
    JS_PROP_DOUBLE_DEF("KEY_ACON", 7, 0),
    JS_PROP_END,
};
static const JSClassDef exword_gint_obj =
    JS_OBJECT_DEF("gint", exword_gint_props);
"""

def prepare(src: pathlib.Path, dst: pathlib.Path) -> None:
    text = src.read_text()
    # The standard library generator references these function names by text;
    # actual exported C handlers are defined by src/main.c.
    marker = "static const JSPropDef js_global_object[] = {"
    assert text.count(marker) == 1, "upstream global layout changed"
    text = text.replace(marker, GINT_PROPS + "\n" + marker)
    marker = '    JS_PROP_CLASS_DEF("console", &js_console_obj),'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '\n    JS_PROP_CLASS_DEF("gint", &exword_gint_obj),')
    for line in [
        '    JS_PROP_CLASS_DEF("Date", &js_date_class),',
        '    JS_PROP_CLASS_DEF("performance", &js_performance_obj),',
        '    JS_CFUNC_DEF("gc", 0, js_gc),',
        '    JS_CFUNC_DEF("load", 1, js_load),',
        '    JS_CFUNC_DEF("setTimeout", 2, js_setTimeout),',
        '    JS_CFUNC_DEF("clearTimeout", 1, js_clearTimeout),',
    ]:
        assert text.count(line) == 1, "upstream stdlib changed: " + line
        text = text.replace(line, "")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text)

_HEX_WORD = re.compile(r"^\s*0x([0-9a-fA-F]{8}),\s*$")
_STRING_HEADER = re.compile(
    r'^(\s*)\(JS_MTAG_STRING << 1\) \| \((\d+) << JS_MTAG_BITS\)'
    r' \| \((\d+) << \(JS_MTAG_BITS \+ 1\)\)'
    r' \| \((\d+) << \(JS_MTAG_BITS \+ 2\)\)'
    r' \| \((\d+) << \(JS_MTAG_BITS \+ 3\)\),(.*)$'
)
_ARRAY_HEADER = re.compile(r'^(\s*)JS_VALUE_ARRAY_HEADER\((\d+)\),(.*)$')
_BLOCK_HEADER = re.compile(r'^(\s*)JS_MB_HEADER_DEF\((JS_MTAG_[A-Z0-9_]+)\),(.*)$')
_PROPERTY_HEADER = re.compile(
    r'^(\s*)\((\d+) << 1\) \| \((JS_PROP_[A-Z_]+) << 30\),(.*)$'
)


def big_endian_rom(source: str) -> str:
    """Convert MQuickJS host ROM objects to native SH3EB bitfield layout.

    The host builder emits little-endian string bytes, double halves AND
    little-endian C bitfield headers. A SH3EB compiler lays bitfields MSB-first:
    GC mtag bits are at 30..28, string flags at 27..25, lengths at 24..0.
    Without conversion, ROM 'null' is 'llun', and ROM JSString/JSValueArray
    headers are seen as FREE or invalid by the runtime, corrupting the parser.

    Only generated ROM objects are rewritten: dynamic JSValue integers and
    pointer expressions are left intact. The MQuickJS runtime's native C
    bitfields need no changes because it creates those on the target CPU.
    """
    lines = source.splitlines()
    begin = [i for i, line in enumerate(lines) if line.strip() == "/* atom_table */"]
    end = [i for i, line in enumerate(lines)
           if line.strip().startswith("/* sorted atom table (offset=")]
    if len(begin) != 1 or len(end) != 1 or begin[0] >= end[0]:
        raise ValueError("unexpected MQuickJS ROM atom layout")
    counts = dict(strings=0, bytes=0, arrays=0, blocks=0, properties=0, floats=0)
    for i, line in enumerate(lines):
        match = _STRING_HEADER.fullmatch(line)
        if match:
            unique, ascii_, numeric, length = map(int, match.group(2, 3, 4, 5))
            if unique > 1 or ascii_ > 1 or numeric > 1 or length >= 1 << 25:
                raise ValueError("invalid generated ROM string metadata")
            lines[i] = (
                f"{match[1]}(JS_MTAG_STRING << 28) | ({unique} << 27)"
                f" | ({ascii_} << 26) | ({numeric} << 25) | {length},{match[6]}"
            )
            counts["strings"] += 1
            continue
        match = _ARRAY_HEADER.fullmatch(line)
        if match:
            lines[i] = f"{match[1]}((JS_MTAG_VALUE_ARRAY << 28) | {match[2]}),{match[3]}"
            counts["arrays"] += 1
            continue
        match = _BLOCK_HEADER.fullmatch(line)
        if match:
            lines[i] = f"{match[1]}({match[2]} << 28),{match[3]}"
            counts["blocks"] += 1
            continue
        match = _PROPERTY_HEADER.fullmatch(line)
        if match:
            # JSProperty is (hash_next:30, prop_type:2). BE stores the
            # first-declared hash_next in high bits and prop_type in low bits.
            lines[i] = f"{match[1]}(({match[2]} << 3) | {match[3]}),{match[4]}"
            counts["properties"] += 1
            continue
        if begin[0] < i < end[0]:
            match = _HEX_WORD.fullmatch(line)
            if match:
                value = int(match[1], 16)
                swapped = int.from_bytes(value.to_bytes(4, "little"), "big")
                lines[i] = f"  0x{swapped:08x},"
                counts["bytes"] += 1
    for i, line in enumerate(lines):
        if "/* float64 (offset=" not in line:
            continue
        if "JS_MTAG_FLOAT64" not in lines[i + 1]:
            raise ValueError("unexpected MQuickJS ROM float header")
        if not _HEX_WORD.fullmatch(lines[i + 2]) or not _HEX_WORD.fullmatch(lines[i + 3]):
            raise ValueError("unexpected MQuickJS float64 payload")
        # Little-endian binary64 halves -> big-endian high then low.
        lines[i + 2], lines[i + 3] = lines[i + 3], lines[i + 2]
        counts["floats"] += 1
    if counts["bytes"] < 200 or any(count == 0 for count in counts.values()):
        raise ValueError(f"incomplete target-endian ROM conversion: {counts}")
    return "\n".join(lines) + ("\n" if source.endswith("\n") else "")


def generate(executable: str, header: pathlib.Path, atoms: pathlib.Path) -> None:
    with atoms.open("wb") as output:
        subprocess.run([executable, "-a", "-m32"], check=True, stdout=output)
    # The EX-word toolchain is sh3eb-elf: always emit target-endian ROM.
    raw = subprocess.run([executable, "-m32"], check=True, stdout=subprocess.PIPE).stdout
    text = raw.decode("utf-8")
    if sys.byteorder == "little":
        text = big_endian_rom(text)
    header.write_text(text)
    assert header.stat().st_size > 100 and atoms.stat().st_size > 100

if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "prepare" and len(sys.argv) == 4:
        prepare(pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))
    elif mode == "generate" and len(sys.argv) == 5:
        generate(sys.argv[2], pathlib.Path(sys.argv[3]), pathlib.Path(sys.argv[4]))
    else:
        raise SystemExit("usage: make_stdlib.py prepare SOURCE DEST | generate BUILDER STDLIB ATOMS")
