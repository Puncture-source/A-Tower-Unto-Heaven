#include "game.h"

/* ─── Basic room builder ────────────────────────────────────────────── */
static void make_room(Room *r, int w, int h) {
    memset(r, 0, sizeof(Room));
    if (w > MAX_ROOM_W) w = MAX_ROOM_W;
    if (h > MAX_ROOM_H) h = MAX_ROOM_H;
    r->w = w; r->h = h;
    for (int d = 0; d < 4; d++) r->conn[d] = -1;
    for (int y = 0; y < h+2; y++)
        for (int x = 0; x < w+2; x++)
            r->tiles[y][x] = T_WALL;
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            r->tiles[y][x] = T_FLOOR;
}

static int rw(void) { return 14 + rand() % 15; }
static int rh(void) { return  8 + rand() %  7; }

/* ─── Item placement ────────────────────────────────────────────────── */
static void place_item_in_room(Room *r, ItemType it) {
    if (r->n_items >= MAX_ROOM_IT) return;
    int x, y, tries = 0;
    do {
        x = 1 + rand() % r->w;
        y = 1 + rand() % r->h;
        tries++;
    } while (r->tiles[y][x] != T_FLOOR && tries < 50);
    if (r->tiles[y][x] != T_FLOOR) return;
    r->tiles[y][x] = T_ITEM;
    r->items[r->n_items] = it;
    r->ix[r->n_items]    = x;
    r->iy[r->n_items]    = y;
    r->n_items++;
}

/* ─── Corridor / junction primitives ────────────────────────────────── */

/* Cut a centred door in a room wall, record position. */
static void cut_door(Room *r, Dir dir) {
    int dx, dy;
    switch (dir) {
        case D_N: dx = r->w/2 + 1; dy = 0;        break;
        case D_S: dx = r->w/2 + 1; dy = r->h + 1; break;
        case D_E: dx = r->w + 1;   dy = r->h/2+1; break;
        default:  dx = 0;          dy = r->h/2+1; break;
    }
    r->tiles[dy][dx] = T_DOOR_OPEN;
    r->door_x[dir]   = dx;
    r->door_y[dir]   = dy;
}

/* Directly link room a (exiting dir_a) to room b. */
static void rlink(Floor *f, int a, Dir da, int b) {
    cut_door(&f->rooms[a], da);
    cut_door(&f->rooms[b], OPP[da]);
    f->rooms[a].conn[da]      = b;
    f->rooms[b].conn[OPP[da]] = a;
}

/* Append a narrow straight corridor; return its index. */
static int make_corr(Floor *f, Dir dir, int len) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    int idx = f->n_rooms++;
    Room *c = &f->rooms[idx];
    memset(c, 0, sizeof(Room));
    for (int d = 0; d < 4; d++) c->conn[d] = -1;
    c->cleared = c->is_corridor = true;
    int w = (dir == D_E || dir == D_W) ? len : 3;
    int h = (dir == D_N || dir == D_S) ? len : 3;
    c->w = w; c->h = h;
    for (int y = 0; y < h+2; y++)
        for (int x = 0; x < w+2; x++)
            c->tiles[y][x] = T_WALL;
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            c->tiles[y][x] = T_FLOOR;
    /* Decorative strut breaks up long EW corridors */
    if (len >= 8 && (dir == D_E || dir == D_W))
        c->tiles[1][(w+2)/2] = T_WALL;
    return idx;
}

/* Append a 5x5 junction room (no doors yet); return its index. */
static int make_junc(Floor *f) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    int idx = f->n_rooms++;
    Room *j = &f->rooms[idx];
    memset(j, 0, sizeof(Room));
    for (int d = 0; d < 4; d++) j->conn[d] = -1;
    j->cleared = j->is_corridor = true;
    j->w = 5; j->h = 5;
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++)
            j->tiles[y][x] = T_WALL;
    for (int y = 1; y <= 5; y++)
        for (int x = 1; x <= 5; x++)
            j->tiles[y][x] = T_FLOOR;
    return idx;
}

/* Append a small dead-end alcove (optionally with an item); return index. */
static int make_alcove(Floor *f, bool give_item) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    int idx = f->n_rooms++;
    Room *d = &f->rooms[idx];
    int w = 5 + rand()%4, h = 4 + rand()%3;
    memset(d, 0, sizeof(Room));
    for (int i = 0; i < 4; i++) d->conn[i] = -1;
    d->cleared = d->is_corridor = d->is_alcove = true;
    d->w = w; d->h = h;
    for (int y = 0; y < h+2; y++)
        for (int x = 0; x < w+2; x++)
            d->tiles[y][x] = T_WALL;
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            d->tiles[y][x] = T_FLOOR;
    if (give_item) place_item_in_room(d, random_pool_item());
    return idx;
}

/* Pick a random perpendicular direction to main. */
static Dir perp_to(Dir main) {
    if (main == D_E || main == D_W) return rand()%2 ? D_N : D_S;
    else                            return rand()%2 ? D_E : D_W;
}

/* ─── build_connection ──────────────────────────────────────────────── *
 *
 * Connects room a to room b in direction `main`. If side >= 0, at least
 * one junction in the connection will branch off toward `side`.
 *
 * Four patterns (chosen randomly):
 *
 *  STRAIGHT   A -[corr]- B
 *
 *  T-FORK     A -[J]-> B
 *                 |
 *              [corr]-> side
 *    Player enters J from A, sees two exits: continue toward B or detour.
 *
 *  S-BEND     A -[J1]-perp-[J2]- B
 *    Player goes main->perp->main. Path bends through two junctions.
 *
 *  S-BEND     A -[J1]-perp-[J2]- B
 *  + FORK         |
 *              [corr]-> side
 *    J1 has two perpendicular exits (perp and OPP[perp]).
 *    Player must choose north or south at J1; one path leads to B, other
 *    to side content. Neither is labelled.
 */
static void build_connection(Floor *f, int a, int b, Dir main, int side) {
    Dir p    = perp_to(main);
    int clen = 5 + rand() % 6;

    /* Choose pattern */
    int type;
    if (side < 0) type = rand() % 2;       /* 0=straight, 1=S-bend */
    else          type = 2 + rand() % 2;   /* 2=T-fork, 3=S-bend+fork */

    switch (type) {

    case 0: {
        /* STRAIGHT: A -[corr]- B */
        int ci = make_corr(f, main, clen);
        if (ci < 0) break;
        rlink(f, a, main, ci);
        rlink(f, ci, main, b);
        break;
    }

    case 1: {
        /* S-BEND: A -[J1] -perp- [J2]- B
         * Player turns (main → perp → main). */
        int j1 = make_junc(f);
        int j2 = make_junc(f);
        if (j1 < 0 || j2 < 0) break;
        rlink(f, a,  main, j1);
        rlink(f, j1, p,    j2);
        rlink(f, j2, main, b);
        break;
    }

    case 2: {
        /* T-FORK: A -[J]-> B,  J -[corr]- side
         * Real choice: continue toward B or take the detour. */
        int j  = make_junc(f);
        int cs = make_corr(f, p, clen);
        if (j < 0 || cs < 0) break;
        rlink(f, a,  main, j);
        rlink(f, j,  main, b);
        rlink(f, j,  p,    cs);
        rlink(f, cs, p,    side);
        break;
    }

    case 3: {
        /* S-BEND + FORK:
         *   A -[J1] -perp- [J2]- B
         *         |
         *      [corr]-> side
         *
         * J1 has three doors (entry from A, exit toward J2, exit toward side).
         * Player sees two perpendicular exits inside J1 — one leads forward,
         * one leads to side content. They don't know which is which. */
        int j1 = make_junc(f);
        int j2 = make_junc(f);
        int cs = make_corr(f, OPP[p], clen);
        if (j1 < 0 || j2 < 0 || cs < 0) break;
        rlink(f, a,  main,   j1);
        rlink(f, j1, p,      j2);   /* critical path bends perp then main */
        rlink(f, j2, main,   b);
        rlink(f, j1, OPP[p], cs);   /* optional branch, opposite perp */
        rlink(f, cs, OPP[p], side);
        break;
    }

    }
}

static void compute_map_layout(Floor *f);

/* ─── Floor generation ──────────────────────────────────────────────── */
void generate_floor(Floor *f, int floor_num) {
    memset(f, 0, sizeof(Floor));
    f->floor_num = floor_num;

    int n_combat = 3 + floor_num;   /* 3, 4, 5 */
    int ir = n_combat + 1;          /* item room  */
    int br = n_combat + 2;          /* boss room  */

    /* Pre-allocate all named rooms first; corridors/junctions appended after */
    f->n_rooms = br + 1;

    /* Start room */
    make_room(&f->rooms[0], rw(), rh());
    f->rooms[0].cleared = f->rooms[0].visited = true;

    /* Combat rooms */
    for (int i = 1; i <= n_combat; i++)
        make_room(&f->rooms[i], rw(), rh());

    /* Item room */
    make_room(&f->rooms[ir], rw(), rh());
    f->rooms[ir].item_room = f->rooms[ir].cleared = true;

    /* Boss room (slightly larger for drama) */
    make_room(&f->rooms[br], 26 + rand()%4, 12 + rand()%3);
    f->rooms[br].boss_room = f->rooms[br].has_stairs = true;

    /* Decide which spine segment carries the guaranteed item-room branch */
    int item_seg = rand() % n_combat;   /* segment i connects room i to i+1 */

    /* Build spine: room 0 → 1 → 2 → ... → n_combat */
    for (int i = 0; i < n_combat; i++) {
        int side = -1;
        if (i == item_seg) {
            side = ir;   /* guaranteed item-room branch */
        } else if (rand() % 2 == 0) {
            /* Random dead-end alcove: 1 in 3 chance holds a bonus item */
            side = make_alcove(f, (rand()%3 == 0));
        }
        build_connection(f, i, i+1, D_E, side);
    }

    /* Boss connection — occasional dead-end alcove off the approach */
    {
        int side = (rand()%2 == 0) ? make_alcove(f, false) : -1;
        build_connection(f, n_combat, br, D_E, side);
    }

    f->start_room = 0;
    f->boss_room  = br;

    /* Spawn enemies in combat rooms */
    for (int i = 1; i <= n_combat; i++)
        spawn_room_enemies(&f->rooms[i], floor_num);

    /* Boss */
    spawn_boss(&f->rooms[br], floor_num);

    /* Items in item room */
    for (int i = 0; i < 2; i++)
        place_item_in_room(&f->rooms[ir], random_pool_item());

    /* Stairs tile (hidden until boss cleared) */
    Room *boss = &f->rooms[br];
    boss->tiles[boss->h/2 + 1][boss->w/2 + 1] = T_FLOOR;

    /* Assign minimap grid coordinates via BFS */
    compute_map_layout(f);
}

/* ─── Map layout (BFS assigns grid coords to every room) ───────────── */
static void compute_map_layout(Floor *f) {
    bool placed[MAX_ROOMS] = {false};
    int  queue[MAX_ROOMS];
    int  head = 0, tail = 0;

    queue[tail++] = f->start_room;
    placed[f->start_room]       = true;
    f->rooms[f->start_room].map_x = 0;
    f->rooms[f->start_room].map_y = 0;

    while (head < tail) {
        int cur  = queue[head++];
        Room *r  = &f->rooms[cur];
        for (int d = 0; d < 4; d++) {
            int nb = r->conn[d];
            if (nb < 0 || placed[nb]) continue;
            placed[nb] = true;
            f->rooms[nb].map_x = r->map_x + DX[d];
            f->rooms[nb].map_y = r->map_y + DY[d];
            queue[tail++] = nb;
        }
    }
}

/* ─── Accessors ─────────────────────────────────────────────────────── */
Room *cur_room(void) {
    return &G.cf->rooms[G.player.current_room];
}

/* ─── Door state ────────────────────────────────────────────────────── */
void lock_doors(Room *r) {
    for (int d = 0; d < 4; d++) {
        if (r->conn[d] < 0) continue;
        int dx = r->door_x[d], dy = r->door_y[d];
        if (r->tiles[dy][dx] == T_DOOR_OPEN)
            r->tiles[dy][dx] = T_DOOR_LOCK;
    }
}

void unlock_doors(Room *r) {
    for (int d = 0; d < 4; d++) {
        if (r->conn[d] < 0) continue;
        int dx = r->door_x[d], dy = r->door_y[d];
        if (r->tiles[dy][dx] == T_DOOR_LOCK)
            r->tiles[dy][dx] = T_DOOR_OPEN;
    }
}

/* ─── Room entry ────────────────────────────────────────────────────── */
void enter_room(int idx, int from_dir) {
    if (idx < 0 || idx >= G.cf->n_rooms) return;
    Room *r = &G.cf->rooms[idx];
    G.player.current_room = idx;

    if (!r->visited) {
        r->visited = true;
        if (r->is_alcove) {
            /* Atmospheric dead-end flavour */
            static const char *alcove_msgs[] = {
                "A dead end. Something was left here long ago.",
                "The passage ends. Cold air breathes from the stone.",
                "Nothing here but silence and old dust.",
                "A pocket in the wall. It smells of ash.",
                "The corridor stops. Scratches mark the floor — someone waited here.",
            };
            log_msg("%s", alcove_msgs[rand() % 5]);
        } else if (r->is_corridor) {
            /* no message for plain corridors or junctions */
        } else if (r->boss_room) {
            log_msg("A chill falls. Something terrible waits here.");
        } else if (r->item_room) {
            log_msg("The room is quiet. Gifts lie on the floor.");
        }
    }

    /* Spawn position — just inside the entry door */
    if (from_dir >= 0 && from_dir < 4) {
        Dir entry = OPP[(Dir)from_dir];
        int ex = r->door_x[entry];
        int ey = r->door_y[entry];
        G.player.x = ex - DX[entry];
        G.player.y = ey - DY[entry];
        if (G.player.x < 1)    G.player.x = 1;
        if (G.player.x > r->w) G.player.x = r->w;
        if (G.player.y < 1)    G.player.y = 1;
        if (G.player.y > r->h) G.player.y = r->h;
    } else {
        G.player.x = r->w/2 + 1;
        G.player.y = r->h/2 + 1;
    }

    /* Corridors, junctions, and alcoves are always open */
    if (r->is_corridor) return;

    bool has_alive = false;
    for (int i = 0; i < r->n_enemies; i++)
        if (r->enemies[i].alive) { has_alive = true; break; }

    if (has_alive && !r->cleared) {
        lock_doors(r);
    } else {
        r->cleared = true;
        unlock_doors(r);
        if (r->boss_room && r->has_stairs)
            r->tiles[r->h/2 + 1][r->w/2 + 1] = T_STAIRS;
    }
}
