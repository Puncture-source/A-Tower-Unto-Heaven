#include "game.h"

/* ─── Init / cleanup ────────────────────────────────────────────────── */
void render_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(CP_DEF,    COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_DANGER, COLOR_RED,     COLOR_BLACK);
    init_pair(CP_PLAYER, COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_ITEM,   COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_WALL,   COLOR_BLUE,    COLOR_BLACK);
    init_pair(CP_UI,     COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_MAGIC,  COLOR_MAGENTA, COLOR_BLACK);
    init_pair(CP_BOSS,   COLOR_RED,     COLOR_BLACK);
    init_pair(CP_FLOOR,  COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_BLOOD,  COLOR_RED,     COLOR_BLACK);
    init_pair(CP_GLIMMER,COLOR_YELLOW,  COLOR_BLACK);

    getmaxyx(stdscr, G.th, G.tw);
    G.gw = G.tw - UI_W;
    G.gh = G.th - MSG_H;
}

void render_cleanup(void) {
    endwin();
}

/* ─── Helper: draw a string clipped to width ────────────────────────── */
static void mvprintw_clip(int y, int x, int maxw, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[maxw] = '\0';
    mvprintw(y, x, "%s", buf);
}

/* ─── HP bar ────────────────────────────────────────────────────────── */
static void draw_hp_bar(int y, int x, int w, int hp, int max) {
    int filled = (max > 0) ? (hp * w / max) : 0;
    if (filled > w) filled = w;
    attron(COLOR_PAIR(CP_DANGER));
    for (int i = 0; i < filled; i++) mvaddch(y, x+i, '|');
    attroff(COLOR_PAIR(CP_DANGER));
    attron(COLOR_PAIR(CP_WALL));
    for (int i = filled; i < w; i++) mvaddch(y, x+i, '-');
    attroff(COLOR_PAIR(CP_WALL));
}

/* ─── UI Panel ──────────────────────────────────────────────────────── */
static void draw_ui(void) {
    Player *p = &G.player;
    int px = G.gw;   /* panel x start */
    int row = 0;

    /* Separator */
    attron(COLOR_PAIR(CP_UI));
    for (int y = 0; y < G.th; y++) mvaddch(y, px, '|');
    attroff(COLOR_PAIR(CP_UI));
    px++;

    /* Title */
    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw_clip(row++, px, UI_W-2, "A TOWER UNTO HEAVEN");
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);
    row++;

    /* Character name */
    attron(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);
    mvprintw_clip(row++, px, UI_W-2, "%s", CHARS[p->chr].name);
    attroff(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);

    /* HP */
    attron(COLOR_PAIR(CP_DANGER));
    mvprintw_clip(row++, px, UI_W-2, "Flesh %3d/%-3d", p->hp, p->max_hp);
    attroff(COLOR_PAIR(CP_DANGER));
    draw_hp_bar(row++, px, UI_W-2, p->hp, p->max_hp);

    /* Stats */
    attron(COLOR_PAIR(CP_DEF));
    mvprintw_clip(row++, px, UI_W-2, "Tem %-2d  Res %-2d", p->atk, p->def);
    if (p->spd != 0) {
        int cp = p->spd > 0 ? CP_PLAYER : CP_DANGER;
        attroff(COLOR_PAIR(CP_DEF));
        attron(COLOR_PAIR(cp));
        mvprintw_clip(row++, px, UI_W-2, "Gait %+d (%s)",
                      p->spd, p->spd > 0 ? "SWIFT" : "SLUGGISH");
        attroff(COLOR_PAIR(cp));
        attron(COLOR_PAIR(CP_DEF));
    }
    if (p->has_ranged)
        mvprintw_clip(row++, px, UI_W-2, "Edge %-2d  Arc %-2d",
                      p->r_dmg, p->r_rng);
    if (p->r_ammo > 0)
        mvprintw_clip(row++, px, UI_W-2, "Rounds %-3d", p->r_ammo);
    else if (p->has_ranged && p->r_ammo < 0)
        mvprintw_clip(row++, px, UI_W-2, "Rounds  inf");
    if (p->grenades > 0)
        mvprintw_clip(row++, px, UI_W-2, "GRENADES %-2d", p->grenades);
    attroff(COLOR_PAIR(CP_DEF));
    attron(COLOR_PAIR(CP_UI));
    mvprintw_clip(row++, px, UI_W-2, "Momentum %-2d", p->momentum);
    attroff(COLOR_PAIR(CP_UI));

    /* Combat resources */
    attron(COLOR_PAIR(CP_GLIMMER) | A_BOLD);
    mvprintw_clip(row++, px, UI_W-2, "Glimmer  %-3d", p->glimmer);
    attroff(COLOR_PAIR(CP_GLIMMER) | A_BOLD);

    attron(COLOR_PAIR(CP_MAGIC));
    mvprintw_clip(row++, px, UI_W-2, "Memories %-2d", p->n_memories);
    attroff(COLOR_PAIR(CP_MAGIC));

    {
        int ash_cp = p->ash >= 100 ? CP_BOSS :
                     p->ash >= 67  ? CP_DANGER :
                     p->ash >= 34  ? CP_ITEM : CP_PLAYER;
        int ash_at = p->ash >= 100 ? A_BOLD : 0;
        attron(COLOR_PAIR(ash_cp) | ash_at);
        if (p->husk)
            mvprintw_clip(row++, px, UI_W-2, "Ash      HUSK");
        else
            mvprintw_clip(row++, px, UI_W-2, "Ash      %-3d", p->ash);
        attroff(COLOR_PAIR(ash_cp) | ash_at);
    }
    row++;

    /* Floor / kills */
    attron(COLOR_PAIR(CP_UI));
    mvprintw_clip(row++, px, UI_W-2, "Ascent %d/%-d  Sin %-3d",
                  p->floor_num+1, NUM_FLOORS, p->kills);
    attroff(COLOR_PAIR(CP_UI));
    row++;

    /* Items */
    attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
    mvprintw_clip(row++, px, UI_W-2, "ITEMS %d/%d:", p->n_items, p->max_items);
    attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);
    attron(COLOR_PAIR(CP_ITEM));
    for (int i = 0; i < p->n_items && row < G.gh - 6; i++) {
        if (p->items[i] == IT_NONE) continue;
        char line[UI_W];
        snprintf(line, sizeof(line), " %s", ITEMS[p->items[i]].name);
        line[UI_W-2] = '\0';
        mvprintw(row++, px, "%s", line);
    }
    attroff(COLOR_PAIR(CP_ITEM));

    /* Room enemies */
    Room *rm = cur_room();
    int n_alive = 0;
    for (int i = 0; i < rm->n_enemies; i++)
        if (rm->enemies[i].alive) n_alive++;
    if (n_alive > 0 && row < G.gh - 4) {
        row++;
        attron(COLOR_PAIR(CP_DANGER) | A_BOLD);
        mvprintw_clip(row++, px, UI_W-2, "ENEMIES:");
        attroff(COLOR_PAIR(CP_DANGER) | A_BOLD);
        for (int i = 0; i < rm->n_enemies && row < G.gh - 2; i++) {
            Enemy *e = &rm->enemies[i];
            if (!e->alive) continue;
            attron(e->boss ? COLOR_PAIR(CP_BOSS) | A_BOLD : COLOR_PAIR(CP_DANGER));
            char line[UI_W+1];
            snprintf(line, sizeof(line), " %-*s%d/%d",
                     UI_W - 7, e->name, e->tension, e->max_tension);
            line[UI_W-1] = '\0';
            mvprintw(row++, px, "%s", line);
            attroff(COLOR_PAIR(CP_DANGER) | COLOR_PAIR(CP_BOSS) | A_BOLD);
        }
    }

    /* Room floor items */
    int n_floor = 0;
    for (int i = 0; i < rm->n_items; i++)
        if (rm->items[i] != IT_NONE) n_floor++;
    if (n_floor > 0 && row < G.gh - 4) {
        row++;
        attron(COLOR_PAIR(CP_MAGIC) | A_BOLD);
        mvprintw_clip(row++, px, UI_W-2, "ON FLOOR:");
        attroff(COLOR_PAIR(CP_MAGIC) | A_BOLD);
        attron(COLOR_PAIR(CP_MAGIC));
        for (int i = 0; i < rm->n_items && row < G.gh - 2; i++) {
            if (rm->items[i] == IT_NONE) continue;
            char line[UI_W+1];
            snprintf(line, sizeof(line), " %s", ITEMS[rm->items[i]].name);
            line[UI_W-1] = '\0';
            mvprintw(row++, px, "%s", line);
        }
        attroff(COLOR_PAIR(CP_MAGIC));
    }

    row++;

    /* Keybinds */
    if (row < G.gh - 5) {
        attron(COLOR_PAIR(CP_UI));
        mvprintw_clip(row++, px, UI_W-2, "[hjkl] move");
        mvprintw_clip(row++, px, UI_W-2, "[yubn] diagonal");
        mvprintw_clip(row++, px, UI_W-2, "[f+dir] shoot");
        mvprintw_clip(row++, px, UI_W-2, "[e] grenade");
        mvprintw_clip(row++, px, UI_W-2, "[g] pick up");
        mvprintw_clip(row++, px, UI_W-2, "[i] inventory");
        mvprintw_clip(row++, px, UI_W-2, "[m] map");
        mvprintw_clip(row++, px, UI_W-2, "[.] wait");
        mvprintw_clip(row++, px, UI_W-2, "[?] manual");
        mvprintw_clip(row++, px, UI_W-2, "[q] quit");
        attroff(COLOR_PAIR(CP_UI));
    }
}

/* ─── Message log ───────────────────────────────────────────────────── */
static void draw_messages(void) {
    MsgLog *ml = &G.log;
    int base_y = G.gh;

    attron(COLOR_PAIR(CP_UI));
    for (int y = 0; y < G.th; y++) mvaddch(y, 0, ' '); /* clear leftmost */
    mvhline(base_y, 0, '-', G.gw);
    attroff(COLOR_PAIR(CP_UI));

    int start = ml->count - MSG_H + 1;
    if (start < 0) start = 0;
    for (int i = start; i < ml->count && (i - start) < MSG_H - 1; i++) {
        int y = base_y + 1 + (i - start);
        if (y >= G.th) break;
        attron(COLOR_PAIR(CP_DEF));
        mvprintw_clip(y, 0, G.gw - 1, "%s", ml->lines[i]);
        attroff(COLOR_PAIR(CP_DEF));
    }
}

/* ─── Raycasted FOV ─────────────────────────────────────────────────── */
#define FOV_DIM (FOV_RADIUS * 2 + 1)
static bool fov_map[FOV_DIM][FOV_DIM];

/* Returns true if the world tile at (wx,wy) is opaque (blocks LOS). */
static bool world_blocks_los(int wx, int wy) {
    int tx, ty;
    int ri = room_at_world(wx, wy, &tx, &ty);
    if (ri < 0) return true;
    TileType t = G.cf->rooms[ri].tiles[ty][tx];
    return (t == T_WALL || t == T_DOOR_LOCK || t == T_VOID);
}

/* Bresenham ray from (pwx,pwy) to (twx,twy); checks intermediate tiles only.
   Returns true if no opaque tile blocks the ray before reaching the target. */
static bool los_clear(int pwx, int pwy, int twx, int twy) {
    int dx = twx - pwx, dy = twy - pwy;
    if (dx == 0 && dy == 0) return true;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1,  sy = dy < 0 ? -1 : 1;
    int x = pwx, y = pwy;
    int err = ax - ay;
    while (true) {
        int e2 = 2 * err;
        if (e2 >= -ay) { err -= ay; x += sx; }
        if (e2 <=  ax) { err += ax; y += sy; }
        if (x == twx && y == twy) return true;
        if (world_blocks_los(x, y)) return false;
    }
}

/* Compute visibility for every offset in [-FOV_RADIUS, FOV_RADIUS]^2. */
static void compute_fov(int pwx, int pwy) {
    int r2 = FOV_RADIUS * FOV_RADIUS;
    for (int ddy = -FOV_RADIUS; ddy <= FOV_RADIUS; ddy++) {
        for (int ddx = -FOV_RADIUS; ddx <= FOV_RADIUS; ddx++) {
            bool vis = (ddx*ddx + ddy*ddy <= r2) &&
                       los_clear(pwx, pwy, pwx + ddx, pwy + ddy);
            fov_map[ddy + FOV_RADIUS][ddx + FOV_RADIUS] = vis;
        }
    }
}

static bool in_fov(int ddx, int ddy) {
    if (ddx < -FOV_RADIUS || ddx > FOV_RADIUS) return false;
    if (ddy < -FOV_RADIUS || ddy > FOV_RADIUS) return false;
    return fov_map[ddy + FOV_RADIUS][ddx + FOV_RADIUS];
}

/* ─── Room rendering ────────────────────────────────────────────────── */
static void draw_room(void) {
    Player *p  = &G.player;
    Floor  *fl = G.cf;
    Room   *cr = cur_room();

    /* Player's world position */
    int pwx = cr->wx + p->x;
    int pwy = cr->wy + p->y;

    /* Viewport top-left in world coords (player is centred on screen) */
    int vx = pwx - G.gw / 2;
    int vy = pwy - G.gh / 2;

    compute_fov(pwx, pwy);

    /* ── Tiles (all rooms) ── */
    for (int ri = 0; ri < fl->n_rooms; ri++) {
        Room *r = &fl->rooms[ri];
        int rw_full = r->w + 2, rh_full = r->h + 2;

        for (int ty = 0; ty < rh_full && ty < RT_H; ty++) {
            for (int tx = 0; tx < rw_full && tx < RT_W; tx++) {
                int wx = r->wx + tx;
                int wy = r->wy + ty;
                int sx = wx - vx;
                int sy = wy - vy;
                if (sx < 0 || sy < 0 || sx >= G.gw || sy >= G.gh) continue;

                TileType t = r->tiles[ty][tx];
                if (t == T_VOID) continue;
                /* Corridors are open passages — suppress door glyphs */
                if (r->is_corridor && t == T_DOOR_OPEN) t = T_FLOOR;
                if (r->is_corridor && t == T_DOOR_LOCK)  t = T_WALL;

                int ddx = wx - pwx, ddy = wy - pwy;
                bool visible = in_fov(ddx, ddy);

                if (visible) {
                    r->seen[ty][tx] = true; /* permanently reveal this tile */
                    switch (t) {
                        case T_WALL:
                            attron(COLOR_PAIR(CP_WALL));
                            mvaddch(sy, sx, '#');
                            attroff(COLOR_PAIR(CP_WALL));
                            break;
                        case T_FLOOR:
                            attron(COLOR_PAIR(CP_FLOOR) | A_DIM);
                            mvaddch(sy, sx, '.');
                            attroff(COLOR_PAIR(CP_FLOOR) | A_DIM);
                            break;
                        case T_DOOR_OPEN:
                            attron(COLOR_PAIR(CP_ITEM));
                            mvaddch(sy, sx, '+');
                            attroff(COLOR_PAIR(CP_ITEM));
                            break;
                        case T_DOOR_LOCK:
                            attron(COLOR_PAIR(CP_DANGER));
                            mvaddch(sy, sx, '=');
                            attroff(COLOR_PAIR(CP_DANGER));
                            break;
                        case T_STAIRS:
                            attron(COLOR_PAIR(CP_UI) | A_BOLD);
                            mvaddch(sy, sx, '>');
                            attroff(COLOR_PAIR(CP_UI) | A_BOLD);
                            break;
                        case T_ITEM:
                            attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
                            mvaddch(sy, sx, '?');
                            attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);
                            break;
                        case T_GLIMMER:
                            attron(COLOR_PAIR(CP_GLIMMER) | A_BOLD);
                            mvaddch(sy, sx, '*');
                            attroff(COLOR_PAIR(CP_GLIMMER) | A_BOLD);
                            break;
                        default:
                            break;
                    }
                } else if (r->seen[ty][tx] && !visible) {
                    /* Previously seen but not currently visible: dim outline */
                    switch (t) {
                        case T_WALL:
                            attron(COLOR_PAIR(CP_WALL) | A_DIM);
                            mvaddch(sy, sx, '#');
                            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
                            break;
                        case T_FLOOR:
                            attron(COLOR_PAIR(CP_FLOOR) | A_DIM);
                            mvaddch(sy, sx, '.');
                            attroff(COLOR_PAIR(CP_FLOOR) | A_DIM);
                            break;
                        case T_DOOR_OPEN:
                            attron(COLOR_PAIR(CP_DEF) | A_DIM);
                            mvaddch(sy, sx, '+');
                            attroff(COLOR_PAIR(CP_DEF) | A_DIM);
                            break;
                        case T_DOOR_LOCK:
                            attron(COLOR_PAIR(CP_DEF) | A_DIM);
                            mvaddch(sy, sx, '=');
                            attroff(COLOR_PAIR(CP_DEF) | A_DIM);
                            break;
                        default:
                            break;
                    }
                    /* items/stairs/enemies hidden in remembered tiles */
                }
                /* else: never seen — render nothing (pure darkness) */
            }
        }
    }

    /* ── Enemies (all rooms, FOV-filtered) ── */
    for (int ri = 0; ri < fl->n_rooms; ri++) {
        Room *r = &fl->rooms[ri];
        for (int i = 0; i < r->n_enemies; i++) {
            Enemy *e = &r->enemies[i];
            if (!e->alive) continue;
            int ewx = r->wx + e->x, ewy = r->wy + e->y;
            int ddx = ewx - pwx, ddy = ewy - pwy;
            if (!in_fov(ddx, ddy)) continue;
            int sx = ewx - vx, sy = ewy - vy;
            if (sx < 0 || sy < 0 || sx >= G.gw || sy >= G.gh) continue;
            attr_t attr = COLOR_PAIR(e->cp);
            if (e->boss) attr |= A_BOLD;
            attron(attr);
            mvaddch(sy, sx, e->sym);
            attroff(attr);
        }
    }

    /* ── Player (always screen-centre) ── */
    {
        int sx = G.gw / 2, sy = G.gh / 2;
        attron(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);
        mvaddch(sy, sx, CHARS[p->chr].sym);
        attroff(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);
    }

    /* ── Projectiles ── */
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *pr = &G.projs[i];
        if (!pr->active) continue;
        int sx = (cr->wx + pr->x) - vx;
        int sy = (cr->wy + pr->y) - vy;
        if (sx < 0 || sy < 0 || sx >= G.gw || sy >= G.gh) continue;
        attron(COLOR_PAIR(pr->cp) | A_BOLD);
        mvaddch(sy, sx, pr->sym);
        attroff(COLOR_PAIR(pr->cp) | A_BOLD);
    }

    /* ── Boss HP bar (if boss in current room) ── */
    for (int i = 0; i < cr->n_enemies; i++) {
        Enemy *e = &cr->enemies[i];
        if (!e->alive || !e->boss) continue;
        int bw = G.gw - 4;
        if (bw < 4) bw = 4;
        attron(COLOR_PAIR(CP_BOSS) | A_BOLD);
        mvprintw_clip(1, 2, bw, "%s", e->name);
        attroff(COLOR_PAIR(CP_BOSS) | A_BOLD);
        draw_hp_bar(2, 2, bw, e->hp, e->max_hp);
        break;
    }
}

/* ─── Main game render ──────────────────────────────────────────────── */
void render_game(void) {
    getmaxyx(stdscr, G.th, G.tw);
    G.gw = G.tw - UI_W;
    G.gh = G.th - MSG_H;
    if (G.gw < 10) G.gw = 10;
    if (G.gh < 5)  G.gh = 5;

    clear();
    draw_room();
    draw_ui();
    draw_messages();
    refresh();
}

/* ─── Title screen ──────────────────────────────────────────────────── */

static const char *TOWER_ART[] = {
    "        [_] [_] [_]                   ",
    "       /|            |\\               ",
    "        |:  .  !  . :|                ",
    "       s|:  | || |  .:|~              ",
    "        |:.  .  !  . :|               ",
    "       ~|:  .  .  .  :|v              ",
    "        |:. !  .  !  :|               ",
    "       v|:  (      )  :|s             ",
    "        |:  +------+  :|              ",
    "       s|:============:|~             ",
    "        |:  .  !  .  :|               ",
    "        &|:.  .  .  . :|v             ",
    "         |:  !  .  !  :|              ",
    "        s|:  /------\\  :|~            ",
    "        ~|: | [    ] | :|             ",
    "         |: |        | :|v            ",
    "        v|:  +------+  :|s            ",
    "         |:. !  .  !  :|              ",
    "       s/|:  .  !  .  :|\\~            ",
    "      /  |:  .  .  .  :|  \\           ",
    "     /   |:  !  .  !  :|   \\          ",
    "    / v  |:============:|  s \\        ",
    "   /     |:  .  !  .  :|     \\        ",
    "  /======+============+=======\\       ",
    " /  .  !  :  .  !  .  :  !  .  \\      ",
    "/________________________________\\    ",
    NULL
};

void render_title(void) {
    clear();
    getmaxyx(stdscr, G.th, G.tw);

    int art_w = 38;
    int art_rows = 0;
    while (TOWER_ART[art_rows]) art_rows++;

    /* Art: right-aligned, fills rows 0..art_area, clipping spire top if short */
    int art_area = G.th - 2;   /* reserve bottom row for prompt */
    int skip = art_rows - art_area;
    if (skip < 0) skip = 0;
    int art_cx = G.tw - art_w;
    if (art_cx < 0) art_cx = 0;

    for (int i = skip; TOWER_ART[i]; i++) {
        int row = i - skip;
        if (row >= G.th - 1) break;
        const char *line = TOWER_ART[i];
        int len = (int)strlen(line);
        for (int j = 0; j < len; j++) {
            char c = line[j];
            if (c == ' ') { mvaddch(row, art_cx + j, ' '); continue; }
            if (c == '/' || c == '\\' || c == '|' || c == '_') {
                attron(COLOR_PAIR(CP_WALL) | A_BOLD);
                mvaddch(row, art_cx + j, c);
                attroff(COLOR_PAIR(CP_WALL) | A_BOLD);
            } else if (c == '(' || c == ')' || c == '[' || c == ']') {
                attron(COLOR_PAIR(CP_UI));
                mvaddch(row, art_cx + j, c);
                attroff(COLOR_PAIR(CP_UI));
            } else if (c == '=' || c == '-') {
                attron(COLOR_PAIR(CP_WALL) | A_DIM);
                mvaddch(row, art_cx + j, c);
                attroff(COLOR_PAIR(CP_WALL) | A_DIM);
            } else {
                attron(COLOR_PAIR(CP_DEF));
                mvaddch(row, art_cx + j, c);
                attroff(COLOR_PAIR(CP_DEF));
            }
        }
    }

    /* Intro text: left side, vertically centred alongside art */
    static const struct { const char *text; int cp; int bold; } INTRO[] = {
        { "A Tower Unto Heaven",          CP_UI,    1 },
        { "",                             CP_DEF,   0 },
        { "You are alone.",               CP_FLOOR, 0 },
        { "The world dies around you.",   CP_FLOOR, 0 },
        { "",                             CP_DEF,   0 },
        { "A Tower rises unto Heaven",    CP_DEF,   0 },
        { "",                             CP_DEF,   0 },
        { "At its roots:",                CP_WALL,  0 },
        { "  The dead",                   CP_DANGER,0 },
        { "  The scavengers",             CP_DANGER,0 },
        { "  Those consumed",             CP_DANGER,0 },
        { "  Those who linger",           CP_DANGER,0 },
        { "  And you",                    CP_PLAYER,0 },
        { "",                             CP_DEF,   0 },
        { "Salvation waits at the top.",  CP_ITEM,  1 },
        { NULL, 0, 0 }
    };
    int n_intro = 0;
    while (INTRO[n_intro].text) n_intro++;

    int text_start = (art_area - n_intro) / 2;
    if (text_start < 0) text_start = 0;
    int text_col = 2;

    for (int i = 0; i < n_intro; i++) {
        int row = text_start + i;
        if (row >= G.th - 1) break;
        if (INTRO[i].text[0] == '\0') continue;
        int attr = COLOR_PAIR(INTRO[i].cp) | (INTRO[i].bold ? A_BOLD : 0);
        if (INTRO[i].cp == CP_FLOOR) attr |= A_DIM;
        attron(attr);
        mvprintw(row, text_col, "%s", INTRO[i].text);
        attroff(attr);
    }

    /* Prompt pinned to bottom-left */
    attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
    mvprintw(G.th - 1, text_col, "[ Press any key ]");
    attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);

    refresh();
    getch();
}

/* ─── Character select ──────────────────────────────────────────────── */
int render_char_select(void) {
    int sel = 0;
    while (1) {
        clear();
        getmaxyx(stdscr, G.th, G.tw);
        int cx = (G.tw - 50) / 2;
        if (cx < 0) cx = 0;

        attron(COLOR_PAIR(CP_UI) | A_BOLD);
        mvprintw(1, cx, "Choose your survivor:");
        attroff(COLOR_PAIR(CP_UI) | A_BOLD);

        for (int i = 0; i < CH_COUNT; i++) {
            int row = 3 + i * 2;
            if (i == sel) {
                attron(COLOR_PAIR(CHARS[i].cp) | A_BOLD | A_REVERSE);
                mvprintw(row, cx, "  %s  ", CHARS[i].name);
                attroff(COLOR_PAIR(CHARS[i].cp) | A_BOLD | A_REVERSE);
            } else {
                attron(COLOR_PAIR(CP_DEF));
                mvprintw(row, cx, "  %s  ", CHARS[i].name);
                attroff(COLOR_PAIR(CP_DEF));
            }
        }

        /* Description box */
        int desc_y = 3 + CH_COUNT * 2 + 1;
        attron(COLOR_PAIR(CP_ITEM));
        mvprintw(desc_y, cx, "Flesh:%-3d Tem:%-2d Res:%-2d",
                 CHARS[sel].hp, CHARS[sel].atk, CHARS[sel].def);
        attroff(COLOR_PAIR(CP_ITEM));
        attron(COLOR_PAIR(CP_DEF));
        /* Multi-line desc */
        const char *desc = CHARS[sel].desc;
        int drow = desc_y + 1;
        char dbuf[256];
        strncpy(dbuf, desc, sizeof(dbuf)-1);
        char *line = strtok(dbuf, "\n");
        while (line && drow < G.th - 3) {
            mvprintw_clip(drow++, cx, G.tw - cx - 1, "%s", line);
            line = strtok(NULL, "\n");
        }
        attroff(COLOR_PAIR(CP_DEF));

        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvprintw(G.th-2, cx, "[up/down] select   [Enter] confirm");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);

        refresh();
        int ch = getch();
        switch (ch) {
            case KEY_UP:   case 'k': sel = (sel - 1 + CH_COUNT) % CH_COUNT; break;
            case KEY_DOWN: case 'j': sel = (sel + 1) % CH_COUNT; break;
            case '\n': case '\r': case KEY_ENTER: return sel;
            case 'q': return -1;
        }
    }
}

/* ─── Game over ─────────────────────────────────────────────────────── */
static const char *death_lines[] = {
    "You have died. Your bones remain in the Tower's cold halls.",
    "The Tower does not mourn you.",
    "Others will follow. They will find nothing.",
};

void render_game_over(void) {
    clear();
    getmaxyx(stdscr, G.th, G.tw);
    int cx = (G.tw - 50) / 2;
    if (cx < 0) cx = 0;
    int cy = G.th / 2 - 4;

    attron(COLOR_PAIR(CP_DANGER) | A_BOLD);
    mvprintw(cy, cx, "%s", death_lines[0]);
    attroff(COLOR_PAIR(CP_DANGER) | A_BOLD);

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvprintw(cy+1, cx, "%s", death_lines[1]);
    mvprintw(cy+2, cx, "%s", death_lines[2]);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    attron(COLOR_PAIR(CP_DEF));
    mvprintw(cy+4, cx, "Ascent: %d   Sin: %d   Turns: %d",
             G.player.floor_num+1, G.player.kills, G.player.turn);
    attroff(COLOR_PAIR(CP_DEF));

    attron(COLOR_PAIR(CP_ITEM));
    mvprintw(cy+6, cx, "[ Press any key to return ]");
    attroff(COLOR_PAIR(CP_ITEM));

    refresh();
    getch();
}

/* ─── Win screen ────────────────────────────────────────────────────── */
static const char *win_by_char[CH_COUNT] = {
    /* Jackal */
    "He put a crown of scrap on his head and called himself a king.\n"
    "A squatter in hallowed halls. A tyrant whose domain died long before his ascent.",
    /* Zealot */
    "He witnessed the cosmos bloom before him.\n"
    "He had come looking for answers. He found all of them.",
    /* Ferdinand */
    "The Throne grants many powers, but it cannot reverse death.\n"
    "Her flame of revenge went unsated, and slowly starved.",
    /* Running Boy */
    "He placed an offering, wrapped in brown paper, at the foot of the Throne.\n"
    "His task completed, he vanished, and left his memorial in the snow.",
    /* Deserter Hua */
    "\"Min... Min, look. We're going home.\"\n"
    "She held her daughter close, trying to keep her warm.",
};

void render_win(void) {
    clear();
    getmaxyx(stdscr, G.th, G.tw);
    int cx = (G.tw - 55) / 2;
    if (cx < 0) cx = 0;
    int cy = G.th / 2 - 6;

    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(cy, cx, "A Tower Unto Heaven is yours.");
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);

    attron(COLOR_PAIR(CP_FLOOR) | A_DIM);
    mvprintw(cy+1, cx, "Salvation found. The long climb ends.");
    attroff(COLOR_PAIR(CP_FLOOR) | A_DIM);

    attron(COLOR_PAIR(CP_ITEM));
    /* Print character ending */
    const char *ending = win_by_char[G.player.chr];
    char buf[512];
    strncpy(buf, ending, sizeof(buf)-1);
    char *line = strtok(buf, "\n");
    int row = cy + 3;
    while (line && row < G.th - 4) {
        mvprintw_clip(row++, cx, G.tw - cx - 1, "%s", line);
        line = strtok(NULL, "\n");
    }
    attroff(COLOR_PAIR(CP_ITEM));

    attron(COLOR_PAIR(CP_DEF));
    mvprintw(row+1, cx, "Sin: %d   Turns: %d", G.player.kills, G.player.turn);
    attroff(COLOR_PAIR(CP_DEF));

    attron(COLOR_PAIR(CP_UI));
    mvprintw(G.th-2, cx, "[ Press any key ]");
    attroff(COLOR_PAIR(CP_UI));

    refresh();
    getch();
}

/* ─── Help / manual ─────────────────────────────────────────────────── */

typedef struct { const char *key; const char *desc; } HelpEntry;

static const HelpEntry HELP_PAGES[][24] = {
    /* Page 0: Controls */
    {
        { NULL,          "── MOVEMENT ──────────────────────" },
        { "h / LEFT",    "Move west" },
        { "j / DOWN",    "Move south" },
        { "k / UP",      "Move north" },
        { "l / RIGHT",   "Move east" },
        { "y u b n",     "Move diagonally (NW NE SW SE)" },
        { ". / SPACE",   "Wait one turn (enemies still act)" },
        { NULL,          "" },
        { NULL,          "── COMBAT ────────────────────────" },
        { "bump enemy",  "Melee attack (move into an enemy)" },
        { "f + dir",     "Fire ranged weapon in a direction" },
        { "e",           "Throw a Whitefire Grenade (AoE room)" },
        { NULL,          "" },
        { NULL,          "── ITEMS & WORLD ─────────────────" },
        { "g / ,",       "Pick up item at your feet" },
        { "i",           "Open inventory / browse items" },
        { ">",           "Stairs — ascend to next floor" },
        { "+",           "Open door (clear room first)" },
        { NULL,          "" },
        { NULL,          "── OTHER ─────────────────────────" },
        { "?",           "This manual" },
        { "q",           "Quit to title" },
        { NULL, NULL }
    },
    /* Page 1: Mechanics */
    {
        { NULL,          "── HOW TO PLAY ───────────────────" },
        { "Goal",        "Climb 3 floors, defeat each boss," },
        { "",            "reach the Throne. Survive." },
        { NULL,          "" },
        { NULL,          "── ROOMS ─────────────────────────" },
        { "Enter room",  "Doors lock if enemies are inside." },
        { "Clear room",  "Kill all enemies — doors open." },
        { "Item room",   "No enemies; two items on the floor." },
        { "Boss room",   "One boss. Stairs appear on kill." },
        { NULL,          "" },
        { NULL,          "── TURNS ─────────────────────────" },
        { "Turn order",  "You act, then every enemy acts." },
        { "Fast enemies","Some enemies take 2 moves per turn." },
        { "Bleed",       "Inflicted by Rust/Axe: 1 dmg/turn." },
        { "Regen",       "Sutures/Tower Piece heal over time." },
        { "Lantern",     "Melting Lantern burns adj. enemies." },
        { NULL,          "" },
        { NULL,          "── DAMAGE ────────────────────────" },
        { "Melee",       "Temerity +/-1 - enemy Resolve, min 1." },
        { "Ranged",      "Edge +/-1 - enemy Resolve, min 1." },
        { "Grenade",     "~15 dmg to ALL enemies in room." },
        { NULL, NULL }
    },
    /* Page 2: Items */
    {
        { NULL,          "── NOTABLE ITEMS ─────────────────" },
        { "Old Beans",   "+15 max Flesh, heals 10 on pickup." },
        { "Sutures",     "Regenerate 1 Flesh every 4 turns." },
        { "Occam's Razor","  +3 Temerity." },
        { "Holy Woad",   "+5 Resolve." },
        { "Bloody Hands","  +6 Temerity, -5 Resolve, regen." },
        { "Lost Days",   "Enemies have 20% chance to miss." },
        { "Crown/Teeth", "30% chance to reflect damage back." },
        { "Rust",        "Your melee causes bleed (3 turns)." },
        { "What Remains","Survive one killing blow at 1 Flesh." },
        { "Heretic's Heart","Double Temerity, double dmg recv." },
        { "The Professional","10% instakill (non-boss)." },
        { "R+L Stones",  "Both: +5 Tem +5 Res +10 Flesh." },
        { "Tome of Kings","Items have enhanced effects (+50%)." },
        { "The Package", "Opens into 2 random items." },
        { "Nuclear Gospel","Gives ranged attack (3 dmg, rng 7)." },
        { "Grenades",    "3 charges AoE. Press [e] to use." },
        { NULL,          "" },
        { NULL,          "── STARTERS BY CHARACTER ─────────" },
        { "Jackal",      "9mm (ranged), Duffel bag." },
        { "Zealot",      "Tower Shield, Inscribed Axe, Shotgun." },
        { "Ferdinand",   "Embedded Bullet (+25 Flesh), Tome." },
        { "Running Boy", "The Package (2 random items)." },
        { NULL, NULL }
    },
    /* Page 3: Enemies & Bosses */
    {
        { NULL,          "── ENEMIES ───────────────────────" },
        { "o Organelle", "Slow, low Flesh. Abundant on floor 1." },
        { "b Botfly",    "Fast (2 moves/turn). Swarms." },
        { "B Black Lung","Ranged: exhales filth, rng 8." },
        { "s Shade",     "Phases through walls. Relentless." },
        { "r Plague Rat","Fast, medium Flesh." },
        { "T Tithe-Taker","Tough, high Resolve. Demands tribute." },
        { "C Censorite", "Very tough. Floor 2+." },
        { "d Drone",     "Fast, ranged. Floor 2+." },
        { NULL,          "" },
        { NULL,          "── BOSSES ────────────────────────" },
        { "M Inside-Out-Man","Floor 1. 80 Flesh. Melee." },
        { "K Ten-Knives","Floor 2. 140 Flesh. Throws knives." },
        { "O The Throne","Floor 3. 220 Flesh. Ranged + summons." },
        { NULL,          "" },
        { NULL,          "── TIPS ──────────────────────────" },
        { "Kite ranged", "Lure ranged enemies around corners." },
        { "Phase walls", "Shades ignore walls; don't corner." },
        { "Boss summons","Throne Beneath Heaven summons adds." },
        { "Grenades",    "Save for boss rooms." },
        { "Death save",  "What Remains: buy it if you see it." },
        { NULL, NULL }
    },
};
static const int HELP_N_PAGES = 4;
static const char *HELP_TITLES[] = {
    "Controls", "Mechanics", "Items", "Enemies & Bosses"
};

void render_help(void) {
    int page = 0;
    while (1) {
        clear();
        getmaxyx(stdscr, G.th, G.tw);
        int cx = 2;

        /* Header */
        attron(COLOR_PAIR(CP_UI) | A_BOLD);
        mvprintw(0, cx, "A TOWER UNTO HEAVEN  —  Manual");
        attroff(COLOR_PAIR(CP_UI) | A_BOLD);

        /* Page tabs */
        int tx = cx + 32;
        for (int p = 0; p < HELP_N_PAGES; p++) {
            if (p == page) attron(COLOR_PAIR(CP_ITEM) | A_BOLD | A_REVERSE);
            else           attron(COLOR_PAIR(CP_DEF));
            mvprintw(0, tx, " %s ", HELP_TITLES[p]);
            attroff(A_BOLD | A_REVERSE | COLOR_PAIR(CP_ITEM) | COLOR_PAIR(CP_DEF));
            tx += (int)strlen(HELP_TITLES[p]) + 3;
        }

        attron(COLOR_PAIR(CP_UI));
        mvhline(1, 0, '-', G.tw);
        attroff(COLOR_PAIR(CP_UI));

        /* Content */
        int row = 2;
        const HelpEntry *entries = HELP_PAGES[page];
        for (int i = 0; entries[i].key != NULL || entries[i].desc != NULL; i++) {
            if (row >= G.th - 2) break;
            const HelpEntry *e = &entries[i];
            if (e->key == NULL) {
                /* Section header */
                attron(COLOR_PAIR(CP_UI) | A_BOLD);
                mvprintw(row++, cx, "%s", e->desc);
                attroff(COLOR_PAIR(CP_UI) | A_BOLD);
            } else if (e->key[0] == '\0') {
                /* Continuation line (no key) */
                attron(COLOR_PAIR(CP_DEF));
                mvprintw(row++, cx + 20, "%s", e->desc);
                attroff(COLOR_PAIR(CP_DEF));
            } else {
                /* Key + description */
                attron(COLOR_PAIR(CP_ITEM));
                mvprintw(row, cx, "%-18s", e->key);
                attroff(COLOR_PAIR(CP_ITEM));
                attron(COLOR_PAIR(CP_DEF));
                mvprintw(row, cx + 18, "%s", e->desc);
                attroff(COLOR_PAIR(CP_DEF));
                row++;
            }
        }

        /* Footer */
        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvhline(G.th-2, 0, '-', G.tw);
        mvprintw(G.th-1, cx, "[h/l arrows] change page   [?/Esc/q] close");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);

        refresh();
        int ch = getch();
        switch (ch) {
            case 'l': case KEY_RIGHT: case KEY_NPAGE:
                page = (page + 1) % HELP_N_PAGES; break;
            case 'h': case KEY_LEFT:  case KEY_PPAGE:
                page = (page - 1 + HELP_N_PAGES) % HELP_N_PAGES; break;
            case '1': case '2': case '3': case '4':
                page = ch - '1'; break;
            case '?': case 27: case 'q': return;
        }
    }
}

/* ─── Inventory screen ──────────────────────────────────────────────── */
/* ─── Map screen ────────────────────────────────────────────────────── */
void render_map(void) {
    while (1) {
        clear();
        getmaxyx(stdscr, G.th, G.tw);
        Floor *f = G.cf;

        /* ── Bounding box of visible rooms ── */
        int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
        bool any = false;
        for (int i = 0; i < f->n_rooms; i++) {
            Room *r = &f->rooms[i];
            /* Include visited rooms, and unvisited rooms adjacent to visited */
            bool show = r->visited;
            if (!show) {
                for (int d = 0; d < 4; d++) {
                    int nb = r->conn[d];
                    if (nb >= 0 && f->rooms[nb].visited) { show = true; break; }
                }
            }
            if (!show) continue;
            int mx = r->map_x, my = r->map_y;
            if (!any) { min_x = max_x = mx; min_y = max_y = my; any = true; }
            else {
                if (mx < min_x) min_x = mx; if (mx > max_x) max_x = mx;
                if (my < min_y) min_y = my; if (my > max_y) max_y = my;
            }
        }

        /* ── Scale to fit available area ── */
        int avail_w = G.tw - 4;
        int avail_h = G.th - 7;
        int map_span_x = (max_x - min_x) + 1;
        int map_span_y = (max_y - min_y) + 1;
        int sx = 5, sy = 3;
        while (map_span_x * sx > avail_w && sx > 2) sx--;
        while (map_span_y * sy > avail_h && sy > 2) sy--;

        /* Origin: top-left of centred map block */
        int ox = 2 + (avail_w - map_span_x * sx) / 2;
        int oy = 3 + (avail_h - map_span_y * sy) / 2;

#define MX(mx) (ox + ((mx) - min_x) * sx)
#define MY(my) (oy + ((my) - min_y) * sy)

        /* ── Header ── */
        attron(COLOR_PAIR(CP_UI) | A_BOLD);
        mvprintw(0, (G.tw - 28) / 2,
                 "Floor %d/%d  —  Explored Map",
                 G.player.floor_num + 1, NUM_FLOORS);
        attroff(COLOR_PAIR(CP_UI) | A_BOLD);
        attron(COLOR_PAIR(CP_UI));
        for (int x = 0; x < G.tw; x++) mvaddch(1, x, '-');
        attroff(COLOR_PAIR(CP_UI));

        /* ── Draw connections (under room symbols) ── */
        for (int i = 0; i < f->n_rooms; i++) {
            if (!f->rooms[i].visited) continue;
            int ix = MX(f->rooms[i].map_x);
            int iy = MY(f->rooms[i].map_y);

            for (int d = 0; d < 4; d++) {
                int nb = f->rooms[i].conn[d];
                if (nb < 0) continue;
                int nx = MX(f->rooms[nb].map_x);
                int ny = MY(f->rooms[nb].map_y);

                int cp   = f->rooms[nb].visited ? CP_DEF : CP_WALL;
                int attr = f->rooms[nb].visited ? 0 : A_DIM;
                attron(COLOR_PAIR(cp) | attr);

                if (iy == ny) {
                    /* horizontal */
                    int lx = (ix < nx) ? ix + 2 : nx + 2;
                    int rx = (ix < nx) ? nx - 2 : ix - 2;
                    for (int x = lx; x <= rx; x++)
                        if (x >= 0 && x < G.tw) mvaddch(iy, x, '-');
                } else if (ix == nx) {
                    /* vertical */
                    int ty = (iy < ny) ? iy + 1 : ny + 1;
                    int by = (iy < ny) ? ny - 1 : iy - 1;
                    for (int y = ty; y <= by; y++)
                        if (y >= 2 && y < G.th - 4) mvaddch(y, ix, '|');
                }
                attroff(COLOR_PAIR(cp) | attr);
            }
        }

        /* ── Draw room symbols (significant rooms only) ── */
        for (int i = 0; i < f->n_rooms; i++) {
            Room *r = &f->rooms[i];

            /* Skip plain corridors / junctions — only shown as connection lines */
            if (r->is_corridor && !r->is_alcove) continue;

            /* Visibility: visited, or directly adjacent to visited */
            bool visible = r->visited;
            if (!visible) {
                for (int d = 0; d < 4; d++) {
                    int nb = r->conn[d];
                    if (nb >= 0 && f->rooms[nb].visited) { visible = true; break; }
                }
            }
            if (!visible) continue;

            int sx_pos = MX(r->map_x);
            int sy_pos = MY(r->map_y);
            if (sy_pos < 2 || sy_pos >= G.th - 4) continue;
            if (sx_pos < 1 || sx_pos >= G.tw - 1) continue;

            const char *sym;
            int cp, extra = 0;

            if (i == G.player.current_room) {
                sym = "[@]"; cp = CP_PLAYER; extra = A_BOLD;
            } else if (!r->visited) {
                sym = "[?]"; cp = CP_WALL; extra = A_DIM;
            } else if (i == f->start_room) {
                sym = "[S]"; cp = CP_UI;
            } else if (r->boss_room) {
                sym = r->cleared ? "[x]" : "[B]";
                cp = CP_BOSS; extra = A_BOLD;
            } else if (r->item_room) {
                sym = "[$]"; cp = CP_ITEM;
            } else if (r->is_alcove) {
                sym = r->n_items > 0 ? "[.]" : "[_]"; cp = CP_WALL;
            } else if (r->cleared) {
                sym = "[ ]"; cp = CP_DEF; extra = A_DIM;
            } else {
                sym = "[!]"; cp = CP_DANGER;
            }

            attron(COLOR_PAIR(cp) | extra);
            mvprintw(sy_pos, sx_pos - 1, "%s", sym);
            attroff(COLOR_PAIR(cp) | extra);
        }

#undef MX
#undef MY

        /* ── Footer ── */
        attron(COLOR_PAIR(CP_UI));
        for (int x = 0; x < G.tw; x++) mvaddch(G.th - 3, x, '-');
        attroff(COLOR_PAIR(CP_UI));

        /* Legend */
        int lx = 1, ly = G.th - 2;
        struct { const char *sym; int cp; int attr; const char *label; } leg[] = {
            { "[@]", CP_PLAYER, A_BOLD, "You"     },
            { "[S]", CP_UI,     0,      "Start"   },
            { "[!]", CP_DANGER, 0,      "Enemies" },
            { "[ ]", CP_DEF,    A_DIM,  "Cleared" },
            { "[$]", CP_ITEM,   0,      "Items"   },
            { "[B]", CP_BOSS,   A_BOLD, "Boss"    },
            { "[.]", CP_WALL,   0,      "Alcove"  },
            { "[?]", CP_WALL,   A_DIM,  "Unknown" },
        };
        for (int k = 0; k < 8 && lx < G.tw - 12; k++) {
            attron(COLOR_PAIR(leg[k].cp) | leg[k].attr);
            mvprintw(ly, lx, "%s", leg[k].sym);
            attroff(COLOR_PAIR(leg[k].cp) | leg[k].attr);
            lx += 3;
            attron(COLOR_PAIR(CP_DEF));
            mvprintw(ly, lx, " %-9s", leg[k].label);
            attroff(COLOR_PAIR(CP_DEF));
            lx += 10;
        }

        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvprintw(G.th - 1, (G.tw - 16) / 2, "[m / Esc] close");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);

        refresh();
        int ch = getch();
        if (ch == 'm' || ch == 27 || ch == 'q') return;
    }
}

/* ─── Inventory: stat line helper ──────────────────────────────────── */
static void inv_stat(int y, int x, int w, const char *label, const char *val, int cp) {
    attron(COLOR_PAIR(CP_UI));
    mvprintw(y, x, "%-*s", (int)(w/2), label);
    attroff(COLOR_PAIR(CP_UI));
    attron(COLOR_PAIR(cp) | A_BOLD);
    mvprintw(y, x + w/2, "%s", val);
    attroff(COLOR_PAIR(cp) | A_BOLD);
}

static void inv_flag(int *row, int x, const char *text, int cp) {
    attron(COLOR_PAIR(cp));
    mvprintw((*row)++, x, "  + %s", text);
    attroff(COLOR_PAIR(cp));
}

void render_inventory(int *sel) {
    Player *p = &G.player;

    while (1) {
        clear();
        getmaxyx(stdscr, G.th, G.tw);

        /* Layout: left list panel | right detail panel */
        int list_w  = 26;
        int sep_x   = list_w;
        int det_x   = sep_x + 2;
        int det_w   = G.tw - det_x - 1;
        if (det_w < 20) det_w = 20;

        /* ── Header ── */
        attron(COLOR_PAIR(CP_UI) | A_BOLD);
        mvprintw(0, 1, "INVENTORY  %d/%d slots", p->n_items, p->max_items);
        attroff(COLOR_PAIR(CP_UI) | A_BOLD);

        /* ── Player stat summary bar ── */
        attron(COLOR_PAIR(CP_DEF));
        mvprintw(0, 12, "Flesh %d/%d", p->hp, p->max_hp);
        mvprintw(0, 26, "Tem %d", p->atk);
        mvprintw(0, 34, "Res %d", p->def);
        if (p->spd != 0)
            mvprintw(0, 42, "Gait %+d", p->spd);
        if (p->has_ranged)
            mvprintw(0, 40, "SHOT %d  RNG %d", p->r_dmg, p->r_rng);
        if (p->grenades > 0)
            mvprintw(0, 58, "GRENADES %d", p->grenades);
        attroff(COLOR_PAIR(CP_DEF));
        attron(COLOR_PAIR(CP_UI));
        mvprintw(0, 70, "Mom %d", p->momentum);
        attroff(COLOR_PAIR(CP_UI));
        attroff(COLOR_PAIR(CP_DEF));

        /* Divider under header */
        attron(COLOR_PAIR(CP_UI));
        for (int x = 0; x < G.tw; x++) mvaddch(1, x, '-');
        /* Vertical separator */
        for (int y = 1; y < G.th - 2; y++) mvaddch(y, sep_x, '|');
        attroff(COLOR_PAIR(CP_UI));

        /* ── Left panel: item list ── */
        int list_rows = G.th - 4;  /* rows available for list */
        int scroll = 0;
        if (*sel >= list_rows) scroll = *sel - list_rows + 1;

        if (p->n_items == 0) {
            attron(COLOR_PAIR(CP_WALL) | A_DIM);
            mvprintw(3, 1, "You carry nothing.");
            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
        }

        for (int i = 0; i < p->n_items; i++) {
            int row = 2 + (i - scroll);
            if (row < 2 || row >= G.th - 2) continue;
            ItemType it = p->items[i];
            if (it == IT_NONE) continue;
            const ItemDef *d = &ITEMS[it];

            if (i == *sel) {
                attron(COLOR_PAIR(d->cp) | A_REVERSE | A_BOLD);
            } else {
                attron(COLOR_PAIR(d->cp));
            }
            /* Symbol + name, clipped to list width */
            char line[32];
            snprintf(line, sizeof(line), " %c %-*s", d->sym, list_w - 4, d->name);
            line[list_w - 1] = '\0';
            mvprintw(row, 0, "%s", line);
            attroff(COLOR_PAIR(d->cp) | A_REVERSE | A_BOLD);
        }

        /* Scroll indicator */
        if (p->n_items > list_rows) {
            attron(COLOR_PAIR(CP_UI) | A_DIM);
            if (scroll > 0)
                mvprintw(2, list_w - 2, "^");
            if (scroll + list_rows < p->n_items)
                mvprintw(G.th - 3, list_w - 2, "v");
            attroff(COLOR_PAIR(CP_UI) | A_DIM);
        }

        /* ── Right panel: item detail ── */
        if (*sel < p->n_items && p->items[*sel] != IT_NONE) {
            const ItemDef *d = &ITEMS[p->items[*sel]];
            int dr = 2;

            /* Item name */
            attron(COLOR_PAIR(d->cp) | A_BOLD);
            mvprintw_clip(dr++, det_x, det_w, "%c  %s", d->sym, d->name);
            attroff(COLOR_PAIR(d->cp) | A_BOLD);
            dr++;

            /* Flavour description, word-wrapped roughly */
            attron(COLOR_PAIR(CP_FLOOR) | A_DIM);
            char dbuf[128];
            snprintf(dbuf, sizeof(dbuf), "\"%s\"", d->desc);
            /* Naive wrap at det_w */
            int dlen = (int)strlen(dbuf);
            int pos = 0;
            while (pos < dlen && dr < G.th - 8) {
                int end = pos + det_w;
                if (end >= dlen) end = dlen;
                else {
                    /* back up to last space */
                    int e2 = end;
                    while (e2 > pos && dbuf[e2] != ' ') e2--;
                    if (e2 > pos) end = e2;
                }
                char tmp[128] = {0};
                strncpy(tmp, dbuf + pos, (size_t)(end - pos));
                mvprintw(dr++, det_x, "%s", tmp);
                pos = end;
                while (pos < dlen && dbuf[pos] == ' ') pos++;
            }
            attroff(COLOR_PAIR(CP_FLOOR) | A_DIM);
            dr++;

            /* ── Mechanical stats ── */
            attron(COLOR_PAIR(CP_UI));
            mvprintw(dr++, det_x, "Effects:");
            attroff(COLOR_PAIR(CP_UI));

            char buf[64];
            if (d->max_hp > 0) {
                snprintf(buf, sizeof(buf), "+%d max HP", d->max_hp);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            if (d->hp_heal > 0) {
                snprintf(buf, sizeof(buf), "Heal %d Flesh on pickup", d->hp_heal);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            if (d->atk > 0) {
                snprintf(buf, sizeof(buf), "+%d Temerity", d->atk);
                inv_flag(&dr, det_x, buf, CP_ITEM);
            } else if (d->atk < 0) {
                snprintf(buf, sizeof(buf), "%d Temerity", d->atk);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            if (d->def > 0) {
                snprintf(buf, sizeof(buf), "+%d Resolve", d->def);
                inv_flag(&dr, det_x, buf, CP_ITEM);
            } else if (d->def < 0) {
                snprintf(buf, sizeof(buf), "%d Resolve", d->def);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            if (d->spd > 0) {
                snprintf(buf, sizeof(buf), "+%d Gait (extra move%s)",
                         d->spd, d->spd > 1 ? "s" : "");
                inv_flag(&dr, det_x, buf, CP_PLAYER);
            } else if (d->spd < 0) {
                snprintf(buf, sizeof(buf), "%d Gait (sluggish)", d->spd);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            if (d->momentum > 0) {
                snprintf(buf, sizeof(buf), "+%d Momentum", d->momentum);
                inv_flag(&dr, det_x, buf, CP_UI);
            }
            if (d->gives_ranged) {
                if (d->r_ammo < 0)
                    snprintf(buf, sizeof(buf), "Ranged: %d dmg, rng %d, inf ammo",
                             d->r_dmg, d->r_rng);
                else
                    snprintf(buf, sizeof(buf), "Ranged: %d dmg, rng %d, %d ammo",
                             d->r_dmg, d->r_rng, d->r_ammo);
                inv_flag(&dr, det_x, buf, CP_ITEM);
            }
            if (d->regen)  {
                snprintf(buf, sizeof(buf), "Regen %.2f Flesh/turn", d->regen_rate);
                inv_flag(&dr, det_x, buf, CP_PLAYER);
            }
            if (d->instakill) {
                snprintf(buf, sizeof(buf), "%.0f%% instakill (non-boss)", d->ik_ch * 100.0f);
                inv_flag(&dr, det_x, buf, CP_MAGIC);
            }
            if (d->reflect) {
                snprintf(buf, sizeof(buf), "%.0f%% damage reflect", d->rf_ch * 100.0f);
                inv_flag(&dr, det_x, buf, CP_MAGIC);
            }
            if (d->bleed)
                inv_flag(&dr, det_x, "Melee causes bleed (3 turns)", CP_BLOOD);
            if (d->item_boost)
                inv_flag(&dr, det_x, "All item effects +50%", CP_MAGIC);
            if (d->extra_slots) {
                char slotbuf[40];
                snprintf(slotbuf, sizeof(slotbuf), "+%d item slots (total %d)",
                         d->extra_slots, p->max_items);
                inv_flag(&dr, det_x, slotbuf, CP_ITEM);
            }
            if (p->items[*sel] == IT_HERETIC)
                inv_flag(&dr, det_x, "Double ATK, double dmg received", CP_BOSS);
            if (p->items[*sel] == IT_REMAINS)
                inv_flag(&dr, det_x, p->death_save_used
                         ? "Death save: SPENT" : "Survive one killing blow at 1 HP",
                         p->death_save_used ? CP_WALL : CP_MAGIC);
            if (p->items[*sel] == IT_LOSTDAYS)
                inv_flag(&dr, det_x, "Enemies miss 20% of attacks", CP_MAGIC);
            if (p->items[*sel] == IT_LANTERN)
                inv_flag(&dr, det_x, "Adjacent enemies take 1 dmg/turn", CP_ITEM);
            if (d->consumable) {
                snprintf(buf, sizeof(buf), "Charges remaining: %d", p->grenades);
                inv_flag(&dr, det_x, buf, CP_DANGER);
            }
            /* stone synergy hint */
            if (p->items[*sel] == IT_RSTONE || p->items[*sel] == IT_LSTONE) {
                if (p->rstone && p->lstone)
                    inv_flag(&dr, det_x, "SYNERGY ACTIVE: +5 ATK +5 DEF +10 HP", CP_MAGIC);
                else
                    inv_flag(&dr, det_x, "Synergy: find the other Stone", CP_WALL);
            }

            /* stat totals reminder at bottom of detail panel */
            int br = G.th - 5;
            if (br > dr + 1) {
                attron(COLOR_PAIR(CP_UI));
                for (int x = det_x; x < G.tw - 1; x++) mvaddch(br, x, '-');
                attroff(COLOR_PAIR(CP_UI));
                br++;
                char totals[128];
                snprintf(totals, sizeof(totals),
                         "Totals  Tem %d  Res %d  Flesh %d/%d",
                         p->atk, p->def, p->hp, p->max_hp);
                inv_stat(br, det_x, det_w, totals, "", CP_DEF);
            }
        } else if (p->n_items == 0) {
            attron(COLOR_PAIR(CP_WALL) | A_DIM);
            mvprintw(4, det_x, "No items carried.");
            mvprintw(5, det_x, "Find item rooms and boss drops");
            mvprintw(6, det_x, "to fill your pockets.");
            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
        }

        /* ── Footer ── */
        attron(COLOR_PAIR(CP_UI));
        for (int x = 0; x < G.tw; x++) mvaddch(G.th-2, x, '-');
        attroff(COLOR_PAIR(CP_UI));
        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvprintw(G.th-1, 1, "[k/up] [j/down] scroll   [i/Esc] close");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);

        refresh();
        int ch = getch();
        switch (ch) {
            case KEY_UP:   case 'k':
                if (*sel > 0) (*sel)--;
                break;
            case KEY_DOWN: case 'j':
                if (*sel < p->n_items - 1) (*sel)++;
                break;
            case 'i': case 27: return;
            case 'q': G.running = false; return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * THE LAST HEARTH — camp screen (Firelink Shrine equivalent)
 * ═══════════════════════════════════════════════════════════════════════ */

/* ─── Dialogue tree ──────────────────────────────────────────────────── */
typedef struct {
    const char *id;
    const char *lines[6];   /* null-terminated array of speech lines */
    const char *opts[4];    /* option labels (null = no more options) */
    const char *targets[4]; /* node id to jump to, or "ENTER"/"BACK" */
} DlgNode;

static const DlgNode ASH_NODES[] = {
    { "greet_first",
      { "...",
        "Another one.",
        "Sit, if you like. The cold does not",
        "care whether you rest or not.", NULL },
      { "What is the Tower?",
        "Who are you?",
        "I'm ready to climb.", NULL },
      { "topic_tower", "topic_who", "topic_ready", NULL }
    },
    { "greet_second",
      { "Ah. You came back down.",
        "They all do, at first.",
        "The fire is still here. As am I.", NULL },
      { "What is the Tower?",
        "Who are you?",
        "I'm going back up.", NULL },
      { "topic_tower", "topic_who", "topic_ready", NULL }
    },
    { "greet_third",
      { "Still climbing.",
        "Hm.", NULL },
      { "I'm going back up.",
        "...", NULL },
      { "topic_ready", "topic_silence", NULL, NULL }
    },
    { "topic_tower",
      { "Old. Wrong. Hungry.",
        "It was built to reach something.",
        "Whatever answered — it was not God.",
        "The Tower keeps climbing. Even now.", NULL },
      { "Who are you?",
        "I'm ready to climb.", NULL },
      { "topic_who", "topic_ready", NULL, NULL }
    },
    { "topic_who",
      { "Someone who stopped climbing.",
        "I tend the fire.",
        "It is enough. It has to be.", NULL },
      { "What is the Tower?",
        "I'm ready to climb.", NULL },
      { "topic_tower", "topic_ready", NULL, NULL }
    },
    { "topic_ready",
      { "Then go.",
        "Don't look for meaning in it.",
        "Just climb.", NULL },
      { "[Enter the Tower]", NULL },
      { "ENTER", NULL, NULL, NULL }
    },
    { "topic_silence",
      { "Yes.",
        "That is about right.", NULL },
      { "[Enter the Tower]", NULL },
      { "ENTER", NULL, NULL, NULL }
    },
    { NULL, {NULL}, {NULL}, {NULL} }
};

static const DlgNode *dlg_find(const char *id) {
    for (int i = 0; ASH_NODES[i].id != NULL; i++)
        if (strcmp(ASH_NODES[i].id, id) == 0)
            return &ASH_NODES[i];
    return &ASH_NODES[0];
}


/* ─── Dialogue box ───────────────────────────────────────────────────── */
static void draw_dlg_box(const DlgNode *node, int line_idx, bool show_opts) {
    int bw = 58;
    int bx = (G.tw - bw) / 2;
    int by = 14;

    /* Box border */
    attron(COLOR_PAIR(CP_UI));
    mvhline(by,     bx, '-', bw);
    mvhline(by + 8, bx, '-', bw);
    for (int r = by+1; r < by+8; r++) {
        mvaddch(r, bx,      '|');
        mvaddch(r, bx+bw-1, '|');
    }
    attroff(COLOR_PAIR(CP_UI));

    /* Speaker name */
    attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
    mvprintw(by, bx + 2, "[ The Ashwarden ]");
    attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);

    /* Speech lines (show all up to current) */
    int row = by + 1;
    for (int i = 0; i <= line_idx && node->lines[i] != NULL && row < by+5; i++) {
        attron(COLOR_PAIR(CP_DEF));
        mvprintw_clip(row++, bx + 2, bw - 4, "%s", node->lines[i]);
        attroff(COLOR_PAIR(CP_DEF));
    }

    /* Options or advance prompt */
    if (show_opts) {
        int orow = by + 5;
        for (int i = 0; i < 4 && node->opts[i] != NULL && orow < by+8; i++) {
            attron(COLOR_PAIR(CP_PLAYER));
            mvprintw(orow++, bx + 2, "[%d] %s", i+1, node->opts[i]);
            attroff(COLOR_PAIR(CP_PLAYER));
        }
    } else {
        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvprintw(by + 6, bx + 2, "[Space] continue");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);
    }
}

/* ─── Main camp screen ───────────────────────────────────────────────── */
void render_camp(void) {
    halfdelay(4);   /* getch() times out every 0.4 s → fire flickers */

    int  frame      = 0;
    bool in_talk    = false;
    bool done       = false;

    /* Pick greeting based on visit count */
    const char *cur_id;
    int visits = G.ashwarden_visits;
    if      (visits == 0) cur_id = "greet_first";
    else if (visits == 1) cur_id = "greet_second";
    else                  cur_id = "greet_third";

    const DlgNode *node  = dlg_find(cur_id);
    int            line_idx = 0;
    bool           show_opts = false;

    while (!done) {
        clear();
        render_shrine(frame);

        if (in_talk) {
            /* Count lines in current node */
            int nlines = 0;
            while (node->lines[nlines]) nlines++;
            show_opts = (line_idx >= nlines - 1);
            draw_dlg_box(node, line_idx, show_opts);
        } else {
            /* Idle controls */
            int bx = (G.tw - 54) / 2;
            attron(COLOR_PAIR(CP_UI) | A_DIM);
            mvprintw(G.th - 2, bx,
                     "[t] Talk to the Ashwarden   [Enter] Enter the Tower   [q] Quit");
            attroff(COLOR_PAIR(CP_UI) | A_DIM);
        }

        refresh();
        int ch = getch();

        if (ch == ERR) {
            /* Timeout → animate fire */
            frame = (frame + 1) % 3;
            continue;
        }

        if (!in_talk) {
            switch (ch) {
                case 't': case 'T':
                    in_talk  = true;
                    line_idx = 0;
                    show_opts = false;
                    break;
                case '\n': case '\r': case KEY_ENTER: case ' ':
                    G.ashwarden_visits++;
                    done = true;
                    break;
                case 'q': case 'Q':
                    G.running = false;
                    done = true;
                    break;
            }
        } else {
            /* In dialogue */
            if (!show_opts) {
                /* Advance text */
                if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                    int nlines = 0;
                    while (node->lines[nlines]) nlines++;
                    if (line_idx < nlines - 1)
                        line_idx++;
                    else
                        show_opts = true;
                }
            } else {
                /* Option selection */
                int opt = -1;
                if (ch >= '1' && ch <= '4') opt = ch - '1';

                if (opt >= 0 && opt < 4 && node->opts[opt] != NULL) {
                    const char *target = node->targets[opt];
                    if (strcmp(target, "ENTER") == 0) {
                        G.ashwarden_visits++;
                        done = true;
                    } else {
                        node     = dlg_find(target);
                        line_idx = 0;
                        show_opts = false;
                    }
                }
            }

            /* Esc closes dialogue */
            if (ch == 27) {
                in_talk   = false;
                node      = dlg_find(cur_id);
                line_idx  = 0;
                show_opts = false;
            }
        }
    }

    cbreak();   /* restore normal blocking input */
}

/* ─── Combat panel ───────────────────────────────────────────────────── */
/*
 * step 0 = choosing verb (press 1-8)
 * step 1 = choosing fuel (press g/m/a/f/i/t)
 * chosen_verb only meaningful when step == 1
 */
void render_combat(Enemy *e, int silenced_verb, int step, Verb chosen_verb) {
    clear();
    getmaxyx(stdscr, G.th, G.tw);
    Player *p = &G.player;

    static const char *ASP_LABEL[ASP_COUNT] = {
        "", "SHAME", "LOOP", "HUNGER", "JUDGEMENT", "STATIC", "ECHO", "STONE"
    };
    static const char *VERB_LABEL[VERB_COUNT] = {
        "REBUKE","FORGET","ECHO","MEND","CHANT","FEED","SEAL","LIE"
    };
    static const char *VERB_DESC[VERB_COUNT] = {
        "Direct force",    "Strip an aspect", "Mirror wound",   "Heal / purge Ash",
        "Ritual buffer",   "Satisfy hunger",  "Lock it still",  "Twist reality"
    };
    static const char  FUEL_KEY[FUEL_COUNT]    = {'g','m','a','f','i','t'};
    static const char *FUEL_LABEL[FUEL_COUNT]  = {
        "Glimmer","Memory","Ash","Flesh","Item","Turns"
    };

    /* ── Header ── */
    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(0, 2, "[ ENCOUNTER ]  %s", e->name);
    mvprintw(0, G.tw - 14, "Round: %-4d", G.combat_round);
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvhline(1, 0, '-', G.tw);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* ── Enemy: tension pips ── */
    int display_tension = e->tension;
    if (p->ash >= 67 && (rand() % 6 == 0)) {
        display_tension += (rand() % 3) - 1;
        if (display_tension < 0) display_tension = 0;
        if (display_tension > e->max_tension + 1) display_tension = e->max_tension + 1;
    }

    attron(COLOR_PAIR(CP_DANGER) | A_BOLD);
    mvprintw(2, 2, "TENSION %d/%d", display_tension, e->max_tension);
    attroff(COLOR_PAIR(CP_DANGER) | A_BOLD);
    attron(COLOR_PAIR(CP_DANGER));
    for (int i = 0; i < e->max_tension && i < 12; i++)
        mvaddch(2, 16 + i, i < display_tension ? '#' : '.');
    attroff(COLOR_PAIR(CP_DANGER));

    /* Aspects */
    int ax = 30;
    for (int i = 0; i < e->n_aspects && ax < G.tw - 12; i++) {
        attron(COLOR_PAIR(CP_MAGIC));
        mvprintw(2, ax, "[%s]", ASP_LABEL[e->aspects[i]]);
        attroff(COLOR_PAIR(CP_MAGIC));
        ax += (int)strlen(ASP_LABEL[e->aspects[i]]) + 3;
    }

    /* Threshold note */
    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    if (!e->threshold_fired)
        mvprintw(3, 2, "At tension %d: threshold event", e->threshold);
    else
        mvprintw(3, 2, "Threshold triggered.");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* Flavour */
    if (e->hit_msg) {
        attron(COLOR_PAIR(CP_DEF) | A_DIM);
        mvprintw(4, 4, "%s", e->hit_msg);
        attroff(COLOR_PAIR(CP_DEF) | A_DIM);
    }

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvhline(5, 0, '-', G.tw);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* ── Player stats ── */
    attron(COLOR_PAIR(CP_PLAYER));
    mvprintw(6, 2, "Flesh  %d/%d", p->hp, p->max_hp);
    attroff(COLOR_PAIR(CP_PLAYER));

    int ash_cp = p->ash >= 100 ? CP_BOSS :
                 p->ash >= 67  ? CP_DANGER :
                 p->ash >= 34  ? CP_ITEM : CP_PLAYER;
    attron(COLOR_PAIR(ash_cp));
    if (p->husk) mvprintw(6, 18, "Ash: HUSK");
    else         mvprintw(6, 18, "Ash: %-3d", p->ash);
    if (p->ash >= 100)     { attron(A_BOLD); mvprintw(6, 26, "[HUSK]");      attroff(A_BOLD); }
    else if (p->ash >= 67) mvprintw(6, 26, "[GLITCHING]");
    else if (p->ash >= 34) mvprintw(6, 26, "[MISFIRING]");
    attroff(COLOR_PAIR(ash_cp));

    attron(COLOR_PAIR(CP_GLIMMER));
    mvprintw(7, 2, "Glimmer %-3d", p->glimmer);
    attroff(COLOR_PAIR(CP_GLIMMER));

    attron(COLOR_PAIR(CP_MAGIC));
    mvprintw(7, 18, "Memories %-2d", p->n_memories);
    attroff(COLOR_PAIR(CP_MAGIC));

    if (p->chanting) {
        attron(COLOR_PAIR(CP_MAGIC) | A_DIM);
        mvprintw(7, 32, "~ chanting (%d) ~", p->chant_turns);
        attroff(COLOR_PAIR(CP_MAGIC) | A_DIM);
    }

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvhline(8, 0, '-', G.tw);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* ── Verb menu ── */
    int verb_active = (step == 0);
    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(9, 2, verb_active ? "CHOOSE VERB:" : "VERB chosen: %s", verb_active ? "" : VERB_LABEL[(int)chosen_verb]);
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);

    for (int i = 0; i < VERB_COUNT; i++) {
        int vy = 10 + (i / 4);
        int vx = 2 + (i % 4) * (G.tw / 4);
        bool silenced = (i == silenced_verb);
        bool husk_locked = p->husk && i != VERB_MEND && i != VERB_SEAL;
        bool is_chosen = (!verb_active && (int)chosen_verb == i);

        if (is_chosen) {
            attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
            mvprintw(vy, vx, "[%d] %-8s", i+1, VERB_LABEL[i]);
            attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);
        } else if (silenced || husk_locked) {
            attron(COLOR_PAIR(CP_WALL) | A_DIM);
            mvprintw(vy, vx, "[%d] %-8s", i+1, VERB_LABEL[i]);
            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
        } else {
            attron(COLOR_PAIR(CP_PLAYER) | A_BOLD);
            mvprintw(vy, vx, "[%d]", i+1);
            attroff(COLOR_PAIR(CP_PLAYER) | A_BOLD);
            attron(COLOR_PAIR(CP_DEF));
            mvprintw(vy, vx+3, " %-8s", VERB_LABEL[i]);
            attroff(COLOR_PAIR(CP_DEF));
        }
        if (vx + 13 < G.tw) {
            attron(COLOR_PAIR(CP_WALL) | A_DIM);
            mvprintw(vy, vx + 12, "%-16s", VERB_DESC[i]);
            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
        }
    }

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvhline(12, 0, '-', G.tw);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* ── Fuel menu ── */
    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(13, 2, step == 1 ? "CHOOSE FUEL:" : "FUEL:");
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);

    int fx = 16;
    for (int i = 0; i < FUEL_COUNT; i++) {
        bool avail = fuel_available((FuelType)i);
        if (avail) {
            attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
            mvprintw(13, fx, "[%c]", FUEL_KEY[i]);
            attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);
            attron(COLOR_PAIR(CP_DEF));
            mvprintw(13, fx+3, " %-8s", FUEL_LABEL[i]);
            attroff(COLOR_PAIR(CP_DEF));
        } else {
            attron(COLOR_PAIR(CP_WALL) | A_DIM);
            mvprintw(13, fx, "[%c] %-8s", FUEL_KEY[i], FUEL_LABEL[i]);
            attroff(COLOR_PAIR(CP_WALL) | A_DIM);
        }
        fx += 13;
        if (fx >= G.tw - 13) break;
    }

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvhline(14, 0, '-', G.tw);
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    /* ── Recent log ── */
    MsgLog *ml = &G.log;
    int msg_start = ml->count - 4;
    if (msg_start < 0) msg_start = 0;
    for (int i = msg_start; i < ml->count; i++) {
        int log_row = 15 + (i - msg_start);
        if (log_row >= G.th - 1) break;
        attron(COLOR_PAIR(CP_DEF) | A_DIM);
        mvprintw(log_row, 2, "> %s", ml->lines[i]);
        attroff(COLOR_PAIR(CP_DEF) | A_DIM);
    }

    /* ── Footer ── */
    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvprintw(G.th-1, 2, step == 0
        ? "[1-8] choose verb   [Esc] attempt to flee"
        : "[g/m/a/f/i/t] choose fuel   [Esc] cancel");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

    refresh();
}
