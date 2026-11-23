/* FLAGS.JS - Port of PythonExtra gint graphics demo. */
var g=gint;
g.dclear(g.C_WHITE);
g.drect(10,10,30,50,g.C_RGB(0,0,31));
g.drect(30,10,50,50,g.C_WHITE);
g.drect(50,10,70,50,g.C_RGB(31,0,0));
g.drect(80,10,130,20,g.C_BLACK);
g.drect(80,20,130,30,g.C_RGB(31,0,0));
g.drect(80,30,130,40,g.C_RGB(31,31,0));
g.dcircle(30,80,10,g.C_BLACK,g.C_WHITE);
g.dcircle(60,80,10,g.C_NONE,g.C_BLACK);
g.dellipse(100,70,120,90,g.C_RGB(28,28,31),g.C_BLACK);
g.dellipse(130,70,150,90,g.C_NONE,g.C_BLUE);
for(var y=0;y<32;y++){
  var color=g.C_RGB(0,y,y);
  for(var x=10;x<40;x++)g.dpixel(x,100+y,color);
}
for(y=0;y<32;y++)for(x=0;x<32;x++){
  g.dpixel(60+x,100+y,g.C_RGB(x,y,31-x));
}
var gray=g.C_RGB(18,18,18);
g.dvline(10,gray);
g.dvline(g.DWIDTH-10,gray);
g.dhline(10,gray);
g.dhline(g.DHEIGHT-10,gray);
g.dline(150,200,180,200,g.C_BLACK);
g.dline(180,200,180,230,g.C_BLACK);
g.dline(180,230,150,230,g.C_BLACK);
g.dline(150,230,150,200,g.C_BLACK);
g.dtext(12,175,g.C_BLACK,"Press any key to return");
g.dupdate();
g.getkey();
