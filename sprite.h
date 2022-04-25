#ifndef __SPRITE_H
#define __SPRITE_H

extern cpVect camera;
extern const float kPixelRatio;

enum {
    SPRITE_FLAG_DYING       = 1 << 0,
    SPRITE_FLAG_NOCLIP      = 1 << 1,
    SPRITE_FLAG_ROTATES     = 1 << 2,
    SPRITE_FLAG_DESPAWN     = 1 << 3,
};

enum {
    SPRITE_ID_PLAYER,
};

typedef struct _sprite {
    caca_canvas_t   *cv;
    WINDOW          *win;
    cpShape         *shape;
    cpBody          *body;
    char             shortname[8];
    uint32_t        type;
    uint32_t        flags;
    uint16_t        layer;
    gpointer        user;
    struct {
        struct {
            GSList *begin;
            GSList *presolve;
            GSList *postsolve;
            GSList *separate;
        } collision;
        GSList *update;
        GSList *destroy;
    } callbacks;
} sprite_t;

typedef int (*spritecb_t)(sprite_t *, sprite_t *, gpointer);

sprite_t *sprite_new_circle_full(int radius,
                                 int mass,
                                 int x,
                                 int y);

sprite_t *sprite_new_box_full(int mass,
                              int width,
                              int height,
                              int x,
                              int y);

sprite_t *sprite_new_line_full(int mass,
                               int x1,
                               int y1,
                               int x2,
                               int y2);

void sprite_destroy(sprite_t *sprite);

void sprite_redraw(sprite_t *sprite);

void sprite_redraw_rotated(sprite_t *sprite);

void sprite_refresh(sprite_t *sprite);

void sprite_anchor(sprite_t *sprite);

// Mark this sprite as decorative, it shouldnt interact with
// other objects.
void sprite_background(sprite_t *sprite);

// Load the specified ascii art file and draw it to the sprite.
// You can still write to or modify the canvas afterwards.
void sprite_set_bg(sprite_t *sprite, const char *filename);

void canvas_display_window(caca_canvas_t *cv, WINDOW *win);

void sprite_init(cpSpace *space, WINDOW *window);

void sprite_fini();

// Check if the camera can see the specified sprite.
bool sprite_visible(sprite_t *sprite);

// Query the physics engine for the current rotation in degrees.
int sprite_rotation(sprite_t *sprite);


// Move the camera to center on the specified sprite.
void sprite_center_camera(sprite_t *sprite);


// Give the sprite a name that can be queried later.
void sprite_set_name(sprite_t *sprite, const char *name);

// Find all the sprites in the specified space and update their status.
void space_update_sprites();

// Check if a sprite is dead, i.e. is dying and death animation is complete.
bool sprite_is_dead(sprite_t *sprite);

static inline int ttypx(int width)
{
    return width * kPixelRatio;
}

static inline int cmpx(int width)
{
    return width / kPixelRatio;
}

#define sprite_add_callback(s, cb, func) do {                   \
    s->callbacks.cb = g_slist_append(s->callbacks.cb, func);    \
} while (false)

#endif
