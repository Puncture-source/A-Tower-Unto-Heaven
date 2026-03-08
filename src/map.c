#include "game.h"

/*
 * The entire floor is ONE large room whose tile grid is a perfect maze.
 * Iterative DFS (recursive-backtracker) carves passages at odd interior
 * coordinates.  A boss chamber is carved at the bottom-right corner.
 * Player starts at (1,1); the boss waits in the chamber.
 *
 * Maze layout (interior coords 1..MAX_ROOM_W, 1..MAX_ROOM_H):
 *   Cells    : odd  x and y  →  (1,1),(3,1),...,(2*MW-1, 2*MH-1)
 *   Passages : one tile between adjacent carved cells
 *   Boss chamber: rectangle [BC_X..r->w][BC_Y..r->h]
 */

#define MW   25   /* maze cells wide  — cell tiles at x=1,3,...,49 */
#define MH   11   /* maze cells tall  — cell tiles at y=1,3,...,21  */
#define BC_X (2*MW)       /* boss chamber left edge  = 50 */
#define BC_Y (2*MH - 2)   /* boss chamber top edge   = 20 */

/* ─── Maze carving ───────────────────────────────────────────────────── */
static bool mz_vis[MH + 2][MW + 2];

static void carve_maze(TileType tiles[RT_H][RT_W]) {
    memset(mz_vis, 0, sizeof(mz_vis));

    /* Iterative DFS — avoid deep recursion on large mazes */
    int sp = 0;
    int sx[MW * MH + 4], sy_stk[MW * MH + 4];
    sx[sp] = 1; sy_stk[sp] = 1; sp++;
    mz_vis[1][1] = true;
    tiles[1][1]  = T_FLOOR;   /* start cell */

    while (sp > 0) {
        int cx = sx[sp - 1], cy = sy_stk[sp - 1];

        /* Shuffle directions */
        Dir dirs[4] = { D_N, D_S, D_E, D_W };
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i + 1);
            Dir t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
        }

        bool moved = false;
        for (int i = 0; i < 4; i++) {
            Dir d  = dirs[i];
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 1 || nx > MW || ny < 1 || ny > MH) continue;
            if (mz_vis[ny][nx]) continue;

            /* Carve the wall between (cx,cy) and (nx,ny) */
            tiles[2*cy - 1 + DY[d]][2*cx - 1 + DX[d]] = T_FLOOR;
            /* Carve the neighbour cell */
            tiles[2*ny - 1][2*nx - 1] = T_FLOOR;

            mz_vis[ny][nx] = true;
            sx[sp] = nx; sy_stk[sp] = ny; sp++;
            moved = true;
            break;
        }
        if (!moved) sp--;   /* backtrack */
    }
}

/* ─── Item placement ─────────────────────────────────────────────────── */
static void place_maze_item(Room *r, ItemType it) {
    if (r->n_items >= MAX_ROOM_IT) return;
    for (int tries = 0; tries < 500; tries++) {
        int x = 1 + rand() % r->w;
        int y = 1 + rand() % r->h;
        if (r->tiles[y][x] != T_FLOOR) continue;
        if (x >= BC_X - 3 && y >= BC_Y - 3) continue;  /* not near boss chamber */
        if (x <= 3 && y <= 3) continue;                 /* not near start */
        bool occ = false;
        for (int i = 0; i < r->n_items; i++)
            if (r->ix[i] == x && r->iy[i] == y) { occ = true; break; }
        if (occ) continue;
        r->tiles[y][x]    = T_ITEM;
        r->items[r->n_items] = it;
        r->ix[r->n_items]    = x;
        r->iy[r->n_items]    = y;
        r->n_items++;
        return;
    }
}

/* ─── Floor generation ───────────────────────────────────────────────── */
static void compute_map_layout(Floor *f);

void generate_floor(Floor *f, int floor_num) {
    memset(f, 0, sizeof(*f));
    f->floor_num = floor_num;

    /* Single room = the whole maze */
    f->n_rooms = 1;
    Room *r = &f->rooms[0];
    memset(r, 0, sizeof(*r));
    for (int d = 0; d < 4; d++) r->conn[d] = -1;
    r->w          = MAX_ROOM_W;
    r->h          = MAX_ROOM_H;
    r->boss_room  = true;
    r->has_stairs = true;

    /* Fill everything with walls */
    for (int y = 0; y < RT_H; y++)
        for (int x = 0; x < RT_W; x++)
            r->tiles[y][x] = T_WALL;

    /* Carve perfect maze into top-left region */
    carve_maze(r->tiles);

    /* Carve boss chamber in bottom-right corner */
    for (int y = BC_Y; y <= r->h; y++)
        for (int x = BC_X; x <= r->w; x++)
            r->tiles[y][x] = T_FLOOR;

    /* Stairs reveal position: centre of boss chamber */
    r->stairs_x = (BC_X + r->w) / 2;   /* 54 */
    r->stairs_y = (BC_Y + r->h) / 2;   /* 23 */

    f->start_room = 0;
    f->boss_room  = 0;

    /* ── Boss: spawn then reposition to chamber centre ── */
    spawn_boss(r, floor_num);
    if (r->n_enemies > 0) {
        r->enemies[0].x = r->stairs_x;
        r->enemies[0].y = r->stairs_y - 1;
    }

    /* ── Regular enemies scattered through maze corridors ── */
    spawn_room_enemies(r, floor_num);
    spawn_room_enemies(r, floor_num);

    /* Post-process: move enemies that landed near start or in boss chamber */
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive || e->boss) continue;
        bool bad = (e->x <= 3 && e->y <= 3) ||
                   (e->x >= BC_X - 3 && e->y >= BC_Y - 3);
        if (!bad) continue;
        for (int tries = 0; tries < 500; tries++) {
            int nx = 1 + rand() % r->w;
            int ny = 1 + rand() % r->h;
            if (r->tiles[ny][nx] != T_FLOOR) continue;
            if (nx >= BC_X - 3 && ny >= BC_Y - 3) continue;
            if (nx <= 3 && ny <= 3) continue;
            e->x = nx; e->y = ny;
            break;
        }
    }

    /* ── Items scattered in maze corridors ── */
    int n_items = 2 + floor_num;
    for (int i = 0; i < n_items; i++)
        place_maze_item(r, random_pool_item());

    compute_map_layout(f);
}

/* ─── Map layout ─────────────────────────────────────────────────────── */
static void compute_map_layout(Floor *f) {
    Room *r    = &f->rooms[f->start_room];
    r->map_x   = r->map_y = 0;
    r->wx      = r->wy = 0;
}

/* ─── Accessors ──────────────────────────────────────────────────────── */
Room *cur_room(void) {
    return &G.cf->rooms[G.player.current_room];
}

int room_at_world(int wx, int wy, int *out_tx, int *out_ty) {
    Floor *fl = G.cf;
    for (int ri = 0; ri < fl->n_rooms; ri++) {
        Room *r  = &fl->rooms[ri];
        int   tx = wx - r->wx;
        int   ty = wy - r->wy;
        if (tx < 0 || ty < 0 || tx >= r->w + 2 || ty >= r->h + 2) continue;
        TileType t = r->tiles[ty][tx];
        if (t == T_VOID) continue;
        *out_tx = tx;
        *out_ty = ty;
        return ri;
    }
    return -1;
}

/* ─── Door state (no-ops for maze — rooms have no connections) ─────── */
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

/* ─── Room entry ─────────────────────────────────────────────────────── */
void enter_room(int idx, int from_dir) {
    if (idx < 0 || idx >= G.cf->n_rooms) return;
    Room *r = &G.cf->rooms[idx];
    G.player.current_room = idx;

    /* Player always starts at maze entrance (top-left) */
    G.player.x = 1;
    G.player.y = 1;
    (void)from_dir;

    if (!r->visited) {
        r->visited = true;
        log_msg("The maze stretches before you.");
        log_msg("Find the boss at its heart. Kill it to ascend.");
    }

    /* Reveal stairs if boss already dead (floor revisit) */
    if (r->cleared && r->boss_room && r->has_stairs)
        r->tiles[r->stairs_y][r->stairs_x] = T_STAIRS;
}
