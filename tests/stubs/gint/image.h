#ifndef QJS_TEST_GINT_IMAGE_H
#define QJS_TEST_GINT_IMAGE_H
#include <stdint.h>
typedef struct {
    uint8_t format,flags;
    int16_t color_count;
    uint16_t width,height;
    int stride;
    void *data;
    uint16_t *palette;
} image_t;
#define IMAGE_RGB565 0
#define IMAGE_FLAGS_DATA_RO 1
#endif
