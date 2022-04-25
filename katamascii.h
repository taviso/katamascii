#ifndef __KATAMASCII_H
#define __KATAMASCII_H

sprite_t *katamascii_get_player();

typedef struct {
    const char *name;
    const char *author;
    const char *comment;
    const char *filename;
    uint32_t mass;
    uint32_t width;
    uint32_t height;
} crushable_t;

typedef struct {
    uint32_t     mass;
    crushable_t *lastobject;
} katamascii_t;

#endif
