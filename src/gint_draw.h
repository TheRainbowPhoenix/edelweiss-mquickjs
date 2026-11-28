#ifndef QJS_GINT_DRAW_H
#define QJS_GINT_DRAW_H
#include "edl.h"

/* Renderer bounded to the active EX-word framebuffer and optional clip.
   C_NONE is -1 for fill/border arguments, matching PythonExtra's intent. */
void qjs_draw_init(const struct edl_api *host);
void qjs_draw_window(int left,int top,int right,int bottom);
void qjs_draw_get_window(int bounds[4]);
void qjs_draw_clear(int color);
void qjs_draw_rect(int x1,int y1,int x2,int y2,int color);
void qjs_draw_pixel(int x,int y,int color);
void qjs_draw_line(int x1,int y1,int x2,int y2,int color);
void qjs_draw_hline(int y,int color);
void qjs_draw_vline(int x,int color);
void qjs_draw_rect_border(int x1,int y1,int x2,int y2,int fill,int width,int border);
void qjs_draw_circle(int cx,int cy,int radius,int fill,int border);
void qjs_draw_ellipse(int x1,int y1,int x2,int y2,int fill,int border);
void qjs_draw_poly(const int *xy,unsigned vertices,int fill,int border);
#endif
