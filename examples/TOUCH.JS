/* TOUCH.JS - standalone, shallow ES5 event loop for SuperH MQuickJS.
   Use genuine gint events; no nested function declaration to stress the
   constrained MQuickJS parser. EXE clears; EXIT / AC/ON returns to Toybox. */
var g=gint;
var lastX=-1,lastY=-1;
var running=true;
g.dclear(g.C_WHITE);
g.dtext(2,2,g.C_BLACK,"Touch: draw | EXE: clear | EXIT: quit");
g.dupdate();
while(running){
    /* The 16ms timeout also permits EXIT processing without a touch. */
    var ev=g.getkeyevent(16);
    var dirty=false;
    if(ev.type===g.KEYEV_DOWN){
        if(ev.key===g.KEY_EXE){
            g.dclear(g.C_WHITE);
            g.dtext(2,2,g.C_BLACK,"Touch: draw | EXE: clear | EXIT: quit");
            lastX=-1;
            lastY=-1;
            dirty=true;
        }
        if(ev.key===g.KEY_EXIT || ev.key===g.KEY_ACON)
            running=false;
    }else if(ev.type===g.KEYEV_TOUCH_DOWN){
        lastX=ev.x;
        lastY=ev.y;
        g.dpixel(lastX,lastY,g.C_BLACK);
        dirty=true;
    }else if(ev.type===g.KEYEV_TOUCH_DRAG){
        if(lastX>=0 && lastY>=0)
            g.dline(lastX,lastY,ev.x,ev.y,g.C_BLACK);
        lastX=ev.x;
        lastY=ev.y;
        dirty=true;
    }else if(ev.type===g.KEYEV_TOUCH_UP){
        lastX=-1;
        lastY=-1;
    }
    if(dirty)g.dupdate();
}
