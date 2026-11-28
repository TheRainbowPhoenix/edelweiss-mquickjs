/* VMBENCH.JS - timing microbench for EX-word SuperH MQuickJS.
   gint.ticks() is 128Hz (~7.8 ms resolution), wraps at midnight.
   Repeat the same script before/after optimizations to compare results. */
var g=gint,i,x,t,elapsed;
function ms(start){
    var ticks=g.ticks()-start;
    if(ticks<0)ticks+=11059200;
    return ((ticks*1000/128)|0);
}
function report(label,start){
    print(label+" "+ms(start)+"ms");
}
print("VMBENCH 128Hz - elapsed milliseconds");
t=g.ticks();
var checksum=0;
for(i=0;i<10000;i++)checksum=((checksum+((i*31)^(i>>2)))|0);
report("integer 10000",t);

t=g.ticks();
var slots=[],o;
for(i=0;i<3000;i++){
    o={a:i,b:i+1,c:"s"+i};
    o["n"+(i&7)]=i;
    slots[i&31]=o;
}
report("objects 3000",t);

t=g.ticks();
for(i=0;i<1500;i++)o=g.pollevent();
report("events 1500",t);

g.dclear(g.C_WHITE);
t=g.ticks();
for(i=0;i<180;i++){
    x=20+(i%200);
    g.dline(15,x,450,x,g.C_BLACK);
}
report("native lines 180",t);

t=g.ticks();
for(i=0;i<3000;i++)g.dpixel(i%500,40+(i%200),g.C_BLUE);
report("native pixels 3000",t);

t=g.ticks();
g.dupdate();
report("LCD frame 1",t);

print("checksum "+checksum);
print("VMBENCH OK");
