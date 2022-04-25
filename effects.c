#include <stdlib.h>
#include <ncurses.h>
#include <caca.h>
#include <locale.h>
#include <chipmunk.h>
#include <glib.h>

#include "compat.h"
#include "sprite.h"
#include "katamascii.h"

static void increase_player_radius(cpSpace *space, sprite_t *player, void *user)
{
    katamascii_t *data = player->user;
    int radius = data->mass / 100;


    // We might have been called multiple times, check if this is needed.
    if (radius + 1 <= cpCircleShapeGetRadius(player->shape))
        return;

    // Remove the current shape.
    cpSpaceRemoveShape(space, player->shape);
    cpShapeFree(player->shape);

    // Create a new shape.
    player->shape = cpCircleShapeNew(player->body, radius, cpvzero);

    // Configure default settings.
    cpShapeSetUserData(player->shape, player);
    cpShapeSetFriction(player->shape, 1);
    cpSpaceAddShape(space, player->shape);

    // Adjust canvas/window accordingly.
    wresize(player->win, radius * 2 + 1, ttypx(radius * 2) + 1);
    caca_set_canvas_size(player->cv, getmaxx(player->win), getmaxy(player->win));

    // Update graphics.
    caca_clear_canvas(player->cv);
    //caca_draw_thin_ellipse(player->cv,
    //                       ttypx(radius),
    //                       radius,
    //                       ttypx(radius),
    //                       radius);
    caca_fill_ellipse(player->cv, ttypx(radius), radius, ttypx(radius), radius, '@');
}


// The object is being absorbed, so here is the algorithm:
//      - Each char becomes sprite that is magnetically attracted to the player.
//      - When it touches the player, it turns green and gets absorbed.
//      - player->mass++;

static int particle_collision_begin(sprite_t *a, sprite_t *b, gpointer arb)
{
    g_assert_cmpstr(a->shortname, ==, "particle");

    if (strcmp(b->shortname, "player") == 0) {
        int count = caca_get_frame_count(a->cv);
        katamascii_t *data = b->user;
        for (int i = 0; i < count; i++) {
            caca_free_frame(a->cv, 0);
        }
        caca_clear_canvas(a->cv);
        caca_set_color_ansi(a->cv, CACA_GREEN, CACA_TRANSPARENT);
        caca_put_char(a->cv, 0, 0, '*');
        for (int i = 0; i < 4; i++) {
            caca_create_frame(a->cv, 0);
        }

        // Dont keep interacting with the player after this.
        a->flags |= SPRITE_FLAG_NOCLIP;

        // Credit player for the collected mass.
        if (data->mass++ > 100 * cpCircleShapeGetRadius(b->shape)) {
            cpSpaceAddPostStepCallback(cpBodyGetSpace(b->body),
                                       (cpPostStepFunc) increase_player_radius,
                                       b,
                                       NULL);
        }
    }
    return false;
}

static void particle_velocity_func(cpBody *body, cpVect gravity, cpFloat damping, cpFloat dt)
{
    sprite_t *particle = cpBodyGetUserData(body);
    sprite_t *player = katamascii_get_player();
    cpVect dest = cpBodyGetPos(player->body);
    cpBB srcbb = cpShapeCacheBB(particle->shape);
    cpVect src = cpBBCenter(srcbb);
    cpFloat g = cpvdistsq(dest, src);

    if (dest.x < src.x) gravity.x = -g;
    if (dest.x > src.x) gravity.x = +g;
    if (dest.y < src.y) gravity.y = -g;
    if (dest.y > src.y) gravity.y = +g;

    cpBodyUpdateVelocity(body, gravity, damping, dt);
    return;
}

void convert_object_magic_dust(cpSpace *space, sprite_t *object, void *data)
{
    cpBB bb;
    const uint32_t *chars;
    const uint32_t *attrs;
    int width, height;
    int posx, posy;

    bb     = cpShapeCacheBB(object->shape);
    width  = caca_get_canvas_width(object->cv);
    height = caca_get_canvas_height(object->cv);
    chars  = caca_get_canvas_chars(object->cv);
    attrs  = caca_get_canvas_attrs(object->cv);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (chars[y * width + x] != ' ') {
                // Create a new 1x1 box to draw the particles.
                sprite_t *particle = sprite_new_box_full(1, 1, 1, bb.l + cmpx(x), getmaxy(stdscr) - bb.t + y);

                sprite_set_name(particle, "particle");

                // Remove any default image.
                caca_clear_canvas(particle->cv);

                // Change color
                caca_set_color_ansi(particle->cv, CACA_RED, CACA_TRANSPARENT);

                // Put the right character.
                caca_put_char(particle->cv, 0, 0, chars[y * width + x]);

                // The particles are all dying.
                particle->flags |= SPRITE_FLAG_DYING;

                // Let me know if something touches it.
                sprite_add_callback(particle, collision.begin, particle_collision_begin);

                // Create a death animation?
                for (int i = 0; i < 64; i++) {
                    caca_create_frame(particle->cv, 0);
                }

                // The particules should gravitate towards the player.
                cpBodySetVelocityUpdateFunc(particle->body, particle_velocity_func);
            }
        }
    }

    // Okay, now mark the original as dying.
    object->flags |= SPRITE_FLAG_DYING;

    return;
}
