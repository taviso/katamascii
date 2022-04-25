#include <glib.h>
#include <caca.h>
#include <ncurses.h>
#include <locale.h>
#include <chipmunk.h>
#include <sys/ioctl.h>
#include "compat.h"
#include "sprite.h"

const float kPixelRatio = 2.0f;
static cpSpace *kSpace;
static GSList *objects;
cpVect camera;

// Unfortunately character cells are not square. We need to adjust coordinates
// and widths accordingly. The reason for this is otherwise circles wont look
// cicular.
//
// I think the solution is to count 1 pixel as 2 horizontal blocks.
//
//   1x1 px: ██
//
// Yes, this is a chunky boy, but it's far easier to reason about.
//
// XXX I don't know how to do this.

static gint sprite_layer_compare(sprite_t *a, sprite_t *b)
{
    return -1 * (a->layer - b->layer);
}

void sprite_set_layer(sprite_t *sprite, uint32_t layer)
{
    sprite->layer = layer;

    objects = g_slist_sort(objects, (GCompareFunc) sprite_layer_compare);
}

void sprite_set_name(sprite_t *sprite, const char *name)
{
    strncpy(sprite->shortname, name, sizeof(sprite->shortname));
}

static void space_count_body(cpBody *body, int *bodycount)
{
    ++*bodycount;
}

static int space_count_bodies()
{
    int numbodies = 0;

    cpSpaceEachBody(kSpace,
                    (cpSpaceBodyIteratorFunc) space_count_body,
                    &numbodies);

    return numbodies;
}

bool sprite_is_dead(sprite_t *sprite)
{
    if (sprite->flags & SPRITE_FLAG_DYING) {
        return caca_get_frame_count(sprite->cv) == 1;
    }
    return false;
}

static void space_update_body(cpBody *body, cpSpace *space)
{
    sprite_t *sprite = cpBodyGetUserData(body);

    if (sprite == NULL) {
        g_debug("body %p does not have an associated sprite?", body);
        return;
    }

}

// Query all bodies and see if they need to change animation
// or need to be removed, etc, etc.
void space_update_sprites()
{
    uint32_t dirty = 0;

    // Process each object in space.
    for (GSList *curr = objects; curr; curr = curr->next) {
        sprite_t *sprite = curr->data;

        // If the sprite is dying, it means play out the remaining animation
        // frames and then remove it.
        if (sprite->flags & SPRITE_FLAG_DYING) {

            // Are there any death frames remaining?
            if (caca_get_frame_count(sprite->cv) > 1) {
                g_debug("dying sprite %s has %u frames remaining",
                         sprite->shortname,
                         caca_get_frame_count(sprite->cv));
                // Yes, just remove the current active frame.
                caca_free_frame(sprite->cv, 0);
            } else {
                // This is already the last frame, the sprite is dead.
                g_debug("sprite %s has died", sprite->shortname);

                // We need a cleanup pass after this.
                dirty++;

                curr->data = NULL;

                sprite_destroy(sprite);

                continue;
            }
        }

        // Call any callbacks registered.
        for (GSList *c = sprite->callbacks.update; c; c = c->next) {
            spritecb_t cb = curr->data;
            cb(sprite, NULL, NULL);
        }

        // Perform standard updates.
        sprite_redraw(sprite);
        sprite_refresh(sprite);
    }

    if (dirty) {
        objects = g_slist_remove_all(objects, NULL);
    }
}

static sprite_t *sprite_new(void)
{
    sprite_t *sprite = g_new0(sprite_t, 1);

    // Add to our private list of all known sprites. This is used
    // for processing updates, callbacks, and so on.
    objects = g_slist_append(objects, sprite);

    return sprite;
}
// So there are only a few shapes we have to handle.
//  - lines
//  - polylines
//  - circles
//  - ellipses
//  - boxes
//  - triangles
//
// Everything else is just ascii art inside one of those shapes.
sprite_t *sprite_new_circle_full(int radius,
                                 int mass,
                                 int x,
                                 int y)
{
    cpFloat fradius;
    sprite_t *sprite;
    cpFloat moment;
    cpBB bb;

    // Create the new object.
    sprite = sprite_new();

    // The smallest radius we support.
    fradius = radius ? radius : 0.5f;

    // Create the physics object.
    // XXX: FIXME, this isn't working in chipmunk 7?
#if CP_VERSION_MAJOR < 7
    moment = cpMomentForCircle(mass, 0, fradius, cpvzero);
#else
    moment = cpMomentForCircle(INFINITY, 0, fradius, cpvzero);
#endif
    sprite->body  = cpBodyNew(mass, moment);
    sprite->shape = cpCircleShapeNew(sprite->body, fradius, cpvzero);

    // Setup user pointers so we can find the sprite in chipmunk callbacks.
    cpBodySetUserData(sprite->body, sprite);
    cpShapeSetUserData(sprite->shape, sprite);

    // Set a default friction
    cpShapeSetFriction(sprite->shape, 1);

    // Insert it into space.
    cpSpaceAddBody(kSpace, sprite->body);
    cpSpaceAddShape(kSpace, sprite->shape);

    g_debug("new circle %p r=%d -> space@%p (%d bodies)",
            sprite,
            radius,
            kSpace,
            space_count_bodies());

    // Set the intial position. This is the position
    // of the center of gravity.
    cpBodySetPos(sprite->body, cpv(x, getmaxy(stdscr) - y));

    // Determine the object dimensions.
    bb = cpShapeCacheBB(sprite->shape);

    g_debug("circle %p bounding box is (%f, %f, %f, %f)",
            sprite,
            bb.t,
            bb.l,
            bb.b,
            bb.r);

    // Create an ncurses WINDOW to contain it.
    sprite->win = newpad(fradius * 2 + 1, ttypx(fradius * 2) + 1);

    g_debug("circle %p pad size is %dx%d",
            sprite,
            getmaxx(sprite->win),
            getmaxy(sprite->win));

    // Create a caca canvas to draw it.
    sprite->cv = caca_create_canvas(getmaxx(sprite->win), getmaxy(sprite->win));

    g_debug("circle %p canvas is %dx%d",
            sprite,
            caca_get_canvas_width(sprite->cv),
            caca_get_canvas_height(sprite->cv));

    // Draw an initial sprite.
    if (radius) {
        caca_draw_thin_ellipse(sprite->cv,
                               ttypx(radius),
                               radius,
                               ttypx(radius),
                               radius);
    } else {
        caca_put_str(sprite->cv, 0, 0, "()");
    }

    // Draw that contents to the window.
    sprite_redraw(sprite);

    return sprite;
}

sprite_t *sprite_new_box_full(int mass,
                              int width,
                              int height,
                              int x,
                              int y)
{
    sprite_t *sprite;
    cpFloat moment;
    cpBB bb = {
        .l = x,
        .b = getmaxy(stdscr) - (y + height),
        .t = getmaxy(stdscr) - (y),
        .r = x + width,
    };

    // Create the new object.
    sprite = sprite_new();

    // Create the physics object.
    // Note: set to INFINITY to disable rotation
    moment = cpMomentForBox(INFINITY, width, height);
    sprite->body = cpBodyNew(mass, moment);
    sprite->shape = cpBoxShapeNew2(sprite->body, bb);

    // Setup user pointers so we can find the sprite in chipmunk callbacks.
    cpBodySetUserData(sprite->body, sprite);
    cpShapeSetUserData(sprite->shape, sprite);

    // Set a default friction
    cpShapeSetFriction(sprite->shape, 1);

    // Insert it into space.
    cpSpaceAddBody(kSpace, sprite->body);
    cpSpaceAddShape(kSpace, sprite->shape);

    // Create an ncurses WINDOW to contain it.
    sprite->win = newpad(bb.t - bb.b + 1, ttypx(bb.r - bb.l) + 1);

    // Create a caca canvas to draw it.
    sprite->cv = caca_create_canvas(getmaxx(sprite->win), getmaxy(sprite->win));

    // Draw an initial sprite.
    caca_draw_thin_box(sprite->cv, 0, 0, ttypx(width), height);

    // Draw that contents to the window.
    sprite_redraw(sprite);

    return sprite;
}

sprite_t *sprite_new_line_full(int mass,
                               int x1,
                               int y1,
                               int x2,
                               int y2)
{
    sprite_t *sprite;
    cpFloat moment;
    cpBB bb;

    // Create the new object.
    sprite = sprite_new();

    // Create the physics object.
    moment = cpMomentForSegment(mass, cpv(1, 1), cpvzero);
    sprite->body = cpBodyNew(mass, moment);
    sprite->shape = cpSegmentShapeNew(sprite->body, cpv(x1, y1), cpv(x2, y2), 1);

    // Setup user pointers so we can find the sprite in chipmunk callbacks.
    cpBodySetUserData(sprite->body, sprite);
    cpShapeSetUserData(sprite->shape, sprite);

    // Set a default friction
    cpShapeSetFriction(sprite->shape, 1);

    // Insert it into space.
    cpSpaceAddBody(kSpace, sprite->body);
    cpSpaceAddShape(kSpace, sprite->shape);

    // Determine the object dimensions.
    bb = cpShapeCacheBB(sprite->shape);

    // Create an ncurses WINDOW to contain it.
    sprite->win = newpad(bb.t - bb.b + 1,
                         ttypx(bb.r - bb.l) + 2);

    // Create a caca canvas to draw it.
    sprite->cv = caca_create_canvas(getmaxx(sprite->win), getmaxy(sprite->win));

    // Draw an initial sprite.
    caca_draw_thin_line(sprite->cv,
                        x1 - ttypx(bb.l),
                        y2 - bb.b,
                        x2 - ttypx(bb.l),
                        y1 - bb.b);

    // Draw that contents to the window.
    sprite_redraw(sprite);

    return sprite;
}


void sprite_anchor(sprite_t *sprite)
{
    if (cpSpaceContainsBody(kSpace, sprite->body))
        cpSpaceRemoveBody(kSpace, sprite->body);
    if (cpSpaceContainsShape(kSpace, sprite->shape))
        cpSpaceRemoveShape(kSpace, sprite->shape);

    cpShapeSetBody(sprite->shape, cpSpaceGetStaticBody(kSpace));

    cpSpaceAddShape(kSpace, sprite->shape);

    cpBodyFree(sprite->body);

    sprite->body = NULL;
}

void sprite_background(sprite_t *sprite)
{
    cpSpace *kSpace = cpBodyGetSpace(sprite->body);
    g_return_if_fail(kSpace);
    g_return_if_fail(cpSpaceContainsBody(kSpace, sprite->body));
    cpSpaceRemoveBody(kSpace, sprite->body);

    // Don't interact with other objects.
    sprite->flags |= SPRITE_FLAG_NOCLIP;

    // Always draw first.
    sprite_set_layer(sprite, UINT16_MAX);
}

void sprite_destroy(sprite_t *sprite)
{
    g_return_if_fail(sprite);

    // Remove sprite from object list.
    objects = g_slist_remove(objects, sprite);

    if (sprite->shape) {
        cpSpace *space = cpShapeGetSpace(sprite->shape);
        if (space) {
            cpSpaceRemoveShape(space, sprite->shape);
        }
        cpShapeFree(sprite->shape);
    }
    if (sprite->body) {
        cpSpace *space = cpBodyGetSpace(sprite->body);
        if (space) {
            cpSpaceRemoveBody(space, sprite->body);
        }
        cpBodyFree(sprite->body);
    }
    delwin(sprite->win);
    caca_free_canvas(sprite->cv);
    g_slist_free(sprite->callbacks.collision.begin);
    g_slist_free(sprite->callbacks.collision.presolve);
    g_slist_free(sprite->callbacks.collision.postsolve);
    g_slist_free(sprite->callbacks.collision.separate);
    g_slist_free(sprite->callbacks.update);
    g_slist_free(sprite->callbacks.destroy);
    g_free(sprite);
    return;
}

int sprite_rotation(sprite_t *sprite)
{
    float rads = fmod(cpBodyGetAngle(sprite->body), 2 * M_PI);
    return nearbyint(fabs(rads * (180 / M_PI)));
}

// The contents of the canvas is only copied over on redraw.
// The reason for this is that you can use multiple canvas operations
// and they're not displayed until you're ready.
//
// Note that this does not take into account any changes to the
// physics properties of the object, such as it's location or rotation.
void sprite_redraw(sprite_t *sprite)
{
    // No point wasting our time if we can't see it.
    if (sprite_visible(sprite) == false)
        return;

    // Clear current window contents.
    wclear(sprite->win);

    // Translate caca format to ncurses.
    canvas_display_window(sprite->cv, sprite->win);
}

void sprite_redraw_rotated(sprite_t *sprite)
{
    caca_canvas_t *rotated;
    int width = caca_get_canvas_width(sprite->cv);
    int height = caca_get_canvas_height(sprite->cv);
    int angle = sprite_rotation(sprite);

    // Check if we need to do anything...
    if (angle >= 315 || angle < 45) {
        // The sprite is not rotated, so just redraw normally.
        sprite_redraw(sprite);
        return;
    }

    // The sprite is rotated, so we need a temporary canvas to draw on.
    rotated = caca_create_canvas(width, height);

    // Copy the current image over.
    caca_blit(rotated, 0, 0, sprite->cv, NULL);

    // Choose a rotated variant.
    // XXX: If the pixel ratio is *not* 2.0f, then this won't work,
    // you need to resize the new canvas and use caca_stretch_ not
    // cac_rotate_.
    switch (sprite_rotation(sprite)) {
        case 45 ... 134:
            caca_rotate_right(rotated);
            break;
        case 135 ... 224:
            caca_rotate_right(rotated);
            caca_rotate_right(rotated);
            break;
        case 225 ... 314:
            caca_rotate_left(rotated);
            break;
    }

    // Clear current window contents.
    wclear(sprite->win);

    // Translate caca format to ncurses.
    canvas_display_window(rotated, sprite->win);

    caca_free_canvas(rotated);
}

bool sprite_visible(sprite_t *sprite)
{
    cpBB bb;

    // If there was no sprite, it cannot be visible.
    g_return_val_if_fail(sprite, false);

    // Learn sprite position.
    bb = cpShapeCacheBB(sprite->shape);

    // If the bottom edge is above the top of the screen.
    if (bb.b > camera.y + getmaxy(stdscr))
        return false;
    // If the top edge is below the bottom of the screen.
    if (bb.t < camera.y)
        return false;
    if (bb.r < cmpx(camera.x))
        return false;
    if (bb.l > cmpx(camera.x + getmaxx(stdscr)))
        return false;

    return true;
}

void sprite_center_camera(sprite_t *sprite)
{
    cpBB box;
    cpVect cent;

    // Find the location of this sprite.
    box = cpShapeCacheBB(sprite->shape);

    // Now we want the center of that box.
    cent = cpBBCenter(box);

    camera.x = cent.x - cmpx(getmaxx(stdscr)) / 2;
    camera.y = cent.y - getmaxy(stdscr) / 2;

    // That is in chipmunk pixels, convert to tty pixels
    camera.x = ttypx(camera.x);
    camera.x = CLAMP(camera.x, 0, INFINITY);
    camera.y = CLAMP(camera.y, 0, INFINITY);

    return;
}

// The sprite position and dimensions are updated from the physics simulation.
void sprite_refresh(sprite_t *sprite)
{
    int ret;
    int sminrow, smincol;
    int dminrow, dmincol;
    int dmaxrow, dmaxcol;
    cpBB bb;

    g_return_if_fail(sprite);

    // We don't need to waste our time if this isn't visible.
    if (sprite_visible(sprite) == false)
        return;

    // Determine the object dimensions.
    bb = cpShapeCacheBB(sprite->shape);

    // Now we need to adjust it's position relative to the camera
    // viewport.
    bb.t -= camera.y;
    bb.b -= camera.y;
    bb.l -= cmpx(camera.x);
    bb.r -= cmpx(camera.x);

    // Okay, but maybe it's only partially visible, so adjust our start
    // position. The src should be the first visible line.
    sminrow = 0;
    smincol = 0;
    dminrow = getmaxy(stdscr) - bb.t;
    dmincol = ttypx(bb.l);
    dmaxrow = MIN(getmaxy(stdscr) - bb.b, getmaxy(stdscr) - 1);
    dmaxcol = CLAMP(ttypx(bb.r), ttypx(bb.l), getmaxx(stdscr) - 1);

    if (dmincol < 0) {
        smincol -= dmincol;
        dmincol = 0;
    }

    dmaxcol = CLAMP(dmaxcol, dmincol, dmincol + (getmaxx(sprite->win) - smincol - 1));

    if (dminrow < 0) {
        sminrow -= dminrow;
        dminrow = 0;
    }

    dmaxrow = CLAMP(dmaxrow, dminrow, dminrow + (getmaxy(sprite->win) - sminrow - 1));

    // This copies the sprite into strscr.
    ret = copywin(sprite->win,
                  stdscr,
                  sminrow,
                  smincol,
                  dminrow,
                  dmincol,
                  dmaxrow,
                  dmaxcol,
                  true);

    if (copywin != OK) {
        g_debug("copywin for %s failed, %d %d %d %d", sprite->shortname,
                                                      dminrow,
                                                      dmincol,
                                                      dmaxrow,
                                                      dmaxcol);
    }
    return;
}

void sprite_set_bg(sprite_t *sprite, const char *filename)
{
    caca_clear_canvas(sprite->cv);
    caca_import_canvas_from_file(sprite->cv, filename, "utf8");
    sprite_redraw(sprite);
    return;
}

static int sprite_collision_begin(cpArbiter *arb,
                                  struct cpSpace *space,
                                  cpDataPointer data)
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

    //g_debug("collission %s => %s", spritea->shortname, spriteb->shortname);

    // If someone has registered callbacks, call them.
    for (GSList *curr = spritea->callbacks.collision.begin; curr; curr = curr->next) {
        spritecb_t cb = curr->data;
        if (cb && cb(spritea, spriteb, arb) == false)
            return false;
    }
    for (GSList *curr = spriteb->callbacks.collision.begin; curr; curr = curr->next) {
        spritecb_t cb = curr->data;
        if (cb && cb(spriteb, spritea, arb) == false)
            return false;
    }

    return true;
}

void sprite_init(cpSpace *space, WINDOW *window)
{
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    // Setup the default space.
    kSpace = space;

    // Install a default collision handler.
    cpSpaceSetDefaultCollisionHandler(kSpace,
                                      sprite_collision_begin,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
}

void sprite_fini()
{
    g_slist_free_full(objects, (GDestroyNotify) sprite_destroy);
    endwin();
}
