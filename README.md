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
Basic HELLO.JS and DEMO.JS execution has been confirmed on a physical EX-word device; new graphics extensions remain unverified on hardware. Try the tiny
`SMOKE.JS` (one-shot graphics) before the interactive `DEMO.JS`.

## JavaScript `gint` API

Inspired by [PythonExtra's gint reference](https://raw.githubusercontent.com/ClasspadDev/wiki/refs/heads/main/src/content/docs/python/reference/gint.mdx) and its [native bindings](https://github.com/TheRainbowPhoenix/PythonExtra/blob/main/ports/sh/modgint.c), this JavaScript subset uses the existing Edelweiss resident's drawing callbacks and does **not** install a second gint kernel.

| Category | Methods |
| --- | --- |
| Display | `dclear(color)`, `dupdate()`, `dtext(x,y,color,text)` |
| Shapes | `drect(x1,y1,x2,y2,color)`, `drect_border(x1,y1,x2,y2,fill,width,border)`, `dpixel(x,y,color)`, `dline(x1,y1,x2,y2,color)` |
| More primitives | `dhline(y,color)`, `dvline(x,color)`, `dcircle(cx,cy,r,fill,border)`, `dellipse(x1,y1,x2,y2,fill,border)`, `dpoly([x0,y0,x1,y1,...],fill,border)` |
| Clipping | `dwindow_set(left,top,right,bottom)`, `dwindow_get()` (returns a 4-element array) |
| Color/dimensions | `C_RGB(r5,g6,b5)`, `rgb(r8,g8,b8)`, `DWIDTH`, `DHEIGHT`, `width()`, `height()` |
| Input | `getkey()` (legacy integer), `getkeyevent(timeout_ms=0)`, `pollevent()`, `wait(ms)` |
| Constants | `C_WHITE`, `C_BLACK`, `C_RED`, `C_GREEN`, `C_BLUE`, `C_NONE` (=-1); `UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, `BACK`, `HOME`, `TOUCH`, `REFRESH` |

Drawings remain in the framebuffer until `gint.dupdate()` is called. Rectangle endpoints are **inclusive**; `dwindow_set` clipping endpoints right/bottom are **exclusive** and apply to the new software graphics primitives. The resident's text renderer does not yet honor this software clip. Use `C_NONE` for optional shape fill/border. Coordinates are bounded to -4096..4096; polygons support 3..32 vertex pairs.

### Native key and touch events

`gint.pollevent()` returns the **actual gint-exword event**, including
`KEYEV_TOUCH_DOWN` (5), `KEYEV_TOUCH_DRAG` (6),
`KEYEV_TOUCH_UP` (7), physical `key`, `time`, `shift`,
`alpha`, `mod`, `x`, and `y`, just like PythonExtra.
`gint.getkeyevent(0)` waits for the next raw event;
`getkeyevent(1..60000)` has a timeout.
`gint.keydown(key)`, `keypressed(key)`, `keyreleased(key)`,
`clearevents()` and `cleareventflips()` are native gint functions.

The `KEY_*` constants now use **physical gint keycodes** (e.g.
`KEY_EXE=0x15`, `KEY_EXIT=0x74`, `KEY_UP=0x86`, `KEY_8=0x42`).
The legacy `gint.getkey()` still returns EDL semantic actions;
`gint.UP`/`gint.OK` etc. retain their original abstract values.
The recommended cross-application interface is `pollevent()` and
`getkeyevent()` with physical keys.

### Sprite images and tilemaps

`gint.image_rgb565(width,height,pixels)` takes a **flat row-major JS array**
of RGB565 integer pixels and returns a sprite object. Pixels are decoded **once**
into native aligned 16-bit memory, not decoded again on each draw.
`gint.dimage(x,y,sprite)` draws it at native gint speed;
`gint.dsubimage(x,y,sprite,sx,sy,width,height)` draws a region of an
atlas. Both respect `dwindow_set` clipping. Up to 24 sprites and
128 KiB of decoded images are allowed in one JS invocation, with image
dimensions bounded to 256×256. This initial implementation supports
**opaque RGB565**; palette formats and custom transparent colors remain future work.
Sprite storage is released at script termination. `dgetpixel(x,y)`
is also now exposed for framebuffer reads (returns -1 outside the display).

Demonstration scripts: `TOUCH.JS` (finger drawing), `FLAGS.JS`
(PythonExtra graphics), `TILES.JS` (512 atlas subimage draws), and
`DINO.JS` (keyboard-based endless runner with native cached sprites).

### Example

```js
gint.dclear(gint.C_WHITE);
gint.dcircle(120, 130, 55, gint.C_BLUE, gint.C_BLACK);
gint.dwindow_set(20, 80, 500, 240);
gint.dline(0, 0, 528, 300, gint.C_RED);
gint.dwindow_set(0, 0, gint.DWIDTH, gint.DHEIGHT);
gint.dupdate();
gint.wait(3000);
```

See `examples/GFX.JS` for the basic shapes and `examples/TOUCH.JS`, `FLAGS.JS`, `TILES.JS` and `DINO.JS` for the newer native facilities. Custom fonts and indexed/palette images are still not implemented. This remains a subset of PythonExtra's gint module.

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
