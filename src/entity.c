#include "game.h"

/* ─── Utility ────────────────────────────────────────────────────────── */
int roll(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + rand() % (hi - lo + 1);
}

void log_msg(const char *fmt, ...) {
    MsgLog *ml = &G.log;
    char buf[MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MSG_LEN, fmt, ap);
    va_end(ap);
    if (ml->count < MAX_MSG) {
        strncpy(ml->lines[ml->count++], buf, MSG_LEN-1);
    } else {
        /* shift log */
        memmove(ml->lines[0], ml->lines[1], MSG_LEN*(MAX_MSG-1));
        strncpy(ml->lines[MAX_MSG-1], buf, MSG_LEN-1);
    }
}

/* ─── Player init ────────────────────────────────────────────────────── */
void init_player(CharType ct) {
    Player *p = &G.player;
    memset(p, 0, sizeof(Player));
    p->max_items = MAX_PLR_IT; /* temporary; recalc_stats() will set final value */
    const CharDef *cd = &CHARS[ct];
    p->chr       = ct;
    p->hp        = cd->hp;
    p->max_hp    = cd->hp;
    p->base_atk  = cd->atk;
    p->base_def  = cd->def;
    p->atk       = cd->atk;
    p->def       = cd->def;
    p->floor_num = 0;
    p->current_room = 0;

    /* Starting items */
    for (int i = 0; i < cd->n_start; i++) {
        ItemType it = cd->start[i];
        p->items[i]   = it;
        p->charges[i] = ITEMS[it].charges;
        /* Apply max_hp and hp_heal immediately */
        if (ITEMS[it].max_hp > 0) {
            p->max_hp += ITEMS[it].max_hp;
            p->hp     += ITEMS[it].max_hp;
        }
        if (ITEMS[it].hp_heal > 0) {
            p->hp = p->hp + ITEMS[it].hp_heal;
            if (p->hp > p->max_hp) p->hp = p->max_hp;
        }
        if (ITEMS[it].consumable && it == IT_GRENADES)
            p->grenades += ITEMS[it].charges;
    }
    p->n_items = cd->n_start;

    /* Handle package special case */
    for (int i = 0; i < p->n_items; i++) {
        if (p->items[i] == IT_PACKAGE) {
            apply_item(IT_PACKAGE, i);
            break;
        }
    }

    recalc_stats();
}

/* ─── Enemy definitions ──────────────────────────────────────────────── */
typedef struct {
    EnemyType type;
    const char *name;
    int hp, atk, def, spd;
    bool phasing, ranged;
    int r_dmg, r_rng, r_cd;
    bool summoner;
    int s_cd;
    const char *hit_msg;
    const char *die_msg;
    char sym;
    int cp;
} EnemyDef;

static const EnemyDef EDEFS[EN_COUNT] = {
    [EN_ORGANELLE] = {
        .name="Organelle", .hp=10, .atk=3, .def=0, .spd=0,
        .hit_msg="The Organelle lurches into you.",
        .die_msg="The Organelle collapses, viscera trailing.",
        .sym='o', .cp=CP_DEF
    },
    [EN_BOTFLY] = {
        .name="Botfly", .hp=7, .atk=4, .def=0, .spd=1,
        .hit_msg="The Botfly burrows against you.",
        .die_msg="The host collapses, host barely discernible.",
        .sym='b', .cp=CP_DEF
    },
    [EN_BLACKLUNG] = {
        .name="Black Lung", .hp=18, .atk=0, .def=1, .spd=0,
        .ranged=true, .r_dmg=4, .r_rng=8, .r_cd=2,
        .hit_msg="The Black Lung exhales filth at you.",
        .die_msg="The Black Lung deflates with a wet hiss.",
        .sym='B', .cp=CP_DANGER
    },
    [EN_SHADE] = {
        .name="Shade", .hp=14, .atk=5, .def=0, .spd=0,
        .phasing=true,
        .hit_msg="The Shade passes through you like cold water.",
        .die_msg="The Shade disperses, grief unspent.",
        .sym='s', .cp=CP_MAGIC
    },
    [EN_RAT] = {
        .name="Plague Rat", .hp=12, .atk=4, .def=0, .spd=1,
        .hit_msg="The Plague Rat gnashes at your leg.",
        .die_msg="The Plague Rat's bloated form bursts.",
        .sym='r', .cp=CP_DEF
    },
    [EN_TITHE] = {
        .name="Tithe-Taker", .hp=25, .atk=6, .def=3, .spd=0,
        .hit_msg="The Tithe-Taker demands its due.",
        .die_msg="Coin and bone clatter to the floor.",
        .sym='T', .cp=CP_ITEM
    },
    [EN_CENSOR] = {
        .name="Censorite", .hp=30, .atk=7, .def=4, .spd=0,
        .hit_msg="The Censorite's two hands close around you.",
        .die_msg="The Censorite crumbles, offense unremoved.",
        .sym='C', .cp=CP_DEF
    },
    [EN_DRONE] = {
        .name="Consumption Drone", .hp=15, .atk=0, .def=0, .spd=1,
        .ranged=true, .r_dmg=3, .r_rng=7, .r_cd=1,
        .hit_msg="The Drone's advertisement burns into your mind.",
        .die_msg="The advertisement loops on a broken screen.",
        .sym='d', .cp=CP_UI
    },
    /* Bosses */
    [EN_INSIDEOUT] = {
        .name="Inside-Out-Man", .hp=80, .atk=8, .def=2, .spd=0,
        .hit_msg="The Inside-Out-Man shows you his soft places.",
        .die_msg="He collapses inward. An ally scorned.",
        .sym='M', .cp=CP_BOSS
    },
    [EN_TENKNIVES] = {
        .name="Ten-Knives, Wasteland King", .hp=140, .atk=10, .def=3, .spd=0,
        .ranged=true, .r_dmg=8, .r_rng=10, .r_cd=2,
        .hit_msg="A knife finds its mark on your skin.",
        .die_msg="The King falls. Eleven notches remain on his belt.",
        .sym='K', .cp=CP_BOSS
    },
    [EN_THRONE] = {
        .name="The Throne Beneath Heaven", .hp=220, .atk=12, .def=5, .spd=0,
        .ranged=true, .r_dmg=10, .r_rng=12, .r_cd=3,
        .summoner=true, .s_cd=5,
        .hit_msg="The Throne's authority is absolute.",
        .die_msg="Salvation waits at the top. You have found it.",
        .sym='O', .cp=CP_BOSS
    },
};

/* ─── Spawn enemies ──────────────────────────────────────────────────── */
static void place_enemy(Room *r, EnemyType et) {
    if (r->n_enemies >= MAX_ROOM_EN) return;
    Enemy *e = &r->enemies[r->n_enemies++];
    memset(e, 0, sizeof(Enemy));
    const EnemyDef *d = &EDEFS[et];
    strncpy(e->name, d->name, 31);
    e->type    = et;
    e->hp      = d->hp;
    e->max_hp  = d->hp;
    e->atk     = d->atk;
    e->def     = d->def;
    e->spd     = d->spd;
    e->boss    = (et >= EN_INSIDEOUT);
    e->phasing = d->phasing;
    e->ranged  = d->ranged;
    e->r_dmg   = d->r_dmg;
    e->r_rng   = d->r_rng;
    e->r_cd    = d->r_cd;
    e->r_tick  = 0;
    e->summoner= d->summoner;
    e->s_cd    = d->s_cd;
    e->s_tick  = 0;
    e->hit_msg = d->hit_msg;
    e->die_msg = d->die_msg;
    e->sym     = d->sym;
    e->cp      = d->cp;
    e->alive   = true;
    /* random position in room interior */
    int tries = 0;
    do {
        e->x = 1 + rand() % r->w;
        e->y = 1 + rand() % r->h;
        tries++;
    } while (tries < 100 && r->tiles[e->y][e->x] != T_FLOOR);
    /* Fallback: scan for any floor tile if random placement failed */
    if (r->tiles[e->y][e->x] != T_FLOOR) {
        bool found = false;
        for (int fy = 1; fy <= r->h && !found; fy++)
            for (int fx = 1; fx <= r->w && !found; fx++)
                if (r->tiles[fy][fx] == T_FLOOR) {
                    e->x = fx; e->y = fy; found = true;
                }
        if (!found) { r->n_enemies--; return; }
    }
}

static EnemyType floor_enemies[3][4] = {
    /* floor 0 */ { EN_ORGANELLE, EN_BOTFLY,    EN_ORGANELLE, EN_BOTFLY   },
    /* floor 1 */ { EN_BLACKLUNG, EN_SHADE,     EN_RAT,       EN_TITHE    },
    /* floor 2 */ { EN_CENSOR,    EN_DRONE,     EN_SHADE,     EN_TITHE    },
};

void spawn_room_enemies(Room *r, int floor_num) {
    int fl = floor_num;
    if (fl >= 3) fl = 2;
    int n = 2 + floor_num + roll(0, 2);
    if (n > MAX_ROOM_EN) n = MAX_ROOM_EN;
    for (int i = 0; i < n; i++) {
        EnemyType et = floor_enemies[fl][rand() % 4];
        place_enemy(r, et);
    }
}

void spawn_boss(Room *r, int floor_num) {
    EnemyType bt;
    switch (floor_num) {
        case 0:  bt = EN_INSIDEOUT;  break;
        case 1:  bt = EN_TENKNIVES;  break;
        default: bt = EN_THRONE;     break;
    }
    place_enemy(r, bt);
    r->enemies[0].phase = 1;
    log_msg(""); /* spacer */
}

/* ─── Knockback ──────────────────────────────────────────────────────── */
static bool pos_has_enemy(Room *r, int x, int y); /* forward decl */
static void apply_knockback(Enemy *e, int dx, int dy, int tiles) {
    Room *r = cur_room();
    for (int i = 0; i < tiles; i++) {
        int nx = e->x + dx;
        int ny = e->y + dy;
        if (nx < 0 || ny < 0 || nx >= RT_W || ny >= RT_H) break;
        TileType t = r->tiles[ny][nx];
        if (t == T_WALL || t == T_DOOR_LOCK || t == T_VOID) {
            /* Wall slam: bonus damage */
            int bonus = tiles;
            e->hp -= bonus;
            log_msg("The %s slams into the wall! (%d bonus damage)", e->name, bonus);
            if (e->hp <= 0) {
                e->alive = false;
                G.player.kills++;
                log_msg("%s", e->die_msg);
                check_room_clear();
            }
            break;
        }
        if (pos_has_enemy(r, nx, ny)) break; /* stop before occupied tile */
        e->x = nx;
        e->y = ny;
    }
}

/* ─── Combat: player attacks enemy ──────────────────────────────────── */
static void player_attack_enemy(Enemy *e, int dx, int dy) {
    Player *p = &G.player;

    /* Instakill check (non-boss) */
    if (p->instakill && !e->boss) {
        float r = (float)rand() / RAND_MAX;
        if (r < p->ik_ch) {
            log_msg("You end the %s cleanly. One shot.", e->name);
            e->hp = 0;
            e->alive = false;
            p->kills++;
            log_msg("%s", e->die_msg);
            check_room_clear();
            return;
        }
    }

    int dmg = p->atk + roll(-1, 2) - e->def;
    if (dmg < 1) dmg = 1;
    e->hp -= dmg;
    log_msg("You strike the %s for %d damage.", e->name, dmg);

    /* Bleed */
    if (p->bleed_on_hit && !e->boss && e->bleed < 3) {
        e->bleed = 3;
        log_msg("The %s bleeds.", e->name);
    }

    if (e->hp <= 0) {
        e->alive = false;
        p->kills++;
        log_msg("%s", e->die_msg);
        check_room_clear();
        return;
    }

    /* Knockback (melee only, non-boss) */
    if (!e->boss && p->momentum > 0) {
        apply_knockback(e, dx, dy, p->momentum);
    }
}

/* ─── Combat: enemy attacks player ──────────────────────────────────── */
static void enemy_attack_player(Enemy *e) {
    Player *p = &G.player;

    /* Lost Days: 20% miss */
    if (p->lost_days && (rand() % 5) == 0) {
        log_msg("%s's blow goes wide.", e->name);
        return;
    }

    int dmg = e->atk + roll(-1, 1) - p->def;
    if (p->heretic) dmg = (int)(dmg * 1.8f);
    if (dmg < 1) dmg = 1;

    /* Crown of Teeth: reflect */
    if (p->reflect) {
        float r = (float)rand() / RAND_MAX;
        if (r < p->rf_ch) {
            int ref = dmg / 2 + 1;
            e->hp -= ref;
            log_msg("Your Crown of Teeth reflects %d damage!", ref);
            if (e->hp <= 0) {
                e->alive = false;
                p->kills++;
                log_msg("%s", e->die_msg);
                check_room_clear();
                return;
            }
        }
    }

    p->hp -= dmg;
    log_msg("%s", e->hit_msg);
    log_msg("  You take %d damage. (%d/%d HP)", dmg, p->hp, p->max_hp);

    /* Death save */
    if (p->hp <= 0 && p->death_save && !p->death_save_used) {
        p->hp = 1;
        p->death_save_used = true;
        log_msg("Something keeps you alive. Not yet.");
        return;
    }
    if (p->hp <= 0) {
        G.game_over = true;
        G.won = false;
    }
}

/* ─── Player movement / interaction ─────────────────────────────────── */
bool player_move(int dx, int dy) {
    Player *p = &G.player;
    Room *r = cur_room();
    int nx = p->x + dx;
    int ny = p->y + dy;

    /* Check for enemy at target */
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive) continue;
        if (e->x == nx && e->y == ny) {
            player_attack_enemy(e, dx, dy);
            p->turn++;
            return true;
        }
    }

    if (nx < 0 || ny < 0 || nx >= RT_W || ny >= RT_H) return false;
    TileType tile = r->tiles[ny][nx];

    if (tile == T_WALL) {
        /* Corridor sidewall fix: the renderer draws all rooms in world space,
           so a visually-visible floor tile from an adjacent room may sit at the
           same world position as this corridor's T_WALL.  If so, enter that room. */
        int pwx = cur_room()->wx + p->x;
        int pwy = cur_room()->wy + p->y;
        int twx = pwx + dx;
        int twy = pwy + dy;
        int tx, ty;
        int ri = room_at_world(twx, twy, &tx, &ty);
        if (ri >= 0 && ri != p->current_room) {
            Floor *fl = G.cf;
            Room  *tr = &fl->rooms[ri];
            TileType wt = tr->tiles[ty][tx];
            if (wt == T_FLOOR || wt == T_DOOR_OPEN || wt == T_ITEM || wt == T_STAIRS) {
                /* Determine entry direction for the target room */
                int from_d = -1;
                for (int d = 0; d < 4; d++) {
                    if (DX[d] == dx && DY[d] == dy) { from_d = d; break; }
                }
                enter_room(ri, from_d);
                p->turn++;
                return true;
            }
        }
        return false;
    }
    if (tile == T_DOOR_LOCK) {
        log_msg("The door is sealed. The room must be cleared.");
        return false;
    }
    if (tile == T_STAIRS) {
        /* Advance floor */
        int next = p->floor_num + 1;
        if (next >= NUM_FLOORS) {
            G.won = true;
            G.game_over = true;
            return true;
        }
        p->floor_num = next;
        G.cf = &G.floors[next];
        if (G.cf->n_rooms == 0)
            generate_floor(G.cf, next);
        enter_room(G.cf->start_room, -1);
        log_msg("You ascend. Floor %d.", next + 1);
        p->turn++;
        return true;
    }
    if (tile == T_DOOR_OPEN) {
        /* Room transition */
        for (int d = 0; d < 4; d++) {
            if (r->conn[d] >= 0 &&
                r->door_x[d] == nx && r->door_y[d] == ny) {
                /* check move aligns with door direction */
                if (dx == DX[d] && dy == DY[d]) {
                    enter_room(r->conn[d], d);
                    p->turn++;
                    return true;
                }
            }
        }
        /* fallthrough: step onto door tile without transition */
    }

    p->x = nx;
    p->y = ny;

    /* Notify player when standing on an item */
    if (tile == T_ITEM) {
        Room *cr = cur_room();
        for (int i = 0; i < cr->n_items; i++) {
            if (cr->items[i] != IT_NONE && cr->ix[i] == nx && cr->iy[i] == ny) {
                log_msg("You see: %s  [g] to pick up", ITEMS[cr->items[i]].name);
                break;
            }
        }
    }

    p->turn++;
    return true;
}

/* ─── Shot symbol helper ─────────────────────────────────────────────── */
static char shot_sym(int dx, int dy) {
    if (dy == 0) return '-';
    if (dx == 0) return '|';
    return '*';
}

/* ─── Shoot ──────────────────────────────────────────────────────────── */
bool player_shoot(int dx, int dy) {
    Player *p = &G.player;
    if (!p->has_ranged) {
        log_msg("You have no ranged weapon.");
        return false;
    }
    if (p->r_ammo == 0) {
        log_msg("No ammunition.");
        return false;
    }

    /* Set up animated projectile */
    Proj *pr = &G.projs[0];
    pr->sym      = shot_sym(dx, dy);
    pr->cp       = CP_ITEM;
    pr->enemy_shot = false;
    pr->active   = true;

    Room *r = cur_room();
    int cx = p->x + dx, cy = p->y + dy;
    int rng = p->r_rng;
    bool hit = false;

    while (rng-- > 0) {
        if (cx < 0 || cy < 0 || cx >= RT_W || cy >= RT_H) break;
        TileType t = r->tiles[cy][cx];
        if (t == T_WALL || t == T_DOOR_LOCK) break;

        pr->x = cx; pr->y = cy;
        render_game();
        napms(40);

        for (int i = 0; i < r->n_enemies; i++) {
            Enemy *e = &r->enemies[i];
            if (!e->alive) continue;
            if (e->x == cx && e->y == cy) {
                int dmg = p->r_dmg + roll(0, 2) - e->def;
                if (dmg < 1) dmg = 1;
                e->hp -= dmg;
                log_msg("You shoot the %s for %d damage.", e->name, dmg);
                if (e->hp <= 0) {
                    e->alive = false;
                    p->kills++;
                    log_msg("%s", e->die_msg);
                    check_room_clear();
                }
                hit = true;
                break;
            }
        }
        if (hit) break;
        cx += dx; cy += dy;
    }

    pr->active = false;

    if (!hit) log_msg("Your shot finds nothing.");
    if (p->r_ammo > 0) p->r_ammo--;
    p->turn++;
    return true;
}

/* ─── Grenade ────────────────────────────────────────────────────────── */
void player_use_grenade(void) {
    Player *p = &G.player;
    if (p->grenades <= 0) {
        log_msg("No grenades remaining.");
        return;
    }
    p->grenades--;
    Room *r = cur_room();
    int total = 0;
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive) continue;
        int dmg = 15 + roll(-2, 4);
        e->hp -= dmg;
        total++;
        if (e->hp <= 0) {
            e->alive = false;
            p->kills++;
            log_msg("The %s burns in whitefire!", e->name);
            check_room_clear();
        }
    }
    if (total > 0)
        log_msg("Whitefire erupts! %d enemies scorched. (%d grenades left)", total, p->grenades);
    else
        log_msg("The grenade detonates in an empty room. Wasteful.");
    p->turn++;
}

/* ─── Pick up items ──────────────────────────────────────────────────── */
void pick_up_items(void) {
    Player *p = &G.player;
    Room *r = cur_room();
    for (int i = 0; i < r->n_items; i++) {
        if (r->items[i] == IT_NONE) continue;
        if (r->ix[i] != p->x || r->iy[i] != p->y) continue;
        if (p->n_items >= p->max_items) {
            log_msg("Your hands are full. Drop something first.");
            continue;
        }
        ItemType it = r->items[i];
        /* Apply max_hp bonus immediately */
        if (ITEMS[it].max_hp > 0) {
            p->max_hp += (int)(ITEMS[it].max_hp * (p->item_boost ? 1.5f : 1.0f));
            p->hp     += ITEMS[it].max_hp;
            if (p->hp > p->max_hp) p->hp = p->max_hp;
        }
        if (ITEMS[it].hp_heal > 0) {
            p->hp += ITEMS[it].hp_heal;
            if (p->hp > p->max_hp) p->hp = p->max_hp;
        }
        if (ITEMS[it].consumable && it == IT_GRENADES) {
            p->grenades += ITEMS[it].charges;
            log_msg("You pick up %s. (%d grenades total)", ITEMS[it].name, p->grenades);
        } else {
            p->items[p->n_items]   = it;
            p->charges[p->n_items] = ITEMS[it].charges;
            p->n_items++;
            log_msg("You pick up: %s", ITEMS[it].name);
            log_msg("  \"%s\"", ITEMS[it].desc);
        }
        apply_item(it, p->n_items - 1);
        recalc_stats();

        r->tiles[r->iy[i]][r->ix[i]] = T_FLOOR;
        r->items[i] = IT_NONE;
    }
}

/* ─── Enemy AI ───────────────────────────────────────────────────────── */
static bool tile_passable(Room *r, int x, int y, bool phase) {
    if (x < 0 || y < 0 || x >= RT_W || y >= RT_H) return false;
    TileType t = r->tiles[y][x];
    if (phase) return (t != T_VOID);
    return (t == T_FLOOR || t == T_DOOR_OPEN || t == T_ITEM || t == T_STAIRS);
}

/* ─── BFS pathfinding ────────────────────────────────────────────────── *
 * Returns the direction index (0-3) of the first step from (sx,sy) toward
 * (gx,gy) navigating around walls, or -1 if no path exists.
 * Uses the room tile grid; phasing enemies ignore walls.
 */
static int bfs_step(Room *r, int sx, int sy, int gx, int gy, bool phase) {
    if (sx == gx && sy == gy) return -1;

    /* from[y][x] = direction we travelled to reach (x,y); 4 = start, -1 = unseen */
    int8_t from[RT_H][RT_W];
    memset(from, -1, sizeof(from));
    from[sy][sx] = 4;

    int queue[RT_H * RT_W];
    int head = 0, tail = 0;
    queue[tail++] = sy * RT_W + sx;

    while (head < tail) {
        int cell = queue[head++];
        int cx = cell % RT_W, cy = cell / RT_W;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || ny < 0 || nx >= RT_W || ny >= RT_H) continue;
            if (from[ny][nx] != -1) continue;
            /* Check goal first — player tile is always reachable regardless of type */
            if (nx == gx && ny == gy) { from[ny][nx] = (int8_t)d; goto found; }
            if (!tile_passable(r, nx, ny, phase)) continue;
            from[ny][nx] = (int8_t)d;
            queue[tail++] = ny * RT_W + nx;
        }
    }
    return -1; /* no path */

found:;
    /* Trace back from goal to find first step */
    int x = gx, y = gy, first = -1;
    while (x != sx || y != sy) {
        first = from[y][x];
        x -= DX[first];
        y -= DY[first];
    }
    return first;
}

static bool pos_has_enemy(Room *r, int x, int y) {
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (e->alive && e->x == x && e->y == y) return true;
    }
    return false;
}

static void enemy_shoot_player(Enemy *e) {
    Player *p = &G.player;
    int ex = e->x, ey = e->y;
    int px = p->x, py = p->y;
    int dx = (px > ex) - (px < ex);
    int dy = (py > ey) - (py < ey);
    /* Only shoot if roughly in a line */
    if (dx == 0 || dy == 0) {
        Proj *pr = &G.projs[0];
        pr->sym       = shot_sym(dx, dy);
        pr->cp        = CP_DANGER;
        pr->enemy_shot = true;
        pr->active    = true;

        int cx = ex + dx, cy = ey + dy;
        Room *r = cur_room();
        int rng = e->r_rng;
        bool hit = false;
        while (rng-- > 0) {
            if (cx < 0 || cy < 0 || cx >= RT_W || cy >= RT_H) break;
            TileType t = r->tiles[cy][cx];
            if (t == T_WALL || t == T_DOOR_LOCK) break;

            pr->x = cx; pr->y = cy;
            render_game();
            napms(40);

            if (cx == px && cy == py) {
                hit = true;
                break;
            }
            cx += dx; cy += dy;
        }
        pr->active = false;

        if (hit) {
            enemy_attack_player(e);
            return;
        }
    }
    log_msg("The %s's shot goes wide.", e->name);
}

static void summon_minion(Room *r, int floor_num) {
    EnemyType et = (floor_num >= 2) ? EN_CENSOR : EN_ORGANELLE;
    place_enemy(r, et);
    log_msg("The Throne summons a servant from the dark.");
}

void update_enemies(void) {
    Player *p = &G.player;
    Room   *r = cur_room();
    if (G.game_over) return;

    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive) continue;
        if (!r->visited)  continue;

        e->alerted = true;

        /* Bleed tick */
        if (e->bleed > 0) {
            e->hp--;
            e->bleed--;
            if (e->hp <= 0) {
                e->alive = false;
                p->kills++;
                log_msg("The %s bleeds out.", e->name);
                check_room_clear();
                continue;
            }
        }

        /* Speed: fast enemies act twice */
        int actions = 1 + e->spd;
        for (int ac = 0; ac < actions; ac++) {
            if (!e->alive) break;
            if (G.game_over) return;

            int dist = abs(e->x - p->x) + abs(e->y - p->y);

            /* Ranged attack */
            if (e->ranged && dist <= e->r_rng) {
                e->r_tick++;
                if (e->r_tick >= e->r_cd) {
                    e->r_tick = 0;
                    enemy_shoot_player(e);
                    if (G.game_over) return;
                    continue;
                }
            }

            /* Summoner (boss) */
            if (e->summoner && e->boss) {
                e->s_tick++;
                if (e->s_tick >= e->s_cd) {
                    e->s_tick = 0;
                    summon_minion(r, p->floor_num);
                }
            }

            /* Adjacent: melee */
            if (dist <= 1 && !e->ranged) {
                enemy_attack_player(e);
                if (G.game_over) return;
                continue;
            }
            if (dist == 1 && e->ranged && e->r_tick < e->r_cd) {
                enemy_attack_player(e);
                if (G.game_over) return;
                continue;
            }

            /* BFS toward player */
            {
                int step = bfs_step(r, e->x, e->y, p->x, p->y, e->phasing);
                if (step >= 0) {
                    int nx = e->x + DX[step], ny = e->y + DY[step];
                    if (!(nx == p->x && ny == p->y) &&
                        tile_passable(r, nx, ny, e->phasing) &&
                        !pos_has_enemy(r, nx, ny)) {
                        e->x = nx; e->y = ny;
                    }
                }
            }

            /* After moving, check adjacency */
            dist = abs(e->x - p->x) + abs(e->y - p->y);
            if (dist <= 1 && !e->ranged) {
                enemy_attack_player(e);
                if (G.game_over) return;
            }
        }
    }
}

/* ─── Room clear check ───────────────────────────────────────────────── */
void check_room_clear(void) {
    Room *r = cur_room();
    if (r->cleared) return;
    for (int i = 0; i < r->n_enemies; i++)
        if (r->enemies[i].alive) return;
    r->cleared = true;
    unlock_doors(r);
    if (r->boss_room) {
        /* Reveal stairs */
        r->tiles[r->h/2 + 1][r->w/2 + 1] = T_STAIRS;
        log_msg("The Tower shudders. Stairs appear. Ascend.");
    } else {
        log_msg("The room is cleared. The doors open.");
    }
}

/* ─── Passive effects ────────────────────────────────────────────────── */
void do_regen(void) {
    Player *p = &G.player;
    float rate = 0.0f;
    for (int i = 0; i < p->n_items; i++) {
        const ItemDef *d = &ITEMS[p->items[i]];
        if (d->regen) rate += d->regen_rate;
    }
    if (rate <= 0.0f) return;
    p->regen_acc += rate;
    while (p->regen_acc >= 1.0f && p->hp < p->max_hp) {
        p->hp++;
        p->regen_acc -= 1.0f;
    }
    if (p->regen_acc >= 1.0f) p->regen_acc = 0.0f;
}

void do_lantern(void) {
    if (!G.player.lantern) return;
    Room *r = cur_room();
    for (int i = 0; i < r->n_enemies; i++) {
        Enemy *e = &r->enemies[i];
        if (!e->alive) continue;
        int dist = abs(e->x - G.player.x) + abs(e->y - G.player.y);
        if (dist <= 1) {
            e->hp--;
            if (e->hp <= 0) {
                e->alive = false;
                G.player.kills++;
                log_msg("The %s melts in your lantern's glow.", e->name);
                check_room_clear();
            }
        }
    }
}
