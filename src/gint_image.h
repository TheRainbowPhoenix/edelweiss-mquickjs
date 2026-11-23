#ifndef QJS_GINT_IMAGE_H
#define QJS_GINT_IMAGE_H
/* Bounded native RGB565 image pool: objects live until one JS invocation ends.
   Avoid pointers in JS and avoid re-decoding bitmap arrays on every frame. */
void qjs_image_reset(void);
int qjs_image_create(int width,int height);
int qjs_image_set_pixel(int id,int at,unsigned color);
int qjs_image_draw(int id,int x,int y,int sx,int sy,int width,int height);
#endif
