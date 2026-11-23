#include "gint_draw.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 32
#define HEIGHT 24
static uint16_t pixels[HEIGHT][WIDTH];
static unsigned calls;
static void rect(int x,int y,int w,int h,uint16_t color)
{
    assert(x>=0 && y>=0 && w>0 && h>0);
    assert(x+w<=WIDTH && y+h<=HEIGHT);
    calls++;
    for(int row=y;row<y+h;row++)
        for(int col=x;col<x+w;col++)
            pixels[row][col]=color;
}
static const struct edl_api mock={
    .width=WIDTH,.height=HEIGHT,.rect=rect
};
int main(void)
{
    qjs_draw_init(&mock);
    qjs_draw_clear(0xffff);
    assert(pixels[0][0]==0xffff && pixels[23][31]==0xffff);
    int bounds[4];
    qjs_draw_window(2,3,30,20);
    qjs_draw_get_window(bounds);
    assert(bounds[0]==2 && bounds[1]==3 && bounds[2]==30 && bounds[3]==20);
    qjs_draw_rect(-99,-99,6,6,0x1234);
    assert(pixels[3][2]==0x1234 && pixels[6][6]==0x1234);
    assert(pixels[2][2]==0xffff && pixels[3][1]==0xffff);
    qjs_draw_pixel(29,19,0x44);
    assert(pixels[19][29]==0x44);
    qjs_draw_pixel(30,19,0x45);
    assert(pixels[19][30]==0xffff);
    qjs_draw_line(0,3,31,3,0x10);
    assert(pixels[3][2]==0x10 && pixels[3][29]==0x10);
    qjs_draw_line(2,4,6,8,0x11);
    assert(pixels[5][3]==0x11 && pixels[8][6]==0x11);
    qjs_draw_hline(19,0x12);
    assert(pixels[19][2]==0x12 && pixels[19][30]==0xffff);
    qjs_draw_vline(29,0x13);
    assert(pixels[3][29]==0x13 && pixels[20][29]==0xffff);
    qjs_draw_rect_border(4,4,11,11,0x22,2,0x33);
    assert(pixels[4][4]==0x33 && pixels[5][5]==0x33);
    assert(pixels[6][6]==0x22);
    qjs_draw_rect(20,4,28,11,0xabab);
    qjs_draw_rect_border(20,4,28,11,-1,2,0x99);
    assert(pixels[6][23]==0xabab); /* transparent center preserved */
    assert(pixels[4][22]==0x99 && pixels[10][21]==0x99);
    qjs_draw_circle(16,11,4,0x44,0x55);
    assert(pixels[11][16]==0x44);
    assert(pixels[11][20]==0x55 && pixels[15][16]==0x55);
    qjs_draw_ellipse(6,8,20,18,0x66,0x77);
    assert(pixels[13][13]==0x66);
    assert(pixels[13][6]==0x77 && pixels[13][20]==0x77);
    int tri[]={3,5,14,5,8,16};
    qjs_draw_poly(tri,3,0x88,0x99);
    assert(pixels[7][8]==0x88);
    assert(pixels[5][3]==0x99);
    unsigned count=calls;
    qjs_draw_circle(15,15,3,-1,-1);
    qjs_draw_ellipse(3,4,10,10,-1,-1);
    assert(calls==count);
    qjs_draw_window(-10,-10,100,100);
    qjs_draw_get_window(bounds);
    assert(bounds[0]==0 && bounds[1]==0 && bounds[2]==WIDTH && bounds[3]==HEIGHT);
    puts("PASS: gint graphics clipping, shapes, polygon and C_NONE sentinel");
    return 0;
}
