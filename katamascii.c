#include <stdlib.h>
#include <ncurses.h>
#include <caca.h>
#include <locale.h>
#include <chipmunk.h>
#include <glib.h>

#include "compat.h"
#include "sprite.h"

// TODO:
//      - Support animation frames
//      - Collission should start an animation sequence with coins,
//          - maybe set a flag like DYING
//          - when an animation sequence completes and DYING is set, delete object.
//
//      - I dont know what to do about circles, the shape isnt right because of char ratio.
//      - should I make one space unit != one curses unit?
//      - If nothing is moving, put timeout() back up to normal.

static int collision_begin(cpArbiter *arb, struct cpSpace *space, cpDataPointer data)
{
    cpBody *bodya, *bodyb;
    sprite_t *spritea, *spriteb;

    cpArbiterGetBodies(arb, &bodya, &bodyb);

    spritea = cpBodyGetUserData(bodya);
    spriteb = cpBodyGetUserData(bodyb);

    // All bodies should have a sprite?
    g_return_val_if_fail(spritea, true);
    g_return_val_if_fail(spriteb, true);

    // Background or decorative sprites.
    if (spritea->flags & SPRITE_FLAG_NOCLIP)
        return false;
    if (spriteb->flags & SPRITE_FLAG_NOCLIP)
        return false;

    g_debug("collission %s => %s", spritea->shortname, spriteb->shortname);

    // Experimenting with death animations...
    if (strcmp(spritea->shortname, "coin") == 0) {
        if (strcmp(spriteb->shortname, "player") == 0) {
            sprite_set_bg(spritea, "art/coin2.raw");
            spritea->flags |= SPRITE_FLAG_DYING;
            for (int i = 1; i < 5; i++) {
                caca_create_frame(spritea->cv, i);
            }
        }
    }

    if (strcmp(spritea->shortname, "player") == 0) {
        if (strcmp(spriteb->shortname, "coin") == 0) {
            sprite_set_bg(spriteb, "art/coin2.raw");
            spriteb->flags |= SPRITE_FLAG_DYING;
            for (int i = 1; i < 5; i++) {
                caca_create_frame(spriteb->cv, i);
            }
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    GSList *coins = NULL;
    GSList *backdrops = NULL;
    GSList *objects = NULL;
    sprite_t *platform;
    sprite_t *player;
    cpSpace *space;
    int ch;

    sprite_init();

    space = cpSpaceNew();
    cpSpaceSetGravity(space, cpv(0, -75));
    cpSpaceSetDamping(space, 0.00001);

    cpSpaceSetDefaultCollisionHandler(space, collision_begin, NULL, NULL, NULL, NULL);

    platform = sprite_new_box_full(space, 1, 110, 3, 30, 10);

    player = sprite_new_circle_full(space, 5, 1, 30, 1);

    sprite_set_name(player, "player");
    sprite_set_name(platform, "platform");

    sprite_anchor(platform, space);

    sprite_set_bg(player, "art/pool.utf");

    for (int i = 0; i < 2; i++) {
        sprite_t *backdrop = sprite_new_box_full(space,
                                                 1,
                                                 40,
                                                 16,
                                                 30 + i * 39,
                                                 -12);
        sprite_background(backdrop);
        sprite_set_bg(backdrop, "tiles/city.utf");
        sprite_set_name(backdrop, "backdrop");
        backdrops = g_slist_append(backdrops, backdrop);
    }

    timeout(1000 / 20);

    while (true) {
        cpBB bb;
        wclear(stdscr);

        // Draw backdrops.
        for (GSList *curr = backdrops; curr; curr = curr->next) {
            sprite_redraw(curr->data);
            sprite_refresh(curr->data);
        }
        // Draw Objects
        for (GSList *curr = objects; curr; curr = curr->next) {
            sprite_redraw(curr->data);
            sprite_refresh(curr->data);
        }

        sprite_redraw_rotated(player);

        sprite_refresh(platform);
        sprite_refresh(player);

        for (GSList *curr = coins; curr; curr = curr->next) {
            // Check if sprite is still valid.
            if (curr->data == NULL)
                continue;
            // Is the sprite valid, but dead?
            if (sprite_is_dead(curr->data)) {
                sprite_destroy(curr->data);
                curr->data = NULL;
                continue;
            }
            // Sprite is alive and valid, draw it.
            sprite_redraw(curr->data);
            sprite_refresh(curr->data);
        }

        // Learn position;
        bb = cpShapeCacheBB(player->shape);

        mvwprintw(stdscr, 0, 0, "player@(%f,%f) %svisible",
                                bb.t,
                                bb.l,
                                sprite_visible(player) ? "" : "in");
        mvwprintw(stdscr, 1, 0, "camera@(%f,%f)", camera.x, camera.y);

        wrefresh(stdscr);

        // You can't really hold two keys simultaneously in ncurses.
        // Let's have a gish like character who follows walls?

        switch (ch = getch()) {
            case KEY_F(1):
                goto finish;
            case KEY_F(3):
                // Objects
                {
                    static int i;
                    sprite_t *o;
                    switch (i++ % 4) {
                        case 0:
                    //                             m w/2  h  x   y
                    o = sprite_new_box_full(space, 1, 9, 13, bb.l + 5, getmaxy(stdscr) - bb.t - 10);
                    sprite_set_bg(o, "art/plant.raw");
                    objects = g_slist_append(objects, o);
                        break;
                        case 1:
                    o = sprite_new_box_full(space, 1, 10, 8, bb.l + 5, getmaxy(stdscr) - bb.t - 10);
                    sprite_set_bg(o, "art/sofa.raw");
                    objects = g_slist_append(objects, o);
                        break;
                        case 2:
                    o = sprite_new_box_full(space, 1, 5, 8, bb.l + 5, getmaxy(stdscr) - bb.t - 10);
                    sprite_set_bg(o, "art/chair.raw");
                    objects = g_slist_append(objects, o);
                        break;
                        case 3:
                    o = sprite_new_box_full(space, 1, 6, 8, bb.l + 5, getmaxy(stdscr) - bb.t - 10);
                    sprite_set_bg(o, "art/cactus.raw");
                    objects = g_slist_append(objects, o);
                        break;
                    }
                }
                break;
            case KEY_F(4):

                for (int i = 0; i < 16; i++) {
                    sprite_t *coin = sprite_new_circle_full(space,
                                                            0,
                                                            1,
                                                            bb.l + 5,
                                                            getmaxy(stdscr) - bb.t - 10);
                    sprite_set_bg(coin, "art/coin.raw");
                    sprite_set_name(coin, "coin");
                    coins = g_slist_append(coins, coin);
                }
                break;


            case KEY_RIGHT:
                cpBodyApplyImpulse(player->body,
                                   cpv(35, 0),
                                   cpv(0, 0));
                break;
            case KEY_LEFT:
                cpBodyApplyImpulse(player->body,
                                   cpv(-35, 0),
                                   cpv(0, 0));
                break;
            case KEY_UP:
                cpBodyApplyImpulse(player->body,
                                   cpv(0, 10),
                                   cpv(0, 0));
                break;
            case KEY_DOWN:
                cpBodyApplyImpulse(player->body,
                                   cpv(0, -10),
                                   cpv(0, 0));
            case ERR:
                break;
        }

        cpSpaceStep(space, 1.0/20.0);
        sprite_center_camera(player);
        space_update_sprites(space);

        // TODO: if everything is sleeping, no need to redraw, just jump back to getch()?
    }

finish:
    g_slist_free_full(coins, (GDestroyNotify) sprite_destroy);
    g_slist_free_full(backdrops, (GDestroyNotify) sprite_destroy);
    g_slist_free_full(objects, (GDestroyNotify) sprite_destroy);
    sprite_destroy(platform);
    sprite_destroy(player);
    cpSpaceFree(space);
    sprite_fini();

    return 0;
}
