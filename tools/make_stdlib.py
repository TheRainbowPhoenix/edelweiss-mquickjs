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
    JS_CFUNC_DEF("dupdate", 0, js_gint_dupdate),
    JS_CFUNC_DEF("getkey", 0, js_gint_getkey),
    JS_CFUNC_DEF("wait", 1, js_gint_wait),
    JS_CFUNC_DEF("width", 0, js_gint_width),
    JS_CFUNC_DEF("height", 0, js_gint_height),
    JS_CFUNC_DEF("rgb", 3, js_gint_rgb),
    JS_PROP_DOUBLE_DEF("UP", 1, 0),
    JS_PROP_DOUBLE_DEF("DOWN", 2, 0),
    JS_PROP_DOUBLE_DEF("LEFT", 3, 0),
    JS_PROP_DOUBLE_DEF("RIGHT", 4, 0),
    JS_PROP_DOUBLE_DEF("OK", 5, 0),
    JS_PROP_DOUBLE_DEF("BACK", 6, 0),
    JS_PROP_DOUBLE_DEF("HOME", 9, 0),
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

_HEX_WORD = re.compile(r"^(\s*)0x([0-9a-fA-F]{8})(,\s*)$")


def big_endian_rom(source: str) -> str:
    """Convert host-generated 32-bit ROM data into SH3EB/SH4 big-endian.

    The -m32 builder runs on a little-endian host. Its 32-bit word output
    includes packed UTF-8 bytes and IEEE754 float64 halves. Compiling those
    words *unchanged* for big-endian SH reverses every four characters
    ('null' -> 'llun') and reverses each double's 32-bit halves. The JS atom
    lookup uses these strings while parsing: corrupt tables can crash the VM.

    Numeric JSValue words and ROM relocations must NOT be byteswapped.
    """
    lines = source.splitlines(keepends=True)
    begins = [i for i, line in enumerate(lines) if line.strip() == "/* atom_table */"]
    ends = [i for i, line in enumerate(lines)
            if line.strip().startswith("/* sorted atom table (offset=")]
    if len(begins) != 1 or len(ends) != 1 or begins[0] >= ends[0]:
        raise ValueError("unexpected MQuickJS ROM atom layout")
    words = 0
    for i in range(begins[0] + 1, ends[0]):
        match = _HEX_WORD.fullmatch(lines[i])
        if match is None:
            continue
        value = int(match[2], 16)
        # The builder packs source bytes into 32-bit little-endian words.
        swapped = int.from_bytes(value.to_bytes(4, "little"), "big")
        lines[i] = f"{match[1]}0x{swapped:08x}{match[3]}"
        words += 1
    floats = 0
    for i, line in enumerate(lines):
        if "/* float64 (offset=" not in line:
            continue
        if "JS_MB_HEADER_DEF(JS_MTAG_FLOAT64)" not in lines[i + 1]:
            raise ValueError("unexpected MQuickJS float64 ROM layout")
        if not _HEX_WORD.fullmatch(lines[i + 2]) or not _HEX_WORD.fullmatch(lines[i + 3]):
            raise ValueError("unexpected MQuickJS float64 payload layout")
        # C stores the 64-bit IEEE value high-word first on a BE target.
        lines[i + 2], lines[i + 3] = lines[i + 3], lines[i + 2]
        floats += 1
    if words < 200 or floats < 1:
        raise ValueError(f"incomplete ROM conversion: {words} atoms, {floats} floats")
    return "".join(lines)


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
