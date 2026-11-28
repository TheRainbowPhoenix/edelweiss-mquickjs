#include "gint_draw.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int native_pixels, native_lines, host_rects;
static void rect(int x,int y,int w,int h,uint16_t c)
{
    (void)c;
    assert(x>=0 && y>=0 && w>0 && h>0);
    assert(x+w<=32 && y+h<=24);
    host_rects++;
}
/* When built with -D__sh__ these stubs stand in for hosted gint imports. */
void dpixel(int x,int y,int c)
{
    (void)c;
    assert(x>=0 && x<32 && y>=0 && y<24);
    native_pixels++;
}
void dline(int x1,int y1,int x2,int y2,int c)
{
    (void)c;
    assert(x1>=0 && x1<32 && x2>=0 && x2<32);
    assert(y1>=0 && y1<24 && y2>=0 && y2<24);
    native_lines++;
}
static const struct edl_api api={.width=32,.height=24,.rect=rect};

int main(void)
{
    qjs_draw_init(&api);
    qjs_draw_pixel(4,4,0);
    assert(native_pixels==1);
    assert(host_rects==0);

    qjs_draw_line(2,3,23,16,0);
    assert(native_lines==1 && host_rects==0);

    qjs_draw_window(4,4,20,18);
    qjs_draw_line(4,4,19,17,0);
    assert(native_lines==2);
    qjs_draw_pixel(3,3,0); /* logical clip exclusion */
    assert(native_pixels==1 && host_rects==0);

    /* Diagonal endpoint outside clip forces safe Bresenham fallback, yet
       every individual in-clip pixel still uses resident native dpixel. */
    qjs_draw_line(0,6,17,15,0);
    assert(native_lines==2);
    assert(native_pixels>1);
    /* C_NONE must never reach the native routines. */
    int np=native_pixels,nl=native_lines,nr=host_rects;
    qjs_draw_line(4,4,12,12,-1);
    qjs_draw_pixel(5,5,-1);
    assert(native_pixels==np && native_lines==nl && host_rects==nr);
    puts("PASS: native line and pixel fastpath plus clipped fallback");
    return 0;
}
