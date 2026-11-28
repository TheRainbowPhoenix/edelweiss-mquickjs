/* GCROOT.JS - stress the native gint result objects without touch input.
 * The ROM event, window and image bindings must survive a moving GC.
 * A successful run shows "GC ROOTS OK" in Toybox, then returns. */
var g=gint;
var i, ev, win, tmp;
g.dclear(g.C_WHITE);
for(i=0;i<350;i++){
    ev=g.pollevent();
    if(typeof ev.type!=="number" || typeof ev.key!=="number")
        throw new Error("bad event "+i);
    win=g.dwindow_get();
    if(win.length!==4 || win[0]!==0 || win[2]!==g.DWIDTH)
        throw new Error("bad window "+i);
    /* Deliberately create short-lived JS values between native results.
       MQuickJS GC may relocate an event in a low-memory VM. */
    tmp=[i, "GC-"+i, {at:i, key:ev.key}, win, ev];
    if((i%50)===0){
        print("GC events "+i);
    }
}
var img=g.image_rgb565(2,2,[g.C_BLACK,g.C_WHITE,g.C_RED,g.C_BLUE]);
if(img.id<=0 || img.width!==2 || img.height!==2)
    throw new Error("bad image");
g.dimage(5,35,img);
g.dupdate();
print("GC ROOTS OK");
