"""Validate the MQuickJS EX-word bootstrap and the Toybox CMD1 ELF manifest."""
from pathlib import Path
import re
import struct
import sys

base = Path(sys.argv[1])
cache = (base / "CMakeCache.txt").read_text()
def setting(name):
    match = re.search(rf"^{name}:STRING=(.*)$", cache, re.MULTILINE)
    assert match, name
    return match[1]

appid, module = setting("EXWORD_APP_ID"), setting("EXWORD_MODULE")
bootstrap = (base / "edelweiss.d01").read_bytes()
payload = (base / "qjs.edl").read_bytes()
assert (base / "main.edl").read_bytes() == payload
assert bootstrap[:16] == b"CASIODICPLUGIN10" and 128 < len(bootstrap) <= 131072
assert (base / (module + ".d01")).read_bytes() == bootstrap
assert payload[:7] == b"\x7fELF\x01\x02\x01" and struct.unpack_from(">H", payload, 18)[0] == 42
assert re.fullmatch(r"[0-9a-f]{40}\n", (base / "edelweiss-version.txt").read_text())
for locale in ("ja", "cn"):
    package = base / "packages" / locale / appid
    assert (package / (module + ".d01")).read_bytes() == bootstrap
    assert (package / "main.edl").read_bytes() == payload
    assert (package / "DEMO.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "DEMO.JS").read_bytes()
    assert (package / "ARGS.JS").is_file()
    assert (package / "GFX.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "GFX.JS").read_bytes()
    assert (package / "GCROOT.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "GCROOT.JS").read_bytes()
    assert (package / "VMSTRESS.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "VMSTRESS.JS").read_bytes()
    assert (package / "VMBENCH.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "VMBENCH.JS").read_bytes()
    assert (package / "TOUCH.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "TOUCH.JS").read_bytes()
    assert (package / "FLAGS.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "FLAGS.JS").read_bytes()
    assert (package / "TILES.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "TILES.JS").read_bytes()
    assert (package / "DINO.JS").read_bytes() == (Path(__file__).resolve().parents[1] / "examples" / "DINO.JS").read_bytes()
    assert (package / "SCRIPTS.TXT").is_file()
    assert (package / "fileinfo.cji").is_file()
    html = list(package.glob("*.htm"))
    assert html
    for page in html:
        data = page.read_bytes()
        assert b"@APP" not in data and b"\r\n" in data
    print(locale, appid, "main.edl + current Edelweiss bootstrap: package passed")

# Runtime imports must remain relocatable zero-valued ABS symbols. Kernel
# startup must never be linked into a hosted application.
u32 = lambda at: struct.unpack_from(">I", payload, at)[0]
u16 = lambda at: struct.unpack_from(">H", payload, at)[0]
section_base, section_count = u32(32), u16(48)
imports, defined = set(), set()
stdlib_table_vaddr = None
for i in range(section_count):
    sec = section_base + i * 40
    if u32(sec + 4) != 2:
        continue
    strings = section_base + u32(sec + 24) * 40
    for at in range(u32(sec + 16), u32(sec + 16) + u32(sec + 20), 16):
        name_at = u32(strings + 16) + u32(at)
        name = payload[name_at:payload.index(b"\0", name_at)].decode()
        if name == "_js_stdlib_table":
            stdlib_table_vaddr = u32(at + 4)
        if u16(at + 14) == 0xfff1 and u32(at + 4) == 0:
            imports.add(name)
        elif u16(at + 14):
            defined.add(name)
assert imports, "expected hosted gint import symbols"
assert {"_edl_native_dsubimage", "_edl_native_dgetpixel", "_edl_native_dline", "_edl_native_dpixel"} <= imports, (
    "native sprite and pixel calls must resolve through resident gint"
)
assert not {"_dsubimage", "_dgetpixel", "_dline", "_dpixel"} & defined, (
    "forbid private duplicate graphics implementations in the EDL"
)
assert not {"_start", "_exword_entry", "_gint_kernel_install", "_mmu_exword_prepare"} & defined
print("Hosted gint imports verified; no second kernel/startup in qjs.edl")
# The CMD1 applet section must be a non-allocated 396-byte PROGBITS object.
shstr = section_base + u16(50) * 40
shstr_offset, shstr_len = u32(shstr + 16), u32(shstr + 20)
assert shstr_offset + shstr_len <= len(payload)
names_blob = payload[shstr_offset:shstr_offset + shstr_len]
found = []
for i in range(section_count):
    sec = section_base + i * 40
    name_off = u32(sec)
    if name_off >= len(names_blob):
        continue
    section_name = names_blob[name_off:].split(bytes([0]), 1)[0]
    if section_name != b".edl.cmd":
        continue
    assert u32(sec + 4) == 1, "manifest must be PROGBITS"
    assert not u32(sec + 8) & 2, "manifest must not have SHF_ALLOC"
    assert u32(sec + 20) == 12 + 24 * 16
    offset = u32(sec + 16)
    assert offset + 396 <= len(payload)
    magic, version, n = struct.unpack_from(">III", payload, offset)
    assert (magic, version, n) == (0x434D4431, 1, 2)
    names = [payload[offset + 12 + j * 16:offset + 28 + j * 16].split(bytes([0]))[0]
             for j in range(n)]
    assert names == [b"js", b"qjs"], names
    found.append(i)
assert len(found) == 1, "exactly one Toybox manifest required"
print("PASS: qjs.edl has valid non-loaded Toybox CMD1 manifest (js, qjs)")

# The host -m32 builder packs ROM strings in little-endian 32-bit words.
# SH3EB must instead store their actual byte order: 'null', not 'llun'.
assert stdlib_table_vaddr is not None, "generated js_stdlib_table missing"
pht_off, pht_size, pht_count = u32(28), u16(42), u16(44)
table_fileoff = None
for n in range(pht_count):
    ph = pht_off + n * pht_size
    if u32(ph) != 1:  # PT_LOAD
        continue
    offset, vaddr, length = u32(ph + 4), u32(ph + 8), u32(ph + 16)
    if vaddr <= stdlib_table_vaddr and stdlib_table_vaddr + 20 <= vaddr + length:
        table_fileoff = offset + stdlib_table_vaddr - vaddr
        break
assert table_fileoff is not None, "ROM stdlib symbol is not loadable"
# SH3EB C bitfields also store the first-declared fields MSB-first.
# MQuickJS's JSString header must contain mtag=3, unique=1, ascii=1,
# numeric=0, len=4. A little-endian 0x00000236 header is not usable here.
assert u32(table_fileoff) == 0x3c000004, (
    f"invalid big-endian JSString header 0x{u32(table_fileoff):08x}"
)
header_text = (base / "generated" / "mqjs_stdlib.h").read_text()
sorted_match = re.search(r"/\* sorted atom table \(offset=(\d+)\) \*/", header_text)
assert sorted_match, "generated sorted atom table missing"
sorted_offset = int(sorted_match[1])
array_header = u32(table_fileoff + sorted_offset * 4)
assert (array_header >> 28) == 5, (
    f"invalid big-endian JSValueArray header 0x{array_header:08x}"
)
assert payload[table_fileoff + 4:table_fileoff + 8] == b"null", (
    "wrong ROM atom byte order: expected literal 'null' (SH big endian)"
)
assert payload[table_fileoff + 16:table_fileoff + 20] == b"fals", (
    "wrong ROM atom byte order: expected literal 'fals'"
)
print("PASS: big-endian ROM atom bytes validated in compiled SH ELF")
