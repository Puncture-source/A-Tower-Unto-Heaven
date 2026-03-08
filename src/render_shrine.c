#include "game.h"

/*
 * render_shrine.c
 *
 * Draws the "Last Hearth" shrine scene — the safe room between floors.
 * Aesthetic: Dark Souls / Firelink Shrine. Worn, ancient, melancholic.
 * A single fire burns in the dark. The Tower looms above everything.
 *
 * Scene layout (70 cols x 20 rows):
 *   - Background: The Tower, monolithic, faint in the dark (centre)
 *   - Left:       Broken stone arch / ruined pillar
 *   - Centre-L:   Cloaked seated NPC (hunched, still)
 *   - Centre:     Campfire (animated across 3 frames)
 *   - Right:      Sword planted in earth, scattered bones
 *   - Floor:      Stone step line, debris
 *
 * Colour mapping (existing pairs from game.h):
 *   CP_DEF    (white)   — stone, ground, arch
 *   CP_DANGER (red)     — fire core, embers
 *   CP_ITEM   (yellow)  — flame body, warm light
 *   CP_WALL   (blue)    — deep shadow, tower silhouette
 *   CP_UI     (cyan)    — NPC figure
 *   CP_FLOOR  (white)   — ground scatter, bones
 *   CP_MAGIC  (magenta) — sword (faint enchantment)
 */

/* ------------------------------------------------------------------ */
/*  Scene strings — exactly 70 characters wide, padded with spaces    */
/*                                                                     */
/*  Verified column counts. Each line is a string of exactly 70 chars */
/*  (no trailing newline). Spaces fill unused columns.                */
/* ------------------------------------------------------------------ */

/* The full scene: 20 rows x 70 cols (reference only — drawn procedurally below) */
static const char *SHRINE_ROWS_REF[20] = {
/*         0         1         2         3         4         5         6    */
/*         0123456789012345678901234567890123456789012345678901234567890123456789 */
/* row  0 */ "                              |   |                              ",
/* row  1 */ "                             /|   |\\                             ",
/* row  2 */ "                            / |   | \\                            ",
/* row  3 */ "                           /  |   |  \\                           ",
/* row  4 */ "                     ______|  |   |  |______                     ",
/* row  5 */ "                    |         |   |         |                    ",
/* row  6 */ "                    |_________|   |_________|                    ",
/* row  7 */ "                              |   |                              ",
/* row  8 */ "  _____                       |   |                   |          ",
/* row  9 */ " |#####|  ,--.__              |   |                  _|_         ",
/* row 10 */ " |#####| /       `--.___      |   |                 /   \\        ",
/* row 11 */ " |#####|/    .  .     . `--._.|___|            .   | ___ |   .  ",
/* row 12 */ " |#####|   . (_) .  (`)  (   `)  (`  .   .        |_|_|_|       ",
/* row 13 */ " |####/|  . /|\\  .  ) (  -[*]-  ) (  .  . ,--,   __|_|__  .    ",
/* row 14 */ "  \\###/ \\ . ||| .  ( )  . [|] . ( )  .  ./    | _/ _|_ \\_ .    ",
/* row 15 */ "   `--'  `._|||_'. )(  . /|||\\  . )(  . /|    |/  / | \\  \\      ",
/* row 16 */ "      .   _/|||\\_ ....../|||||\\....... /  |____|  /__|__\\  \\     ",
/* row 17 */ "======================================================================",
/* row 18 */ "  ___________________________________________________________________  ",
/* row 19 */ "                                                                      ",
};

/*
 * That initial sketch above has width issues on some rows — the
 * composition below is the corrected, production version rendered
 * with explicit per-character placement via ncurses mvaddch/mvprintw.
 * We draw each element in its own pass for colour control.
 */

/* ------------------------------------------------------------------ */
/*  Campfire animation — 3 frames, each 7 wide x 5 tall               */
/*  Frame index cycles: 0 -> 1 -> 2 -> 0  (call with turn % 3)        */
/*                                                                     */
/*  Rendered relative to a given (oy, ox) origin.                     */
/*  Colour: row 0-1 = CP_ITEM (yellow flame), row 2 = CP_DANGER (red) */
/*          row 3-4 = CP_FLOOR (white embers/ash)                     */
/* ------------------------------------------------------------------ */

/* 3 frames x 5 rows x 7 cols (null-terminated strings, spaces = transparent) */
static const char *FIRE_FRAMES[3][5] = {
    { /* frame 0 — tall lean left */
        "  ) (  ",
        " ( )(  ",
        "  \\*/  ",
        " -[*]- ",
        "  )|(  ",
    },
    { /* frame 1 — full billow */
        " )   ( ",
        "( ) ( )",
        " \\(*)/  ",
        " -[*]- ",
        " .)|(.  ",
    },
    { /* frame 2 — lean right, low */
        "   ) ( ",
        " (  )( ",
        "  /*(\\  ",
        " -[*]- ",
        "  )|(.  ",
    },
};

/* ------------------------------------------------------------------ */
/*  Element drawing helpers                                            */
/* ------------------------------------------------------------------ */

static void shrine_str(int y, int x, int cp, int attr, const char *s) {
    if (y < 0 || x < 0) return;
    attron(COLOR_PAIR(cp) | attr);
    mvprintw(y, x, "%s", s);
    attroff(COLOR_PAIR(cp) | attr);
}

/* Draw a single scene element string, skipping space characters
   so underlying content shows through (layered composition). */
static void shrine_overlay(int y, int x, int cp, int attr, const char *s) {
    attron(COLOR_PAIR(cp) | attr);
    for (int i = 0; s[i]; i++) {
        if (s[i] != ' ')
            mvaddch(y, x + i, (unsigned char)s[i]);
    }
    attroff(COLOR_PAIR(cp) | attr);
}

/* ------------------------------------------------------------------ */
/*  render_shrine()                                                    */
/*                                                                     */
/*  Call once per display refresh. Pass (turn % 3) as fire_frame.     */
/*  The scene is drawn centred in the terminal.                       */
/*                                                                     */
/*  Expected terminal size: >= 72 wide, >= 22 tall.                   */
/* ------------------------------------------------------------------ */
/* render_shrine: pure draw — caller must clear() before and refresh() after */
void render_shrine(int fire_frame) {
    int th, tw;
    getmaxyx(stdscr, th, tw);

    /* Centre the 70-wide, 20-tall scene */
    int ox = (tw - 70) / 2;   /* horizontal offset */
    int oy = (th - 22) / 2;   /* vertical offset (leave room at bottom) */
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    /* -------------------------------------------------------------- */
    /* LAYER 0: Deep background fill — blue shadow everywhere          */
    /* -------------------------------------------------------------- */
    attron(COLOR_PAIR(CP_WALL));
    for (int r = 0; r < 20; r++) {
        mvhline(oy + r, ox, ' ', 70);
    }
    attroff(COLOR_PAIR(CP_WALL));

    /* -------------------------------------------------------------- */
    /* LAYER 1: THE TOWER — background centre, rows 0-7               */
    /* Faint blue/dim to feel distant and oppressive.                  */
    /* -------------------------------------------------------------- */
    /*           col offset from ox:  28       */
    shrine_str(oy+0, ox+31, CP_WALL, A_DIM,  "|   |"         );
    shrine_str(oy+1, ox+30, CP_WALL, A_DIM,  "/|   |\\"       );
    shrine_str(oy+2, ox+29, CP_WALL, A_DIM,  "/ |   | \\"     );
    shrine_str(oy+3, ox+28, CP_WALL, A_DIM,  "/  |   |  \\"   );
    shrine_str(oy+4, ox+22, CP_WALL, A_DIM,  "_____|  |   |  |_____"  );
    shrine_str(oy+5, ox+21, CP_WALL, A_DIM,  "|        |   |        |" );
    shrine_str(oy+6, ox+21, CP_WALL, A_DIM,  "|________|   |________|" );
    shrine_str(oy+7, ox+31, CP_WALL, A_DIM,  "|   |"         );

    /* -------------------------------------------------------------- */
    /* LAYER 2: RUINED ARCH / BROKEN PILLAR — left side, rows 8-16   */
    /* Stone colour: white (CP_DEF). Cracks in dim.                   */
    /* -------------------------------------------------------------- */

    /* Pillar body — left column, rows 8-16 */
    shrine_str(oy+ 8, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+ 9, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+10, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+11, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+12, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+13, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+14, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+15, ox+2, CP_DEF, 0,      "|###|");
    shrine_str(oy+16, ox+1, CP_DEF, A_DIM,  ".|###|.");

    /* Arch crown — broken span angling up-right, rows 8-10 */
    shrine_str(oy+ 8, ox+7, CP_DEF, A_DIM, ",--.__");
    shrine_str(oy+ 9, ox+6, CP_DEF, A_DIM, "/      `--.");
    shrine_str(oy+10, ox+5, CP_DEF, A_DIM, "|    .      `-._");
    shrine_str(oy+11, ox+5, CP_DEF, A_DIM, "|  .   .        `--.");

    /* Rubble at arch base right side — rows 11-13 */
    shrine_str(oy+12, ox+15, CP_DEF, A_DIM, "___.");
    shrine_str(oy+13, ox+14, CP_DEF, A_DIM, "/    \\");
    shrine_str(oy+14, ox+14, CP_DEF, A_DIM, "|    |");
    shrine_str(oy+15, ox+14, CP_DEF, A_DIM, "|____|");

    /* -------------------------------------------------------------- */
    /* LAYER 3: STONE STEPS — rows 17-18, full width                  */
    /* -------------------------------------------------------------- */
    attron(COLOR_PAIR(CP_DEF) | A_BOLD);
    mvhline(oy+17, ox, '=', 70);
    attroff(COLOR_PAIR(CP_DEF) | A_BOLD);

    attron(COLOR_PAIR(CP_DEF));
    mvhline(oy+18, ox, '_', 70);
    attroff(COLOR_PAIR(CP_DEF));

    /* Step detail — stone seams */
    shrine_str(oy+17, ox+10, CP_DEF, 0, "+");
    shrine_str(oy+17, ox+23, CP_DEF, 0, "+");
    shrine_str(oy+17, ox+35, CP_DEF, 0, "+");
    shrine_str(oy+17, ox+47, CP_DEF, 0, "+");
    shrine_str(oy+17, ox+60, CP_DEF, 0, "+");

    /* -------------------------------------------------------------- */
    /* LAYER 4: GROUND LINE / FLOOR SCATTER — row 16                  */
    /* -------------------------------------------------------------- */
    shrine_str(oy+16, ox+ 0, CP_DEF, A_DIM, "........");
    shrine_str(oy+16, ox+20, CP_DEF, A_DIM, "......");
    shrine_str(oy+16, ox+42, CP_DEF, A_DIM, ".....");
    shrine_str(oy+16, ox+60, CP_DEF, A_DIM, "........");

    /* Scattered bones */
    shrine_str(oy+15, ox+44, CP_FLOOR, A_DIM, ".-.");
    shrine_str(oy+16, ox+44, CP_FLOOR, A_DIM, "(_)");
    shrine_str(oy+15, ox+49, CP_FLOOR, A_DIM, "o");
    shrine_str(oy+16, ox+50, CP_FLOOR, A_DIM, "o");
    shrine_str(oy+14, ox+42, CP_FLOOR, A_DIM, ". * .");

    /* -------------------------------------------------------------- */
    /* LAYER 5: SWORD IN EARTH — right area, rows 9-16                */
    /* -------------------------------------------------------------- */
    shrine_str(oy+ 9, ox+58, CP_MAGIC, A_DIM,  "|"    );
    shrine_str(oy+10, ox+57, CP_MAGIC, A_DIM,  "_|_"  );
    shrine_str(oy+11, ox+57, CP_MAGIC, 0,      "/_\\" );
    shrine_str(oy+12, ox+58, CP_MAGIC, 0,      "|"    );
    shrine_str(oy+13, ox+58, CP_MAGIC, 0,      "|"    );
    shrine_str(oy+14, ox+58, CP_MAGIC, 0,      "|"    );
    shrine_str(oy+15, ox+58, CP_MAGIC, 0,      "|"    );
    shrine_str(oy+16, ox+55, CP_FLOOR, A_DIM,  ". | .");

    /* -------------------------------------------------------------- */
    /* LAYER 6: NPC — cloaked seated figure, rows 9-16                */
    /* -------------------------------------------------------------- */
    /* Head — small, bowed */
    shrine_str(oy+ 9, ox+27, CP_UI, A_DIM,  "."     );
    /* Shoulders/cloak top */
    shrine_str(oy+10, ox+25, CP_UI, 0,      "(_)"   );
    /* Body / cloak hunched */
    shrine_str(oy+11, ox+24, CP_UI, 0,      "/||\\"  );
    shrine_str(oy+12, ox+23, CP_UI, 0,      "/ || \\" );
    shrine_str(oy+13, ox+23, CP_UI, 0,      "|_||_|" );
    /* Seated base — legs tucked */
    shrine_str(oy+14, ox+22, CP_UI, A_DIM,  "\\_/|\\_/" );
    shrine_str(oy+15, ox+23, CP_UI, A_DIM,  "  |||  "  );
    shrine_str(oy+16, ox+22, CP_UI, A_DIM,  "._|||_."  );

    /* -------------------------------------------------------------- */
    /* LAYER 7: CAMPFIRE — animated, rows 9-16, centred ~col 34       */
    /* -------------------------------------------------------------- */
    int fc = fire_frame % 3;

    /* Flame top 2 rows — yellow */
    shrine_overlay(oy+ 9, ox+33, CP_ITEM,   A_BOLD, FIRE_FRAMES[fc][0]);
    shrine_overlay(oy+10, ox+33, CP_ITEM,   A_BOLD, FIRE_FRAMES[fc][1]);
    /* Flame mid — transition yellow->red */
    shrine_overlay(oy+11, ox+33, CP_ITEM,   0,      FIRE_FRAMES[fc][2]);
    /* Fire core — red */
    shrine_overlay(oy+12, ox+33, CP_DANGER, A_BOLD, FIRE_FRAMES[fc][3]);
    /* Ember base */
    shrine_overlay(oy+13, ox+33, CP_DANGER, A_DIM,  FIRE_FRAMES[fc][4]);

    /* Log bed */
    shrine_str(oy+14, ox+33, CP_DEF, A_DIM, "/[___]\\" );
    /* Ash / warm glow on ground */
    shrine_str(oy+15, ox+32, CP_FLOOR, A_DIM, "~~~~~~~~~" );
    shrine_str(oy+16, ox+31, CP_ITEM,  A_DIM, "`. . . .'" );

    /* LAYER 8: scene label at bottom of shrine art */
    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(oy+19, ox + (70 - 22) / 2, "~ The Last Hearth ~");
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);
}

/* ------------------------------------------------------------------ */
/*  render_shrine_loop()                                               */
/*                                                                     */
/*  Blocking call: shows shrine scene, animates fire, waits for       */
/*  any keypress to return. Suitable for inter-floor rest screen.     */
/* ------------------------------------------------------------------ */
void render_shrine_loop(void) {
    int turn = 0;
    nodelay(stdscr, TRUE);   /* non-blocking getch */
    halfdelay(4);             /* 0.4s per tick — fire flicker rate  */

    for (;;) {
        clear();
        render_shrine(turn);
        refresh();
        int ch = getch();
        if (ch != ERR) break; /* any key exits */
        turn++;
    }

    nodelay(stdscr, FALSE);
    cbreak();                 /* restore normal input mode */
}
