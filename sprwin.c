#include <glib.h>
#include <caca.h>
#include <ncurses.h>
#include <chipmunk.h>

#include "sprite.h"

// Color pairs
static int attrs[16*16];

static bool ncurses_init_graphics(void)
{
    static bool initialized;
    const int curses_colors[] = {
        /* Standard curses colours */
        COLOR_BLACK,
        COLOR_BLUE,
        COLOR_GREEN,
        COLOR_CYAN,
        COLOR_RED,
        COLOR_MAGENTA,
        COLOR_YELLOW,
        COLOR_WHITE,
        /* Extra values for xterm-16color */
        COLOR_BLACK + 8,
        COLOR_BLUE + 8,
        COLOR_GREEN + 8,
        COLOR_CYAN + 8,
        COLOR_RED + 8,
        COLOR_MAGENTA + 8,
        COLOR_YELLOW + 8,
        COLOR_WHITE + 8
    };

    int fg, bg, max;

    if (initialized)
        return initialized;

    max = COLORS >= 16 ? 16 : 8;

    for (bg = 0; bg < max; bg++) {
        for (fg = 0; fg < max; fg++) {
            int col = ((max + 7 - fg) % max) + max * bg;
            init_pair(col, curses_colors[fg], curses_colors[bg]);
            attrs[fg + 16 * bg] = COLOR_PAIR(col);

            if (max == 8) {
                attrs[fg + 8 + 16 * bg] = A_BOLD | COLOR_PAIR(col);
                attrs[fg + 16 * (bg + 8)] = A_BLINK | COLOR_PAIR(col);
                attrs[fg + 8 + 16 * (bg + 8)] = A_BLINK | A_BOLD | COLOR_PAIR(col);
            }
        }
    }

    return initialized++;
}

static void ncurses_write_utf32(WINDOW *win, uint32_t ch)
{
    char buf[10];
    int bytes;

    if(ch == CACA_MAGIC_FULLWIDTH)
        return;

    bytes = caca_utf32_to_utf8(buf, ch);
    buf[bytes] = '\0';
    waddstr(win, buf);
}

void canvas_display_window(caca_canvas_t *cv, WINDOW *win)
{
    int x, y, i;
    uint32_t const *cvchars, *cvattrs;
    int dx = 0, dy = 0, dw, dh;

    ncurses_init_graphics();

    dw = caca_get_canvas_width(cv);
    dh = caca_get_canvas_height(cv);

    cvchars = caca_get_canvas_chars(cv) + dx + dy * dw;
    cvattrs = caca_get_canvas_attrs(cv) + dx + dy * dw;

    for(y = dy; y < dy + dh; y++) {
        wmove(win, y, dx);

        for(x = dx; x < dx + dw; x++) {
            uint32_t attr = *cvattrs++;

            wattrset(win, attrs[caca_attr_to_ansi(attr)]);
            if(attr & CACA_BOLD)
                wattron(win, A_BOLD);
            if(attr & CACA_BLINK)
                wattron(win, A_BLINK);
            if(attr & CACA_UNDERLINE)
                wattron(win, A_UNDERLINE);

            ncurses_write_utf32(win, *cvchars++);
        }
    }
}
