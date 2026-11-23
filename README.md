# MicroQuickJS for EX-word / Edelweiss

An experimental port of [bellard/mquickjs](https://github.com/bellard/mquickjs)
to [Edelweiss](https://github.com/TheRainbowPhoenix/Edelweiss) on EX-word,
including the [Exword-toybox](https://github.com/TheRainbowPhoenix/Exword-toybox)
`CMD1` module manifest and a tiny **`gint` JavaScript drawing API**.

## Ways to launch

**From Toybox:** copy `qjs.edl` next to Toybox's `base.edl`,
`editors.edl`, `text.edl` and `utils.edl` in `TOYBX/_CONTENT`.
The Toybox resident must support five EDL bundles (the current unmodified
Toybox only supports four). Re-enter the shell, then:

```text
EX> js DEMO.JS
EX> qjs ARGS.JS hello 123
EX> js
```

`js` with no filename opens a filesystem browser in the **current Toybox directory**.
EXE opens folders or runs any `.JS`/`.js` file; EXIT returns to its parent. `scriptArgs` contains
the arguments *after* the filename, and `scriptPath` is the resolved file path.

**From the home screen:** build an EX-word installation package with ID
`QJSJS`. This includes a normal Edelweiss bootstrap `qjs.d01` and an
identical payload renamed to `main.edl`, as required by its bootstrap. When
run directly, the interpreter opens a full filesystem browser starting beside
its `main.edl` payload.

**Native EX-word file browser:** The standalone QJSJS bootstrap now uses a
new **read-only, one-world-switch** directory listing and file-size service
in the Edelweiss resident D01. Unlike the previous experimental build, it
does not carry a native `findfirst` handle across world switches. This host
extension is on [Edelweiss PR #3](https://github.com/TheRainbowPhoenix/Edelweiss/pull/3),
included by this QuickJS port. **Update both `qjs.d01` and `main.edl`**;
mixing old and new versions will not provide the directory-list service.
The picker navigates mounted EX-word volumes, directories and `.JS` files.

**Troubleshooting:** Before each operation, MicroQuickJS displays an
initialization stage on screen: browser scan, file read, VM allocation,
context initialization, source parsing, and JavaScript execution.
If the display stays on one stage, photograph or quote that exact line.
The payload has not been verified on physical hardware. Try the tiny
`SMOKE.JS` (one-shot graphics) before the interactive `DEMO.JS`.

## JavaScript graphics API

The JavaScript global `gint` uses Edelweiss's resident drawing callbacks:

| Function | Effect |
| --- | --- |
| `gint.dclear(color)` | Fill the 528 × 320 framebuffer with RGB565 |
| `gint.drect(x1,y1,x2,y2,color)` | Draw clipped, inclusive filled rectangle |
| `gint.dtext(x,y,color,string)` | Draw text |
| `gint.dupdate()` | Refresh screen |
| `gint.getkey()` | Wait for an abstract key code |
| `gint.wait(ms)` | Wait with timeout, *if the host provides* `wait_timeout` |
| `gint.rgb(r,g,b)` | Convert 8-bit RGB to RGB565 |
| `gint.width()`, `gint.height()` | Current screen dimensions |

Key constants: `gint.UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, `BACK` and
`HOME`. See `examples/DEMO.JS` for an input-driven GUI example.

This is **MicroQuickJS**, not QuickJS/Node. It supports a subset of JavaScript
close to ES5. There is no `require`, `fs`, DOM, imported modules, or event loop.
Host `load`, `setTimeout`, `Date`, and `performance` have been intentionally
omitted from the ROM stdlib; file loading is done in the C launcher. Maximum
script source size is **96 KiB**, and each invocation uses a **160 KiB VM
memory buffer**, alongside the payload's local gint heap.

## Build

The CI workflow cross-builds with the EX-word fxSDK toolchain and gint-exword.
It fetches upstream MQuickJS and Edelweiss and generates the ROM standard
library **for 32-bit target pointers** with a native host builder. Equivalent
local configuration:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/fxsdk/EXWORD.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/gint-exword \
  -DEXWORD_METADATA_SOURCE=/path/to/exword-metadata
cmake --build build --parallel 2
python3 tools/check_package.py build
```

Outputs: `build/qjs.edl` (Toybox), `build/main.edl` and `build/qjs.d01`
(standalone bootstrap), and `build/packages/ja,QJSJS` / `cn,QJSJS` packages
(the actual directory layout is `packages/{ja,cn}/QJSJS`).

## Safety and current status

This port is **experimental**. Passing native cross-build and ELF validation
does not establish device safety. The firmware profiles, EDL backing arena,
cache coherence and native storage interface must be verified on the exact
supported EX-word model. Back up device data before testing. Launch the
read-only GUI demo before attempting filesystem-writing scripts or important
user files. Never execute untrusted precompiled MQuickJS bytecode; this port
only accepts source `.JS` files.

MIT-licensed upstream MQuickJS remains fetched from its original repository.
The application-specific glue uses that upstream code without vendoring it.
