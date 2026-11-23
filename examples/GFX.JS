/* GFX.JS - MicroQuickJS expanded gint drawing API
 * No modules or external files required. Auto-return after four seconds. */
var W=gint.width(),H=gint.height();
var navy=gint.C_RGB(2,5,14),mint=gint.C_RGB(9,48,18);
gint.dclear(gint.C_WHITE);

/* Gradient bands and outlined window use the same RGB565 values as PythonExtra. */
for(var x=0;x<W;x+=24){
    var col=gint.C_RGB((x/24)%32,10+(x/12)%42,25);
    gint.drect(x,0,x+23,48,col);
}
gint.drect_border(18,65,W-18,H-26,0xf7de,3,navy);
gint.dline(20,110,W-22,110,navy);
gint.dcircle(115,190,58,mint,navy);
gint.dellipse(220,134,390,254,0xf980,navy);
gint.dpoly([426,139,490,234,407,231],0x7c1f,navy);
gint.dwindow_set(30,125,W-30,H-35);
gint.dline(0,0,W,H,navy); /* This line is clipped to the content window. */
gint.dwindow_set(0,0,W,H);
gint.dtext(28,76,navy,"MicroQuickJS gint API");
gint.dtext(28,96,navy,"shapes, RGB565, polygons, clipping");
gint.dupdate();

var e=gint.pollevent();
if(e.type!==gint.KEYEV_NONE && e.key===gint.BACK){
    /* Optional immediate exit with BACK */
}else{
    gint.wait(4000);
}
