/* EX-word MicroQuickJS demo: draw an RGB565 UI and wait for EXIT.
 * Keep ES5-compatible syntax; MicroQuickJS is not Node.js. */
var white = 0xffff;
var dark = 0x0841;
var blue = gint.rgb(26, 80, 170);
var n = 0;
/* Auto-return after approximately 15 seconds if EXIT is not mapped. */
for (var frame = 0; frame < 60; frame++) {
    gint.dclear(white);
    gint.drect(12, 12, gint.width() - 13, 50, blue);
    gint.dtext(25, 25, white, "Hello from MicroQuickJS!");
    gint.dtext(25, 75, dark, "UP / DOWN to change the counter");
    gint.dtext(25, 100, dark, "Counter: " + n);
    gint.dtext(25, 140, dark, "EXIT or automatic return in 15 seconds.");
    gint.dupdate();
    var key = gint.wait(250);
    if (key === gint.BACK || key === gint.HOME) break;
    if (key === gint.UP) n++;
    if (key === gint.DOWN) n--;
}
