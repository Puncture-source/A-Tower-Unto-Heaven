#include "game.h"

/*
 * Maze floor — single large Room whose tile grid is a perfect maze.
 *
 * Cell pitch CP=3: each maze cell occupies a CW×CW (2×2) tile block;
 * walls between adjacent cells are 1 tile thick.  Carved passages are
 * therefore CW=2 tiles wide.  After generation a widening pass randomly
 * opens thin walls to produce occasional 3-wide sections.
 *
 * Interior coords (1..MAX_ROOM_W, 1..MAX_ROOM_H):
 *   Cell (cx,cy) top-left tile: ( CP*(cx-1)+1 , CP*(cy-1)+1 )
 *   Boss chamber: [ BC_X .. MAX_ROOM_W ] × [ BC_Y .. MAX_ROOM_H ]
 */

#define CW   2          /* corridor/cell width in tiles          */
#define CP   3          /* cell pitch = CW(2) + wall(1)          */
#define MW   16         /* maze cells wide  (tiles 1..47)        */
#define MH    7         /* maze cells tall  (tiles 1..20)        */
#define BC_X (CP * MW)          /* boss chamber left  = 48       */
#define BC_Y (CP * (MH - 1))    /* boss chamber top   = 18       */

static bool mz_vis[MH + 2][MW + 2];

/* ─── Maze carving (iterative DFS) ──────────────────────────────────── */
static void carve_maze(TileType tiles[RT_H][RT_W]) {
    memset(mz_vis, 0, sizeof(mz_vis));

    int sp = 0;
    int sx[MW * MH + 4], sy_stk[MW * MH + 4];
    sx[sp] = 1; sy_stk[sp] = 1; sp++;
    mz_vis[1][1] = true;

    /* Carve starting cell (cx=1,cy=1) → top-left tile (1,1) */
    for (int dy = 0; dy < CW; dy++)
        for (int dx = 0; dx < CW; dx++)
            tiles[1 + dy][1 + dx] = T_FLOOR;

    while (sp > 0) {
        int cx = sx[sp-1], cy = sy_stk[sp-1];
        int tx = CP*(cx-1)+1;   /* cell top-left tile x */
        int ty = CP*(cy-1)+1;   /* cell top-left tile y */

        /* Shuffle directions */
        Dir dirs[4] = { D_N, D_S, D_E, D_W };
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i+1);
            Dir t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
        }

        bool moved = false;
        for (int i = 0; i < 4; i++) {
            Dir d  = dirs[i];
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 1 || nx > MW || ny < 1 || ny > MH) continue;
            if (mz_vis[ny][nx]) continue;

            int ntx = CP*(nx-1)+1;
            int nty = CP*(ny-1)+1;

            /* Carve the 1-tile wall between the two cells */
            if (d == D_E) {
                for (int wi = 0; wi < CW; wi++)
                    tiles[ty+wi][tx+CW] = T_FLOOR;
            } else if (d == D_W) {
                for (int wi = 0; wi < CW; wi++)
                    tiles[ty+wi][tx-1] = T_FLOOR;
            } else if (d == D_S) {
                for (int wi = 0; wi < CW; wi++)
                    tiles[ty+CW][tx+wi] = T_FLOOR;
            } else { /* D_N */
                for (int wi = 0; wi < CW; wi++)
                    tiles[ty-1][tx+wi] = T_FLOOR;
            }

            /* Carve neighbour cell */
            for (int dy2 = 0; dy2 < CW; dy2++)
                for (int dx2 = 0; dx2 < CW; dx2++)
                    tiles[nty+dy2][ntx+dx2] = T_FLOOR;

            mz_vis[ny][nx] = true;
            sx[sp] = nx; sy_stk[sp] = ny; sp++;
            moved = true;
            break;
        }
        if (!moved) sp--;
    }
}

/* ─── Widening pass ──────────────────────────────────────────────────── *
 * Randomly open thin walls (1 tile) that sit between two floor tiles.   *
 * This converts some corridors from 2-wide to 3-wide for visual variety. */
static void widen_maze(TileType tiles[RT_H][RT_W]) {
    /* Only operate inside the maze area, not the boss chamber */
    int mx = BC_X;          /* x limit */
    int my = CP * MH + 1;   /* y limit (includes last wall row) */
    for (int y = 2; y < my && y <= MAX_ROOM_H; y++) {
        for (int x = 2; x < mx && x <= MAX_ROOM_W; x++) {
            if (tiles[y][x] != T_WALL) continue;
            bool horiz = (tiles[y][x-1] == T_FLOOR && tiles[y][x+1] == T_FLOOR);
            bool vert  = (tiles[y-1][x] == T_FLOOR && tiles[y+1][x] == T_FLOOR);
            if ((horiz || vert) && rand() % 3 == 0)
                tiles[y][x] = T_FLOOR;
        }
    }
}

/* ─── Item placement ─────────────────────────────────────────────────── */
static void place_maze_item(Room *r, ItemType it) {
    if (r->n_items >= MAX_ROOM_IT) return;
    for (int tries = 0; tries < 500; tries++) {
        int x = 1 + rand() % r->w;
        int y = 1 + rand() % r->h;
        if (r->tiles[y][x] != T_FLOOR) continue;
        if (x >= BC_X - 3 && y >= BC_Y - 3) continue;
        if (x <= CW+1 && y <= CW+1) continue;
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

    /* Carve maze then vary corridor widths */
    carve_maze(r->tiles);
    widen_maze(r->tiles);

    /* Boss chamber: open rectangle in bottom-right */
    for (int y = BC_Y; y <= r->h; y++)
        for (int x = BC_X; x <= r->w; x++)
            r->tiles[y][x] = T_FLOOR;

    /* Stairs hidden in chamber centre until boss dies */
    r->stairs_x = (BC_X + r->w) / 2;
    r->stairs_y = (BC_Y + r->h) / 2;

    f->start_room = 0;
    f->boss_room  = 0;

    /* Boss: spawn then place at chamber centre */
    spawn_boss(r, floor_num);
    if (r->n_enemies > 0) {
        r->enemies[0].x = r->stairs_x;
        r->enemies[0].y = r->stairs_y - 1;
    }

    /* Regular enemies scattered through maze corridors */
    spawn_room_enemies(r, floor_num);
    spawn_room_enemies(r, floor_num);

    /* Move any enemy that landed near start or inside boss chamber */
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive || e->boss) continue;
        bool bad = (e->x <= CW+1 && e->y <= CW+1) ||
                   (e->x >= BC_X - 3 && e->y >= BC_Y - 3);
        if (!bad) continue;
        for (int tries = 0; tries < 500; tries++) {
            int nx = 1 + rand() % r->w;
            int ny = 1 + rand() % r->h;
            if (r->tiles[ny][nx] != T_FLOOR) continue;
            if (nx >= BC_X - 3 && ny >= BC_Y - 3) continue;
            if (nx <= CW+1 && ny <= CW+1) continue;
            e->x = nx; e->y = ny;
            break;
        }
    }

    /* Items in maze corridors */
    for (int i = 0; i < 2 + floor_num; i++)
        place_maze_item(r, random_pool_item());

    compute_map_layout(f);
}

/* ─── Map layout ─────────────────────────────────────────────────────── */
static void compute_map_layout(Floor *f) {
    Room *r  = &f->rooms[f->start_room];
    r->map_x = r->map_y = 0;
    r->wx    = r->wy = 0;
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

/* ─── Door state (no-ops for maze — no connections) ─────────────────── */
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

    /* Always start at maze entrance */
    G.player.x = 1;
    G.player.y = 1;
    (void)from_dir;

    if (!r->visited) {
        r->visited = true;
        log_msg("The maze stretches before you.");
        log_msg("Find the boss at its heart. Kill it to ascend.");
    }

    if (r->cleared && r->boss_room && r->has_stairs)
        r->tiles[r->stairs_y][r->stairs_x] = T_STAIRS;
}
