#include <stdlib.h>
#include <ncurses.h>
#include <caca.h>
#include <chipmunk/chipmunk.h>

int main(int argc, char **argv)
{
    WINDOW *w;
    cpVect gravity = cpv(0, -100);
    cpSpace *space = cpSpaceNew();
    int ch;

    initscr();
    start_color();
    cbreak();

    cpSpaceSetGravity(space, gravity);
    cpShape *ground = cpSegmentShapeNew(cpSpaceGetStaticBody(space),
                                        cpv(-20, 5),
                                        cpv(20, -5),
                                        0);

    cpShapeSetFriction(ground, 1);
    cpSpaceAddShape(space, ground);

    cpFloat radius = 5;
    cpFloat mass = 1;

    // The moment of inertia is like mass for rotation
    // Use the cpMomentFor*() functions to help you approximate it.
    cpFloat moment = cpMomentForCircle(mass, 0, radius, cpvzero);

    // The cpSpaceAdd*() functions return the thing that you are adding.
    // It's convenient to create and add an object in one line.
    cpBody *ballBody = cpSpaceAddBody(space, cpBodyNew(mass, moment));
    cpBodySetPos(ballBody, cpv(0, 15));
    cpShape *ballShape = cpSpaceAddShape(space, cpCircleShapeNew(ballBody, radius, cpvzero));
    cpShapeSetFriction(ballShape, 0.7);

    // Now that it's all set up, we simulate all the objects in the space by
    // stepping forward through time in small increments called steps.
    // It is *highly* recommended to use a fixed size time step.
    cpFloat timeStep = 1.0/60.0;

    keypad(stdscr, TRUE);
    noecho();

    init_pair(1, COLOR_CYAN, COLOR_RED);

    attron(COLOR_PAIR(1));

    printw("Hello");

    cpVect pos = cpBodyGetPos(ballBody);

    w = newwin(5, 5, 20 - pos.y, pos.x);

    //box(w, 0, 0);

    waddstr(w, "O");
    refresh();
    wrefresh(w);

    while ((ch = getch()) != KEY_F(1)) {
        cpVect pos = cpBodyGetPos(ballBody);
        cpVect vel = cpBodyGetVel(ballBody);
        werase(w);
        wrefresh(w);
        refresh();
        mvwin(w, 20 - pos.y, pos.x);
        waddstr(w, "O");
        wrefresh(w);
        refresh();
        cpSpaceStep(space, timeStep);
    }

    delwin(w);
    endwin();

    // Clean up our objects and exit!
    cpShapeFree(ballShape);
    cpBodyFree(ballBody);
    cpShapeFree(ground);
    cpSpaceFree(space);
    return 0;
}
