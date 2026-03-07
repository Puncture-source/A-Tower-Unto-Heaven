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
    mvprintw_clip(row++, px, UI_W-2, "HP %3d/%-3d", p->hp, p->max_hp);
    attroff(COLOR_PAIR(CP_DANGER));
    draw_hp_bar(row++, px, UI_W-2, p->hp, p->max_hp);

    /* Stats */
    attron(COLOR_PAIR(CP_DEF));
    mvprintw_clip(row++, px, UI_W-2, "ATK %-2d  DEF %-2d", p->atk, p->def);
    if (p->has_ranged)
        mvprintw_clip(row++, px, UI_W-2, "SHOT %-2d  RNG %-2d",
                      p->r_dmg, p->r_rng);
    if (p->r_ammo > 0)
        mvprintw_clip(row++, px, UI_W-2, "AMMO %-3d", p->r_ammo);
    else if (p->has_ranged && p->r_ammo < 0)
        mvprintw_clip(row++, px, UI_W-2, "AMMO  inf");
    if (p->grenades > 0)
        mvprintw_clip(row++, px, UI_W-2, "GRENADES %-2d", p->grenades);
    attroff(COLOR_PAIR(CP_DEF));
    row++;

    /* Floor / kills */
    attron(COLOR_PAIR(CP_UI));
    mvprintw_clip(row++, px, UI_W-2, "Floor %d/%-d  Kills %-3d",
                  p->floor_num+1, NUM_FLOORS, p->kills);
    attroff(COLOR_PAIR(CP_UI));
    row++;

    /* Items */
    attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
    mvprintw_clip(row++, px, UI_W-2, "ITEMS:");
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

/* ─── Room rendering ────────────────────────────────────────────────── */
static void draw_room(void) {
    Room *r = cur_room();
    Player *p = &G.player;

    /* Center room in game area */
    int rw = r->w + 2, rh = r->h + 2;
    int ox = (G.gw - rw) / 2;
    int oy = (G.gh - rh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    /* Tiles */
    for (int ty = 0; ty < rh && ty < RT_H; ty++) {
        for (int tx = 0; tx < rw && tx < RT_W; tx++) {
            int sx = ox + tx;
            int sy = oy + ty;
            if (sx >= G.gw || sy >= G.gh) continue;
            TileType t = r->tiles[ty][tx];
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
                default:
                    break;
            }
        }
    }

    /* Enemies */
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive) continue;
        int sx = ox + e->x, sy = oy + e->y;
        if (sx < 0 || sy < 0 || sx >= G.gw || sy >= G.gh) continue;
        attr_t attr = COLOR_PAIR(e->cp);
        if (e->boss) attr |= A_BOLD;
        attron(attr);
        mvaddch(sy, sx, e->sym);
        attroff(attr);
    }

    /* Player */
    {
        int sx = ox + p->x, sy = oy + p->y;
        if (sx >= 0 && sy >= 0 && sx < G.gw && sy < G.gh) {
            attron(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);
            mvaddch(sy, sx, CHARS[p->chr].sym);
            attroff(COLOR_PAIR(CHARS[p->chr].cp) | A_BOLD);
        }
    }

    /* Boss HP bar (if boss in room) */
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
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
void render_title(void) {
    clear();
    getmaxyx(stdscr, G.th, G.tw);
    int cy = G.th / 2 - 6;
    int cx = (G.tw - 22) / 2;
    if (cx < 0) cx = 0;

    attron(COLOR_PAIR(CP_UI) | A_BOLD);
    mvprintw(cy+0, cx, " A Tower Unto Heaven");
    attroff(COLOR_PAIR(CP_UI) | A_BOLD);

    attron(COLOR_PAIR(CP_FLOOR) | A_DIM);
    mvprintw(cy+2,  cx, "You are alone.");
    mvprintw(cy+3,  cx, "The world dies around you.");
    mvprintw(cy+4,  cx, "A Tower rises unto Heaven.");
    mvprintw(cy+6,  cx, "Salvation waits at the top.");
    attroff(COLOR_PAIR(CP_FLOOR) | A_DIM);

    attron(COLOR_PAIR(CP_ITEM));
    mvprintw(cy+9,  cx, "[ Press any key ]");
    attroff(COLOR_PAIR(CP_ITEM));

    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    mvprintw(G.th-2, cx, "hjkl / arrows: move  f+dir: shoot");
    mvprintw(G.th-1, cx, "e: grenade  g: pick up  i: inventory  q: quit");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);

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
        mvprintw(desc_y, cx, "HP:%-3d ATK:%-2d DEF:%-2d",
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
    mvprintw(cy+4, cx, "Floor reached: %d   Kills: %d   Turns: %d",
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
    mvprintw(row+1, cx, "Kills: %d   Turns: %d", G.player.kills, G.player.turn);
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
        { "Melee",       "ATK +/-1 - enemy DEF, min 1." },
        { "Ranged",      "Shot DMG +/-1 - enemy DEF, min 1." },
        { "Grenade",     "~15 dmg to ALL enemies in room." },
        { NULL, NULL }
    },
    /* Page 2: Items */
    {
        { NULL,          "── NOTABLE ITEMS ─────────────────" },
        { "Old Beans",   "+15 max HP, heals 10 HP on pickup." },
        { "Sutures",     "Regenerate 1 HP every 4 turns." },
        { "Occam's Razor","  +3 ATK." },
        { "Holy Woad",   "+5 DEF." },
        { "Bloody Hands","  +6 ATK but -3 DEF." },
        { "Lost Days",   "Enemies have 20% chance to miss." },
        { "Crown/Teeth", "30% chance to reflect damage back." },
        { "Rust",        "Your melee causes bleed (3 turns)." },
        { "What Remains","Survive one killing blow at 1 HP." },
        { "Heretic's Heart","Double ATK, but double damage recv." },
        { "The Professional","10% instakill (non-boss)." },
        { "R+L Stones",  "Both together: +5 ATK +5 DEF +10HP." },
        { "Tome of Kings","Items have enhanced effects (+50%)." },
        { "The Package", "Opens into 2 random items." },
        { "Nuclear Gospel","Gives ranged attack (3 dmg, rng 7)." },
        { "Grenades",    "3 charges AoE. Press [e] to use." },
        { NULL,          "" },
        { NULL,          "── STARTERS BY CHARACTER ─────────" },
        { "Jackal",      "9mm (ranged), Duffel bag." },
        { "Zealot",      "Tower Shield, Inscribed Axe, Shotgun." },
        { "Ferdinand",   "Embedded Bullet (+25 HP), Tome." },
        { "Running Boy", "The Package (2 random items)." },
        { NULL, NULL }
    },
    /* Page 3: Enemies & Bosses */
    {
        { NULL,          "── ENEMIES ───────────────────────" },
        { "o Organelle", "Slow, low HP. Abundant on floor 1." },
        { "b Botfly",    "Fast (2 moves/turn). Swarms." },
        { "B Black Lung","Ranged: exhales filth, rng 8." },
        { "s Shade",     "Phases through walls. Relentless." },
        { "r Plague Rat","Fast, medium HP." },
        { "T Tithe-Taker","Tough, high DEF. Demands tribute." },
        { "C Censorite", "Very tough. Floor 2+." },
        { "d Drone",     "Fast, ranged. Floor 2+." },
        { NULL,          "" },
        { NULL,          "── BOSSES ────────────────────────" },
        { "M Inside-Out-Man","Floor 1. 80 HP. Melee." },
        { "K Ten-Knives","Floor 2. 140 HP. Throws knives." },
        { "O The Throne","Floor 3. 220 HP. Ranged + summons." },
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
void render_inventory(int *sel) {
    Player *p = &G.player;
    while (1) {
        clear();
        getmaxyx(stdscr, G.th, G.tw);
        int cx = 4;

        attron(COLOR_PAIR(CP_UI) | A_BOLD);
        mvprintw(0, cx, "INVENTORY");
        attroff(COLOR_PAIR(CP_UI) | A_BOLD);

        if (p->n_items == 0) {
            attron(COLOR_PAIR(CP_WALL));
            mvprintw(2, cx, "You carry nothing.");
            attroff(COLOR_PAIR(CP_WALL));
        }

        for (int i = 0; i < p->n_items; i++) {
            ItemType it = p->items[i];
            if (it == IT_NONE) continue;
            int row = 2 + i;
            if (i == *sel) {
                attron(COLOR_PAIR(CP_ITEM) | A_REVERSE | A_BOLD);
                mvprintw_clip(row, cx, G.tw-cx-1, "  %s", ITEMS[it].name);
                attroff(COLOR_PAIR(CP_ITEM) | A_REVERSE | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_DEF));
                mvprintw_clip(row, cx, G.tw-cx-1, "  %s", ITEMS[it].name);
                attroff(COLOR_PAIR(CP_DEF));
            }
        }

        /* Description of selected */
        if (*sel < p->n_items && p->items[*sel] != IT_NONE) {
            int dy = 3 + p->n_items + 1;
            const ItemDef *d = &ITEMS[p->items[*sel]];
            attron(COLOR_PAIR(CP_ITEM) | A_BOLD);
            mvprintw_clip(dy, cx, G.tw-cx-1, "%s", d->name);
            attroff(COLOR_PAIR(CP_ITEM) | A_BOLD);
            attron(COLOR_PAIR(CP_FLOOR));
            mvprintw_clip(dy+1, cx, G.tw-cx-1, "\"%s\"", d->desc);
            if (d->consumable)
                mvprintw_clip(dy+2, cx, G.tw-cx-1,
                              "Charges: %d", p->charges[*sel]);
            attroff(COLOR_PAIR(CP_FLOOR));
        }

        attron(COLOR_PAIR(CP_UI) | A_DIM);
        mvprintw(G.th-2, cx, "[up/down] browse   [i/Esc] close");
        attroff(COLOR_PAIR(CP_UI) | A_DIM);

        refresh();
        int ch = getch();
        switch (ch) {
            case KEY_UP:   case 'k': if (*sel > 0) (*sel)--; break;
            case KEY_DOWN: case 'j': if (*sel < p->n_items-1) (*sel)++; break;
            case 'i': case 27: return;
            case 'q': G.running = false; return;
        }
    }
}
