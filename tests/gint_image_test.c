#include "gint_image.h"
#include "gint_draw.h"
#include <gint/image.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned draws;
static int last_x,last_y,last_sx,last_sy,last_w,last_h;
static unsigned last_pixel;
void edl_native_dsubimage(int x,int y,const image_t *img,int sx,int sy,int w,int h,int flags)
{
    (void)flags;
    assert(img->format==IMAGE_RGB565 && img->stride%4==0);
    assert(sx>=0&&sy>=0&&w>0&&h>0);
    assert(sx+w<=img->width && sy+h<=img->height);
    draws++;
    last_x=x;last_y=y;last_sx=sx;last_sy=sy;last_w=w;last_h=h;
    uint16_t *row=(uint16_t *)((unsigned char *)img->data+
                              (unsigned)sy*(unsigned)img->stride);
    last_pixel=row[sx];
}
int main(void)
{
    static struct edl_api host={.width=528,.height=320};
    qjs_draw_init(&host);
    qjs_image_reset();
    int id=qjs_image_create(5,4);
    assert(id==1);
    assert(qjs_image_set_pixel(id,0,0x1234)==0);
    assert(qjs_image_set_pixel(id,7,0xbeef)==0);
    assert(qjs_image_set_pixel(id,20,0)<0);
    assert(qjs_image_draw(id,10,15,2,1,2,2)==0);
    assert(draws==1 && last_x==10 && last_y==15);
    assert(last_sx==2 && last_sy==1 && last_w==2 && last_h==2);
    assert(last_pixel==0xbeef);
    assert(qjs_image_draw(id,10,15,4,3,2,2)<0);
    assert(qjs_image_draw(id,10,15,-1,0,2,2)<0);
    qjs_draw_window(12,16,16,20);
    assert(qjs_image_draw(id,10,15,1,0,4,4)==0);
    assert(draws==2 && last_x==12 && last_y==16);
    assert(last_sx==3 && last_sy==1 && last_w==2 && last_h==3);
    qjs_image_reset();
    assert(qjs_image_draw(id,0,0,0,0,2,2)<0);
    assert(qjs_image_create(999,2)<0);
    puts("PASS: native image pool, pixel stride and clipped source regions");
    return 0;
}
