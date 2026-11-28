/* TILES.JS - Build a native 16x16 RGB565 atlas, reuse dsubimage
   for hundreds of tiles without decoding pixel arrays every draw. */
var g=gint,w=16,h=16,cols=4,p=[];
var colors=[g.C_RGB(0,35,9),g.C_RGB(14,48,21),
            g.C_RGB(8,28,27),g.C_RGB(22,30,7)];
for(var y=0;y<h;y++)for(var x=0;x<w*cols;x++){
  var t=(x/16)|0;
  var grid=((x%16===0)||(y%16===0));
  p.push(grid?g.C_BLACK:colors[t]);
}
var atlas=g.image_rgb565(w*cols,h,p);
g.dclear(g.C_WHITE);
for(var row=0;row<16;row++)for(var col=0;col<32;col++)
  g.dsubimage(col*16,row*16,atlas,((col+row)%4)*16,0,16,16);
g.dtext(10,280,g.C_BLACK,"Atlas: 4 tiles, 512 native subimage blits");
g.dupdate();
g.getkey();
