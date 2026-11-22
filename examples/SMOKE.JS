/* One-screen graphics test. Press a key to return. */
gint.dclear(0xffff);
gint.drect(10, 10, 300, 58, 0x2273);
gint.dtext(22, 25, 0xffff, "MicroQuickJS graphics OK");
gint.dtext(22, 88, 0x0841, "Press any key to exit");
gint.dupdate();
gint.getkey();
