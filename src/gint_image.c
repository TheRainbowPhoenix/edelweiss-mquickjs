#include "gint_image.h"
#include <gint/display.h>
#include <gint/image.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define IMAGE_SLOTS 24
#define IMAGE_BYTES_LIMIT (128u*1024u)
struct bitmap {
    image_t image;
    uint16_t *storage;
};
static struct bitmap slots[IMAGE_SLOTS];
static unsigned count,used_bytes;

void qjs_image_reset(void)
{
    for(unsigned i=0;i<count;i++)free(slots[i].storage);
    memset(slots,0,sizeof slots);
    count=used_bytes=0;
}
int qjs_image_create(int width,int height)
{
    if(width<1||height<1||width>256||height>256||count>=IMAGE_SLOTS)
        return -1;
    unsigned stride=((unsigned)width*2u+3u)&~3u;
    unsigned size=stride*(unsigned)height;
    if(size>IMAGE_BYTES_LIMIT-used_bytes)return -1;
    uint16_t *bytes=malloc(size);
    if(!bytes)return -1;
    memset(bytes,0,size);
    unsigned index=count++;
    used_bytes+=size;
    slots[index].storage=bytes;
    slots[index].image=(image_t){
        .format=IMAGE_RGB565,
        .flags=IMAGE_FLAGS_DATA_RO,
        .width=(uint16_t)width,
        .height=(uint16_t)height,
        .stride=(int)stride,
        .data=bytes
    };
    return (int)index+1;
}
int qjs_image_set_pixel(int id,int at,unsigned color)
{
    if(id<=0||(unsigned)id>count)return -1;
    image_t *img=&slots[id-1].image;
    unsigned total=(unsigned)img->width*img->height;
    if(at<0||(unsigned)at>=total)return -1;
    unsigned row=(unsigned)at/img->width,col=(unsigned)at%img->width;
    uint16_t *line=(uint16_t *)((unsigned char *)img->data+row*(unsigned)img->stride);
    line[col]=(uint16_t)color;
    return 0;
}
int qjs_image_draw(int id,int x,int y,int sx,int sy,int width,int height)
{
    if(id<=0||(unsigned)id>count)return -1;
    image_t *img=&slots[id-1].image;
    if(sx<0||sy<0||width<1||height<1||sx>img->width||sy>img->height||
       width>img->width-sx||height>img->height-sy)return -1;
    dsubimage(x,y,img,sx,sy,width,height,DIMAGE_NONE);
    return 0;
}
