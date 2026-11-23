/* TOUCH.JS - Touch drawing demo using real gint events.
   Drag a finger to draw. EXE clears; EXIT/ACON quits. */
var W=gint.DWIDTH,H=gint.DHEIGHT;
function clear(){
  gint.dclear(gint.C_WHITE);
  gint.dtext(2,2,gint.C_BLACK,"Touch: draw | EXE: clear | EXIT: quit");
}
clear();
gint.dupdate();
var x=-1,y=-1;
var running=true;
while(running){
  var ev=gint.getkeyevent(16);
  var dirty=false;
  while(ev.type!==gint.KEYEV_NONE){
    if(ev.type===gint.KEYEV_DOWN){
      if(ev.key===gint.KEY_EXE){clear();dirty=true;x=-1;y=-1;}
      if(ev.key===gint.KEY_EXIT||ev.key===gint.KEY_ACON){running=false;break;}
    }else if(ev.type===gint.KEYEV_TOUCH_DOWN){
      x=ev.x;y=ev.y;
      gint.dpixel(x,y,gint.C_BLACK);dirty=true;
    }else if(ev.type===gint.KEYEV_TOUCH_DRAG){
      if(x>=0&&y>=0)gint.dline(x,y,ev.x,ev.y,gint.C_BLACK);
      x=ev.x;y=ev.y;dirty=true;
    }else if(ev.type===gint.KEYEV_TOUCH_UP){
      x=-1;y=-1;
    }
    ev=gint.pollevent();
  }
  if(dirty)gint.dupdate();
}
