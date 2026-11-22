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
for i in range(section_count):
    sec = section_base + i * 40
    if u32(sec + 4) != 2:
        continue
    strings = section_base + u32(sec + 24) * 40
    for at in range(u32(sec + 16), u32(sec + 16) + u32(sec + 20), 16):
        name_at = u32(strings + 16) + u32(at)
        name = payload[name_at:payload.index(b"\0", name_at)].decode()
        if u16(at + 14) == 0xfff1 and u32(at + 4) == 0:
            imports.add(name)
        elif u16(at + 14):
            defined.add(name)
assert imports, "expected hosted gint import symbols"
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
    section_name = names_blob[name_off:].split(b"\\0", 1)[0]
    if section_name != b".edl.cmd":
        continue
    assert u32(sec + 4) == 1, "manifest must be PROGBITS"
    assert not u32(sec + 8) & 2, "manifest must not have SHF_ALLOC"
    assert u32(sec + 20) == 12 + 24 * 16
    offset = u32(sec + 16)
    assert offset + 396 <= len(payload)
    magic, version, n = struct.unpack_from(">III", payload, offset)
    assert (magic, version, n) == (0x434D4431, 1, 2)
    names = [payload[offset + 12 + j * 16:offset + 28 + j * 16].split(b"\\0")[0]
             for j in range(n)]
    assert names == [b"js", b"qjs"], names
    found.append(i)
assert len(found) == 1, "exactly one Toybox manifest required"
print("PASS: qjs.edl has valid non-loaded Toybox CMD1 manifest (js, qjs)")
