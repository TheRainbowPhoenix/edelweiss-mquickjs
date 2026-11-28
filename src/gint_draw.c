#include "gint_draw.h"
#include <stdint.h>

/* The resident provides filled rectangles, text and display refresh.
   Keep this purely software renderer independent from gint's private VRAM:
   no second kernel, no direct native OS calls, and no unmapped pointers. */
static const struct edl_api *display;
static int left,top,right,bottom;

static int min_i(int a,int b){return a<b?a:b;}
static int max_i(int a,int b){return a>b?a:b;}
static int abs_i(int a){return a<0?-a:a;}
static void native_rect(int x1,int y1,int x2,int y2,int color)
{
    if(!display || !display->rect || color<0)return;
    if(x1>x2 || y1>y2)return;
    x1=max_i(x1,left);y1=max_i(y1,top);
    x2=min_i(x2,right-1);y2=min_i(y2,bottom-1);
    if(x1<=x2 && y1<=y2)
        display->rect(x1,y1,x2-x1+1,y2-y1+1,(uint16_t)color);
}
void qjs_draw_init(const struct edl_api *host)
{
    display=host;
    left=top=0;
    right=host?(int)host->width:0;
    bottom=host?(int)host->height:0;
}
void qjs_draw_window(int x1,int y1,int x2,int y2)
{
    if(!display)return;
    left=max_i(0,min_i((int)display->width,x1));
    top=max_i(0,min_i((int)display->height,y1));
    right=max_i(left,min_i((int)display->width,x2));
    bottom=max_i(top,min_i((int)display->height,y2));
}
void qjs_draw_get_window(int bounds[4])
{
    bounds[0]=left;bounds[1]=top;bounds[2]=right;bounds[3]=bottom;
}
void qjs_draw_clear(int color)
{
    if(display && display->rect)
        display->rect(0,0,(int)display->width,(int)display->height,(uint16_t)color);
}
void qjs_draw_rect(int x1,int y1,int x2,int y2,int color)
{
    if(x1>x2){int t=x1;x1=x2;x2=t;}
    if(y1>y2){int t=y1;y1=y2;y2=t;}
    native_rect(x1,y1,x2,y2,color);
}
void qjs_draw_pixel(int x,int y,int color){native_rect(x,y,x,y,color);}
void qjs_draw_hline(int y,int color)
{
    if(display)native_rect(0,y,(int)display->width-1,y,color);
}
void qjs_draw_vline(int x,int color)
{
    if(display)native_rect(x,0,x,(int)display->height-1,color);
}
void qjs_draw_line(int x1,int y1,int x2,int y2,int color)
{
    if(color<0)return;
    if(x1==x2){native_rect(x1,min_i(y1,y2),x1,max_i(y1,y2),color);return;}
    if(y1==y2){native_rect(min_i(x1,x2),y1,max_i(x1,x2),y1,color);return;}
    /* JS coordinate inputs are bounded to +/-4096, so Bresenham is safe. */
    int dx=abs_i(x2-x1),dy=abs_i(y2-y1);
    int sx=x1<x2?1:-1,sy=y1<y2?1:-1,err=dx-dy;
    for(;;){
        qjs_draw_pixel(x1,y1,color);
        if(x1==x2 && y1==y2)break;
        int e=2*err;
        if(e>-dy){err-=dy;x1+=sx;}
        if(e<dx){err+=dx;y1+=sy;}
    }
}
void qjs_draw_rect_border(int x1,int y1,int x2,int y2,int fill,int width,int border)
{
    if(x1>x2){int t=x1;x1=x2;x2=t;}
    if(y1>y2){int t=y1;y1=y2;y2=t;}
    if(width<0)width=0;
    if(width && border>=0){
        /* Four clipped edge strips, not a solid outer rectangle:
           a transparent C_NONE interior must remain transparent. */
        native_rect(x1,y1,x2,y1+width-1,border);
        native_rect(x1,y2-width+1,x2,y2,border);
        native_rect(x1,y1+width,x1+width-1,y2-width,border);
        native_rect(x2-width+1,y1+width,x2,y2-width,border);
    }
    if(fill>=0){
        int inset=(width && border>=0)?width:0;
        native_rect(x1+inset,y1+inset,x2-inset,y2-inset,fill);
    }
}
static void octants(int cx,int cy,int x,int y,int color)
{
    qjs_draw_pixel(cx+x,cy+y,color);qjs_draw_pixel(cx-x,cy+y,color);
    qjs_draw_pixel(cx+x,cy-y,color);qjs_draw_pixel(cx-x,cy-y,color);
    qjs_draw_pixel(cx+y,cy+x,color);qjs_draw_pixel(cx-y,cy+x,color);
    qjs_draw_pixel(cx+y,cy-x,color);qjs_draw_pixel(cx-y,cy-x,color);
}
void qjs_draw_circle(int cx,int cy,int radius,int fill,int border)
{
    if(radius<0)return;
    /* Fill first, draw the border second; interleaving the two causes later
       fill scanlines to erase previously drawn octants of the outline. */
    if(fill>=0){
        int x=radius,y=0,err=1-radius;
        while(x>=y){
            native_rect(cx-x,cy+y,cx+x,cy+y,fill);
            native_rect(cx-x,cy-y,cx+x,cy-y,fill);
            native_rect(cx-y,cy+x,cx+y,cy+x,fill);
            native_rect(cx-y,cy-x,cx+y,cy-x,fill);
            y++;
            if(err<0)err+=2*y+1;
            else{x--;err+=2*(y-x)+1;}
        }
    }
    if(border>=0){
        int x=radius,y=0,err=1-radius;
        while(x>=y){
            octants(cx,cy,x,y,border);
            y++;
            if(err<0)err+=2*y+1;
            else{x--;err+=2*(y-x)+1;}
        }
    }
}
void qjs_draw_ellipse(int x1,int y1,int x2,int y2,int fill,int border)
{
    if(x1>x2){int t=x1;x1=x2;x2=t;}
    if(y1>y2){int t=y1;y1=y2;y2=t;}
    int cx2=x1+x2,cy2=y1+y2,rx2=x2-x1,ry2=y2-y1;
    if(!rx2 || !ry2){qjs_draw_line(x1,y1,x2,y2,border>=0?border:fill);return;}
    int begin=max_i(top,y1),end=min_i(bottom-1,y2);
    /* Integer ellipse equation using doubled center/radii to preserve parity.
       64-bit products fit for bounded JS coordinates. */
    int64_t rx_sq=(int64_t)rx2*rx2,ry_sq=(int64_t)ry2*ry2;
    for(int y=begin;y<=end;y++){
        int64_t dy=2*(int64_t)y-cy2;
        int64_t limit=rx_sq*(ry_sq-dy*dy);
        if(limit<0)continue;
        int dx=rx2;
        while(dx>0 && (int64_t)dx*dx*ry_sq>limit)dx--;
        int l=(cx2-dx+1)/2,r=(cx2+dx)/2;
        /* Signed division rounds toward zero; adjust when left of origin. */
        if(cx2-dx<0 && ((cx2-dx)&1))l=(cx2-dx)/2;
        if(cx2+dx<0 && ((cx2+dx)&1))r=(cx2+dx-1)/2;
        if(fill>=0)native_rect(l,y,r,y,fill);
        if(border>=0){qjs_draw_pixel(l,y,border);qjs_draw_pixel(r,y,border);}
    }
}
void qjs_draw_poly(const int *xy,unsigned vertices,int fill,int border)
{
    if(vertices<3 || vertices>32)return;
    int lo=xy[1],hi=xy[1];
    for(unsigned i=1;i<vertices;i++){
        lo=min_i(lo,xy[2*i+1]);hi=max_i(hi,xy[2*i+1]);
    }
    if(fill>=0){
        int intersections[32];
        for(int y=max_i(top,lo);y<=min_i(bottom-1,hi);y++){
            unsigned count=0;
            for(unsigned i=0;i<vertices;i++){
                unsigned j=(i+1)%vertices;
                int x0=xy[2*i],y0=xy[2*i+1];
                int x1=xy[2*j],y1=xy[2*j+1];
                /* Sample scanline at y + 0.5; half-open edge crossing. */
                if(y0==y1 || (y0>y)==(y1>y) || !((y0<=y && y1>y)||(y1<=y && y0>y)))
                    continue;
                int64_t numerator=(int64_t)(2*y+1-2*y0)*(x1-x0);
                int x=x0+(int)(numerator/(2*(int64_t)(y1-y0)));
                intersections[count++]=x;
            }
            for(unsigned i=1;i<count;i++){
                int x=intersections[i];unsigned j=i;
                while(j && intersections[j-1]>x){intersections[j]=intersections[j-1];j--;}
                intersections[j]=x;
            }
            for(unsigned i=0;i+1<count;i+=2)
                native_rect(intersections[i],y,intersections[i+1],y,fill);
        }
    }
    if(border>=0)for(unsigned i=0;i<vertices;i++){
        unsigned j=(i+1)%vertices;
        qjs_draw_line(xy[2*i],xy[2*i+1],xy[2*j],xy[2*j+1],border);
    }
}
