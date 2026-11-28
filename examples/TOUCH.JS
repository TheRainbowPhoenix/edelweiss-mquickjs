/* TOUCH.JS - lower-latency SuperH drawing.
   - Calls resident gint's native VRAM line routine where available.
   - Drains up to 8 pending events before each LCD refresh.
   - Skips duplicate touch coordinates (no-op strokes).
   - EXE clears, EXIT / AC/ON returns to Toybox.
   The batch limit keeps EXIT responsive even under continuous touch. */
var g=gint;
var lastX=-1,lastY=-1;
var running=true;
g.dclear(g.C_WHITE);
g.dtext(2,2,g.C_BLACK,"Touch: draw | EXE: clear | EXIT: quit");
g.dupdate();
while(running){
    /* Wait briefly for the first event; drain the already-pending batch. */
    var ev=g.getkeyevent(16);
    var dirty=false;
    var processed=0;
    while(ev.type!==g.KEYEV_NONE && processed<8 && running){
        if(ev.type===g.KEYEV_DOWN){
            if(ev.key===g.KEY_EXE){
                g.dclear(g.C_WHITE);
                g.dtext(2,2,g.C_BLACK,"Touch: draw | EXE: clear | EXIT: quit");
                lastX=-1;lastY=-1;dirty=true;
            }else if(ev.key===g.KEY_EXIT || ev.key===g.KEY_ACON){
                running=false;
            }
        }else if(ev.type===g.KEYEV_TOUCH_DOWN){
            lastX=ev.x;lastY=ev.y;
            g.dpixel(lastX,lastY,g.C_BLACK);
            dirty=true;
        }else if(ev.type===g.KEYEV_TOUCH_DRAG){
            if(lastX<0 || lastY<0){
                g.dpixel(ev.x,ev.y,g.C_BLACK);
                dirty=true;
            }else if(ev.x!==lastX || ev.y!==lastY){
                g.dline(lastX,lastY,ev.x,ev.y,g.C_BLACK);
                dirty=true;
            }
            lastX=ev.x;lastY=ev.y;
        }else if(ev.type===g.KEYEV_TOUCH_UP){
            lastX=-1;lastY=-1;
        }
        processed++;
        if(running && processed<8)ev=g.pollevent();
    }
    /* Full-screen LCD transfers are far slower than drawing a line. */
    if(dirty)g.dupdate();
}
