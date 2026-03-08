#include "game.h"

/* ─── Room builders ─────────────────────────────────────────────────── */

static void make_room(Room *r, int w, int h) {
    memset(r, 0, sizeof(Room));
    if (w > MAX_ROOM_W) w = MAX_ROOM_W;
    if (h > MAX_ROOM_H) h = MAX_ROOM_H;
    if (w < 4) w = 4;
    if (h < 3) h = 3;
    r->w = w; r->h = h;
    for (int d = 0; d < 4; d++) r->conn[d] = -1;
    for (int y = 0; y < h+2; y++)
        for (int x = 0; x < w+2; x++)
            r->tiles[y][x] = T_WALL;
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            r->tiles[y][x] = T_FLOOR;
}

/* Random room dimensions — wider spread than before */
static int rw(void) { return  8 + rand() % 18; }
static int rh(void) { return  5 + rand() %  8; }

/* Add organic interior features after room is built and doors are cut.
   Only touches interior tiles, never the wall border. */
static void embellish_room(Room *r) {
    int w = r->w, h = r->h;
    if (w < 6 || h < 4) return; /* too small to embellish */

    int choice = rand() % 5;
    switch (choice) {

    case 0: /* L-shaped: carve a corner quadrant */
    {
        /* Pick a corner quadrant (avoid door midpoints by only carving
           corners where no door sits — safe since doors are at midpoints) */
        int cx = (rand()%2) ? 1 : w/2+1;   /* left or right half start */
        int cy = (rand()%2) ? 1 : h/2+1;   /* top or bottom half start */
        int cw = w/3 + 1;
        int ch = h/3 + 1;
        for (int y = cy; y < cy+ch && y <= h; y++)
            for (int x = cx; x < cx+cw && x <= w; x++)
                r->tiles[y][x] = T_WALL;
        break;
    }

    case 1: /* Pillar pairs near corners */
    {
        /* Four pillars at quarter-points */
        int px[4] = { w/4+1, 3*w/4+1, w/4+1, 3*w/4+1 };
        int py[4] = { h/3+1, h/3+1, 2*h/3+1, 2*h/3+1 };
        for (int i = 0; i < 4; i++) {
            if (r->tiles[py[i]][px[i]] == T_FLOOR)
                r->tiles[py[i]][px[i]] = T_WALL;
        }
        break;
    }

    case 2: /* Partial interior wall — horizontal or vertical spine */
    {
        if (rand()%2 && w >= 8) {
            /* Vertical spine off the north or south wall, with a gap */
            int sx = w/2 + 1;
            int gap_y = h/3 + rand()%(h/3+1);
            for (int y = 1; y <= h; y++) {
                if (y == gap_y || y == gap_y+1) continue; /* gap */
                if (r->tiles[y][sx] == T_FLOOR)
                    r->tiles[y][sx] = T_WALL;
            }
        } else if (h >= 6) {
            /* Horizontal spine, with a gap */
            int sy = h/2 + 1;
            int gap_x = w/3 + rand()%(w/3+1);
            for (int x = 1; x <= w; x++) {
                if (x == gap_x || x == gap_x+1) continue;
                if (r->tiles[sy][x] == T_FLOOR)
                    r->tiles[sy][x] = T_WALL;
            }
        }
        break;
    }

    case 3: /* Notched corners — shave wall tiles from all four corners */
    {
        int notch = 1 + rand()%2;
        for (int i = 0; i < notch; i++) {
            for (int j = 0; j <= i; j++) {
                /* top-left */
                r->tiles[1+i][1+j] = T_WALL;
                /* top-right */
                r->tiles[1+i][w-j] = T_WALL;
                /* bottom-left */
                r->tiles[h-i][1+j] = T_WALL;
                /* bottom-right */
                r->tiles[h-i][w-j] = T_WALL;
            }
        }
        break;
    }

    case 4: /* Plain — no change */
    default:
        break;
    }
}

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

/* ─── Corridor / bend primitives ────────────────────────────────────── */

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

/* 1-tile-wide straight corridor; returns index. */
static int make_corr(Floor *f, Dir dir, int len) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    if (len < 2) len = 2;
    int idx = f->n_rooms++;
    Room *c = &f->rooms[idx];
    memset(c, 0, sizeof(Room));
    for (int d = 0; d < 4; d++) c->conn[d] = -1;
    c->cleared = c->is_corridor = true;
    /* 1-tile wide in the perpendicular axis */
    int w = (dir == D_E || dir == D_W) ? len : 1;
    int h = (dir == D_N || dir == D_S) ? len : 1;
    c->w = w; c->h = h;
    for (int y = 0; y < h+2; y++)
        for (int x = 0; x < w+2; x++)
            c->tiles[y][x] = T_WALL;
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            c->tiles[y][x] = T_FLOOR;
    return idx;
}

/* 1×1 bend room — a single-tile turning point. Returns index. */
static int make_bend(Floor *f) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    int idx = f->n_rooms++;
    Room *b = &f->rooms[idx];
    memset(b, 0, sizeof(Room));
    for (int d = 0; d < 4; d++) b->conn[d] = -1;
    b->cleared = b->is_corridor = true;
    b->w = 1; b->h = 1;
    /* 3×3 tile buffer: wall border, one floor tile in centre */
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            b->tiles[y][x] = T_WALL;
    b->tiles[1][1] = T_FLOOR;
    return idx;
}

/* Small dead-end alcove (optionally with an item). */
static int make_alcove(Floor *f, bool give_item) {
    if (f->n_rooms >= MAX_ROOMS) return -1;
    int idx = f->n_rooms++;
    Room *d = &f->rooms[idx];
    int w = 4 + rand()%5, h = 3 + rand()%4;
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

static Dir perp_to(Dir main) {
    if (main == D_E || main == D_W) return rand()%2 ? D_N : D_S;
    else                            return rand()%2 ? D_E : D_W;
}

/* ─── Winding connection ─────────────────────────────────────────────── *
 *
 * Connects room `a` to room `b` in the primary direction `main`.
 * Uses 1-tile-wide corridors and 1×1 bend rooms to produce organic,
 * winding paths. Four patterns:
 *
 *  STRAIGHT   a -[corr]- b
 *
 *  L-BEND     a -[c1]- [bend] -[c2]- b
 *               main     turn   perp         (turns 90°)
 *
 *  Z-BEND     a -[c1]-[bend1]-[c2]-[bend2]-[c3]- b
 *                              (snakes around)
 *
 *  BRANCH     L-BEND with a short dead-end off the bend point
 *
 * If `side` >= 0, the connection sprouts a corridor leading to that room.
 */
static void build_connection(Floor *f, int a, int b, Dir main, int side) {
    Dir p = perp_to(main);
    int type;
    if (side < 0) type = rand() % 3;       /* 0=straight, 1=L, 2=Z */
    else          type = 1 + rand() % 2;   /* 1=L, 2=Z — branch off bend */

    switch (type) {

    case 0: /* STRAIGHT */
    {
        int len = 3 + rand() % 6;
        int ci = make_corr(f, main, len);
        if (ci < 0) break;
        rlink(f, a, main, ci);
        rlink(f, ci, main, b);
        break;
    }

    case 1: /* L-BEND: a -[c1]- bend -[c2]- b */
    {
        int l1 = 3 + rand() % 5;
        int l2 = 3 + rand() % 5;
        int c1 = make_corr(f, main, l1);
        int bd = make_bend(f);
        int c2 = make_corr(f, p, l2);
        if (c1 < 0 || bd < 0 || c2 < 0) break;
        rlink(f, a,  main, c1);
        rlink(f, c1, main, bd);
        rlink(f, bd, p,    c2);
        rlink(f, c2, p,    b);
        if (side >= 0) {
            /* branch off the bend in the remaining perpendicular direction */
            int l3 = 2 + rand() % 4;
            int cs = make_corr(f, OPP[p], l3);
            if (cs >= 0) {
                rlink(f, bd, OPP[p], cs);
                rlink(f, cs, OPP[p], side);
            }
        }
        break;
    }

    case 2: /* Z-BEND: a -[c1]- b1 -[c2]- b2 -[c3]- b */
    {
        int l1 = 2 + rand() % 4;
        int l2 = 2 + rand() % 5;
        int l3 = 2 + rand() % 4;
        int c1 = make_corr(f, main, l1);
        int b1 = make_bend(f);
        int c2 = make_corr(f, p,    l2);
        int b2 = make_bend(f);
        int c3 = make_corr(f, main, l3);
        if (c1<0||b1<0||c2<0||b2<0||c3<0) break;
        rlink(f, a,  main, c1);
        rlink(f, c1, main, b1);
        rlink(f, b1, p,    c2);
        rlink(f, c2, p,    b2);
        rlink(f, b2, main, c3);
        rlink(f, c3, main, b);
        if (side >= 0) {
            /* branch off first bend toward side */
            int ls = 2 + rand() % 4;
            int cs = make_corr(f, OPP[p], ls);
            if (cs >= 0) {
                rlink(f, b1, OPP[p], cs);
                rlink(f, cs, OPP[p], side);
            }
        }
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
    int ir = n_combat + 1;
    int br = n_combat + 2;

    f->n_rooms = br + 1;

    /* Start room */
    make_room(&f->rooms[0], rw(), rh());
    f->rooms[0].cleared = f->rooms[0].visited = true;
    embellish_room(&f->rooms[0]);

    /* Combat rooms */
    for (int i = 1; i <= n_combat; i++) {
        make_room(&f->rooms[i], rw(), rh());
        embellish_room(&f->rooms[i]);
    }

    /* Item room — slightly smaller, untouched */
    make_room(&f->rooms[ir], 7 + rand()%8, 5 + rand()%4);
    f->rooms[ir].item_room = f->rooms[ir].cleared = true;

    /* Boss room — large, no embellishment (clean arena) */
    make_room(&f->rooms[br], 24 + rand()%6, 11 + rand()%3);
    f->rooms[br].boss_room = f->rooms[br].has_stairs = true;

    /* Zigzag spine directions: even segments go east, odd go north/south.
       This winds the path so the boss ends up surrounded on multiple sides. */
    Dir spine[6];
    Dir perp = (rand()%2) ? D_N : D_S;
    for (int i = 0; i < n_combat; i++) {
        spine[i] = (i % 2 == 0) ? D_E : perp;
        if (i % 2 == 1) perp = OPP[perp]; /* alternate N<->S each bend */
    }

    int item_seg = rand() % n_combat;
    for (int i = 0; i < n_combat; i++) {
        int side = -1;
        if (i == item_seg) {
            side = ir;
        } else if (rand() % 3 == 0) {
            side = make_alcove(f, rand()%3 == 0);
        }
        build_connection(f, i, i+1, spine[i], side);
    }

    /* Boss connection — always approached from the west for a clear arena entry */
    {
        int side = (rand()%2) ? make_alcove(f, false) : -1;
        build_connection(f, n_combat, br, D_E, side);
    }

    f->start_room = 0;
    f->boss_room  = br;

    /* Enemies */
    for (int i = 1; i <= n_combat; i++)
        spawn_room_enemies(&f->rooms[i], floor_num);

    /* Boss */
    spawn_boss(&f->rooms[br], floor_num);

    /* Items */
    for (int i = 0; i < 2; i++)
        place_item_in_room(&f->rooms[ir], random_pool_item());

    /* Stairs tile (hidden until boss cleared) */
    Room *boss = &f->rooms[br];
    boss->tiles[boss->h/2 + 1][boss->w/2 + 1] = T_FLOOR;

    compute_map_layout(f);
}

/* ─── Map layout (BFS assigns grid coords + world tile offsets) ─────── */
static void compute_map_layout(Floor *f) {
    bool placed[MAX_ROOMS] = {false};
    int  queue[MAX_ROOMS];
    int  head = 0, tail = 0;

    f->rooms[f->start_room].map_x = 0;
    f->rooms[f->start_room].map_y = 0;
    f->rooms[f->start_room].wx    = 0;
    f->rooms[f->start_room].wy    = 0;
    queue[tail++] = f->start_room;
    placed[f->start_room] = true;

    while (head < tail) {
        int cur = queue[head++];
        Room *r = &f->rooms[cur];
        for (int d = 0; d < 4; d++) {
            int nb = r->conn[d];
            if (nb < 0 || placed[nb]) continue;
            placed[nb] = true;
            Room *n = &f->rooms[nb];
            n->map_x = r->map_x + DX[d];
            n->map_y = r->map_y + DY[d];
            /* Door-aligned world offset: place n so its entry door is
               adjacent to r's exit door in direction d. */
            n->wx = r->wx + r->door_x[d] + DX[d] - n->door_x[OPP[d]];
            n->wy = r->wy + r->door_y[d] + DY[d] - n->door_y[OPP[d]];
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
            static const char *alcove_msgs[] = {
                "A dead end. Something was left here long ago.",
                "The passage ends. Cold air breathes from the stone.",
                "Nothing here but silence and old dust.",
                "A pocket in the wall. It smells of ash.",
                "The corridor stops. Scratches mark the floor — someone waited here.",
            };
            log_msg("%s", alcove_msgs[rand() % 5]);
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
