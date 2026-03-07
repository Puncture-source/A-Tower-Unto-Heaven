#include "game.h"

/* ─── Internal helpers ──────────────────────────────────────────────── */
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

static void connect_rooms(Floor *f, int a, int b, Dir dir) {
    Room *ra = &f->rooms[a];
    Room *rb = &f->rooms[b];
    Dir opp = OPP[dir];

    ra->conn[dir] = b;
    rb->conn[opp] = a;

    /* Door in room A */
    int ax, ay;
    switch (dir) {
        case D_N: ax = (ra->w+2)/2; ay = 0;        break;
        case D_S: ax = (ra->w+2)/2; ay = ra->h+1;  break;
        case D_E: ax = ra->w+1;     ay = (ra->h+2)/2; break;
        default:  ax = 0;           ay = (ra->h+2)/2; break;
    }
    ra->tiles[ay][ax] = T_DOOR_OPEN;
    ra->door_x[dir] = ax;
    ra->door_y[dir] = ay;

    /* Door in room B */
    int bx, by;
    switch (opp) {
        case D_N: bx = (rb->w+2)/2; by = 0;        break;
        case D_S: bx = (rb->w+2)/2; by = rb->h+1;  break;
        case D_E: bx = rb->w+1;     by = (rb->h+2)/2; break;
        default:  bx = 0;           by = (rb->h+2)/2; break;
    }
    rb->tiles[by][bx] = T_DOOR_OPEN;
    rb->door_x[opp] = bx;
    rb->door_y[opp] = by;
}

static int rw(void) { return 14 + rand() % 15; } /* 14-28 */
static int rh(void) { return  8 + rand() %  7; } /* 8-14 */

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

/* ─── Floor generation ──────────────────────────────────────────────── */
void generate_floor(Floor *f, int floor_num) {
    memset(f, 0, sizeof(Floor));
    f->floor_num = floor_num;

    int n_combat = 3 + floor_num;   /* 3, 4, 5 */

    /* Room 0: start (always cleared) */
    make_room(&f->rooms[0], rw(), rh());
    f->rooms[0].cleared = true;
    f->rooms[0].visited = true;

    /* Combat rooms 1..n_combat */
    for (int i = 1; i <= n_combat; i++) {
        make_room(&f->rooms[i], rw(), rh());
        connect_rooms(f, i-1, i, D_E);
    }

    /* Item room: branches south off room 1 */
    int ir = n_combat + 1;
    make_room(&f->rooms[ir], rw(), rh());
    f->rooms[ir].item_room = true;
    f->rooms[ir].cleared   = true;   /* no enemies */
    connect_rooms(f, 1, ir, D_S);

    /* Boss room: end of chain */
    int br = n_combat + 2;
    make_room(&f->rooms[br], 26 + rand()%4, 12 + rand()%3);
    f->rooms[br].boss_room  = true;
    f->rooms[br].has_stairs = true;
    connect_rooms(f, n_combat, br, D_E);

    f->n_rooms    = br + 1;
    f->start_room = 0;
    f->boss_room  = br;

    /* Spawn enemies in combat rooms */
    for (int i = 1; i <= n_combat; i++)
        spawn_room_enemies(&f->rooms[i], floor_num);

    /* Boss */
    spawn_boss(&f->rooms[br], floor_num);

    /* Items in item room (2 random drops) */
    for (int i = 0; i < 2; i++)
        place_item_in_room(&f->rooms[ir], random_pool_item());

    /* Stairs tile hidden until boss cleared */
    Room *boss = &f->rooms[br];
    boss->tiles[boss->h/2 + 1][boss->w/2 + 1] = T_FLOOR; /* will become stairs */
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
        if (r->boss_room)
            log_msg("A chill falls. Something terrible waits here.");
        else if (r->item_room)
            log_msg("The room is quiet. Gifts lie on the floor.");
    }

    /* Position player just inside the entry door */
    if (from_dir >= 0 && from_dir < 4) {
        Dir entry = OPP[(Dir)from_dir];
        int ex = r->door_x[entry];
        int ey = r->door_y[entry];
        /* step one tile inward */
        G.player.x = ex - DX[entry];
        G.player.y = ey - DY[entry];
        /* clamp to interior */
        if (G.player.x < 1)     G.player.x = 1;
        if (G.player.x > r->w)  G.player.x = r->w;
        if (G.player.y < 1)     G.player.y = 1;
        if (G.player.y > r->h)  G.player.y = r->h;
    } else {
        /* start of floor: center of room */
        G.player.x = r->w/2 + 1;
        G.player.y = r->h/2 + 1;
    }

    /* Lock doors if enemies present */
    bool has_alive = false;
    for (int i = 0; i < r->n_enemies; i++)
        if (r->enemies[i].alive) { has_alive = true; break; }

    if (has_alive && !r->cleared)
        lock_doors(r);
    else {
        r->cleared = true;
        unlock_doors(r);
        /* Place stairs if boss room cleared */
        if (r->boss_room && r->has_stairs) {
            r->tiles[r->h/2 + 1][r->w/2 + 1] = T_STAIRS;
        }
    }
}
