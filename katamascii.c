#include <stdlib.h>
#include <ncurses.h>
#include <caca.h>
#include <locale.h>
#include <chipmunk.h>
#include <glib.h>

#include "compat.h"
#include "sprite.h"
#include "effects.h"
#include "katamascii.h"

static sprite_t *player;

crushable_t objects[] = {
    {
      .name     = "Owl",
      .author   = "Donovoan Bake",
      .comment  = "Owls can rotate their necks 270 degrees.",
      .filename = "art/owl.raw",
      .width    = 6,
      .height   = 4,
      .mass     = 15,
    },
    {
      .name     = "Duckling",
      .author   = "Unknown",
      .filename = "art/chick.raw",
      .width    = 4,
      .height   = 2,
      .mass     = 6,
    },
    {
      .name     = "Penguin",
      .author   = "Linda Ball",
      .filename = "art/penguin.raw",
      .width    = 4,
      .height   = 3,
      .mass     = 10,
    },
    {
      .name     = "Pizza",
      .author   = "Joan Stark",
      .comment  = "Pizza is delicious.",
      .filename = "art/pizza.raw",
      .width    = 18,
      .height   = 5,
      .mass     = 49,
    },
    {
      .name     = "Cactus",
      .author   = "kra",
      .comment  = "You can buy cactus leaves in the supermarket in California!",
      .filename = "art/cactus.raw",
      .width    = 12,
      .height   = 8,
      .mass     = 35,
    },
    {
      .name     = "Bed",
      .author   = "Duchamp",
      .comment  = "Yawwwwwwwn",
      .filename = "art/bed.raw",
      .width    = 26,
      .height   = 15,
      .mass     = 185,
    },
    {
      .name     = "Chair",
      .author   = "Normand Veillaux",
      .filename = "art/chair.raw",
      .width    = 10,
      .height   = 8,
      .mass     = 44,
    },
    {
      .name     = "Sofa",
      .author   = "Joan Stark",
      .filename = "art/sofa.raw",
      .width    = 19,
      .height   = 6,
      .mass     = 81,
    }
};

static int coin_collision(sprite_t *coin, sprite_t *object, gpointer arb)
{
    // Experimenting with death animations...
    if (strcmp(object->shortname, "player") == 0) {
        sprite_set_bg(coin, "art/coin2.raw");
        coin->flags |= SPRITE_FLAG_DYING;
        for (int i = 1; i < 5; i++) {
            caca_create_frame(coin->cv, i);
        }
    }

    return true;
}

static int object_collision(sprite_t *object, sprite_t *a, gpointer arb)
{
    if (strcmp(a->shortname, "player") == 0) {
        katamascii_t *data = a->user;
        // Convert this object to dust!
        cpSpaceAddPostStepCallback(cpBodyGetSpace(object->body),
                                   (cpPostStepFunc) convert_object_magic_dust,
                                   object,
                                   NULL);
        data->lastobject = object->user;
    }
    return true;
}

static sprite_t * load_art_object(int x, int y, crushable_t *object)
{
    sprite_t *o = sprite_new_box_full(1,
                                      cmpx(object->width),
                                      object->height,
                                      x,
                                      y);
    sprite_set_bg(o, object->filename);
    sprite_add_callback(o, collision.begin, object_collision);
    o->user = object;
    return o;
}

sprite_t * katamascii_get_player()
{
    return player;
}

int main(int argc, char **argv)
{
    WINDOW *hud;
    katamascii_t data = {0};
    sprite_t *platform;
    cpSpace *space;
    int ch;

    space = cpSpaceNew();
    sprite_init(space, stdscr);
    cpSpaceSetGravity(space, cpv(0, -75));
    cpSpaceSetDamping(space, 0.00001);

    platform = sprite_new_box_full(1, 110, 3, 30, 10);

    player = sprite_new_circle_full(1, 1, 30, 1);

    sprite_set_name(player, "player");
    player->user = &data;

    sprite_set_name(platform, "platform");

    sprite_anchor(platform);

    //sprite_set_bg(player, "art/pool.utf");

    for (int i = 0; i < 2; i++) {
        sprite_t *backdrop = sprite_new_box_full(1,
                                                 40,
                                                 16,
                                                 30 + i * 39,
                                                 -12);
        sprite_background(backdrop);
        sprite_set_bg(backdrop, "tiles/city.utf");
        sprite_set_name(backdrop, "backdrop");
    }

    hud = newwin(8, getmaxx(stdscr), getmaxy(stdscr) - 8, 0);

    timeout(1000 / 20);

    while (true) {
        wclear(stdscr);
        wclear(hud);

        // Advance time.
        cpSpaceStep(space, 1.0/20.0);

        sprite_center_camera(player);
        space_update_sprites();

        box(hud, 0, 0);
        mvwprintw(hud, 1, 1, "You currently weigh %u kg, that is %u 🍔",
                             data.mass,
                             (int)(data.mass / 0.240));
        mvwprintw(hud, 2, 1, "Your radius is %fm, is that chonk?",
                             cpCircleShapeGetRadius(player->shape));


        if (data.lastobject) {
            mvwprintw(hud, 3, 1, "You just crushed %s by %s, it weighed %u kg.",
                                 data.lastobject->name,
                                 data.lastobject->author,
                                 data.lastobject->mass);
            if (data.lastobject->comment) {
                mvwprintw(hud, 4, 1, "Did you know... %s",
                                     data.lastobject->comment);
            }
        }

        wrefresh(stdscr);
        wrefresh(hud);

        // You can't really hold two keys simultaneously in ncurses.
        // Let's have a gish like character who follows walls?

        switch (ch = getch()) {
            case KEY_F(1):
                goto finish;
            case KEY_F(3): {
                    cpVect pos = cpBodyGetPos(player->body);
                    static int i;
                    load_art_object(pos.x,
                                    getmaxy(stdscr) - pos.y - 10,
                                    &objects[i++ % G_N_ELEMENTS(objects)]);
                break;
            }
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
            case ERR:
                break;
        }
        // TODO: if everything is sleeping, no need to redraw, just jump back
        // to getch()?
    }

finish:
    sprite_fini();
    cpSpaceFree(space);

    return 0;
}
