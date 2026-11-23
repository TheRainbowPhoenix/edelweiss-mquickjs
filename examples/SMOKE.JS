/* One-screen graphics test. Auto-return after 3 seconds. */
gint.dclear(0xffff);
gint.drect(10, 10, 300, 58, 0x2273);
gint.dtext(22, 25, 0xffff, "MicroQuickJS graphics OK");
gint.dtext(22, 88, 0x0841, "Returns automatically in 3 seconds");
gint.dupdate();
gint.wait(3000);
