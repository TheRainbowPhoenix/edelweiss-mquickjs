/* DINO.JS - Small ES5-friendly Dino Run using cached native RGB565 sprites.
   EXE/5 starts, UP/8 jumps, DOWN/2 ducks, EXIT quits. */
var g=gint,WHITE=g.C_WHITE,BLACK=g.C_BLACK;
var RUN1=["      #### ","      ## ##","      #####"," #   ##### "," ##########"," ##########","  ######## ","   #   #   ","   ##      "];
var RUN2=["      #### ","      ## ##","      #####"," #   ##### "," ##########"," ##########","  ######## ","   #   #   ","       ##  "];
var DUCK=["           ","           ","           ","      #####"," ##########","###########"," ##########","  #      # "];
var CACTUS=["  #  ","  # #","# # #","# # #","#####","  #  ","  #  "];
function sprite(rows,scale){
  var width=rows[0].length*scale,height=rows.length*scale,pixels=[];
  for(var y=0;y<height;y++)for(var x=0;x<width;x++)
    pixels.push(rows[(y/scale)|0].charAt((x/scale)|0)==="#"?BLACK:WHITE);
  return g.image_rgb565(width,height,pixels);
}
var run=[sprite(RUN1,2),sprite(RUN2,2)],duck=sprite(DUCK,2),cactus=sprite(CACTUS,2);
var ground=g.DHEIGHT-40,frame=0,score=0,high=0,state=0;
var py=0,vy=0,jumping=false,obstacles=[],spawn=45,running=true;
function reset(){state=1;score=0;py=0;vy=0;jumping=false;obstacles=[];spawn=45;frame=0;}
function collision(ax,ay,aw,ah,bx,by,bw,bh){
  return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;
}
while(running){
  var ev=g.getkeyevent(30),doJump=false,doStart=false;
  while(ev.type!==g.KEYEV_NONE){
    if(ev.type===g.KEYEV_DOWN){
      if(ev.key===g.KEY_EXIT||ev.key===g.KEY_ACON)running=false;
      if(ev.key===g.KEY_EXE||ev.key===g.KEY_5)doStart=true;
      if(ev.key===g.KEY_UP||ev.key===g.KEY_8)doJump=true;
    }
    ev=g.pollevent();
  }
  if(!running)break;
  if(state!==1&&doStart)reset();
  if(state===1&&doJump&&!jumping){vy=-11;jumping=true;}
  var ducking=g.keydown(g.KEY_DOWN)||g.keydown(g.KEY_2);
  if(state===1){
    frame++;
    py+=vy;
    if(py<0)vy+=1;else{py=0;vy=0;jumping=false;}
    if(--spawn<=0){obstacles.push(g.DWIDTH);spawn=35+((Math.random()*35)|0);}
    var remaining=[];
    for(var k=0;k<obstacles.length;k++){
      var ox=obstacles[k]-6;
      var ph=ducking&&!jumping?16:18;
      if(collision(25,ground-ph+py,20,ph,ox,ground-14,10,14)){
        state=2;if(score>high)high=score;
      }
      if(ox>-24)remaining.push(ox);else score++;
    }
    obstacles=remaining;
  }
  g.dclear(WHITE);
  g.dline(0,ground,g.DWIDTH-1,ground,BLACK);
  if(state===0){
    g.dtext((g.DWIDTH/2)-40,100,BLACK,"DINO RUN");
    g.dtext((g.DWIDTH/2)-65,125,BLACK,"EXE to start | EXIT quits");
  }else{
    if(ducking&&!jumping)g.dimage(25,ground-16,duck);
    else g.dimage(25,ground-18+py,run[(frame/5|0)%2]);
    for(var j=0;j<obstacles.length;j++)g.dimage(obstacles[j],ground-14,cactus);
    g.dtext(10,10,BLACK,"Score: "+score);
  }
  if(state===2){
    g.dtext((g.DWIDTH/2)-40,100,BLACK,"GAME OVER");
    g.dtext((g.DWIDTH/2)-50,125,BLACK,"High: "+high+"  EXE restarts");
  }
  g.dupdate();
}
