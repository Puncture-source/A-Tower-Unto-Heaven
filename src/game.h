#ifndef GAME_H
#define GAME_H

#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>

/* ─── Constants ────────────────────────────────────────────────────── */
#define GAME_NAME   "A Tower Unto Heaven"
#define NUM_FLOORS  3
#define UI_W        22      /* right-side panel width */
#define MSG_H       5       /* bottom message rows */
#define MAX_ROOMS   60
#define MAX_ROOM_W  30      /* interior (excl. walls) */
#define MAX_ROOM_H  14
#define RT_W        (MAX_ROOM_W + 2)
#define RT_H        (MAX_ROOM_H + 2)
#define MAX_ROOM_EN 10      /* enemies per room */
#define MAX_ROOM_IT 3       /* item pickups per room */
#define MAX_PLR_IT  24      /* hard array cap */
#define BASE_SLOTS   6      /* default item slots */
#define MAX_MSG     80
#define MSG_LEN     120

/* ─── Colour pairs ──────────────────────────────────────────────────── */
#define CP_DEF      1   /* white on black   */
#define CP_DANGER   2   /* red              */
#define CP_PLAYER   3   /* green            */
#define CP_ITEM     4   /* yellow           */
#define CP_WALL     5   /* blue             */
#define CP_UI       6   /* cyan             */
#define CP_MAGIC    7   /* magenta          */
#define CP_BOSS     8   /* red bold         */
#define CP_FLOOR    9   /* dark grey (white)*/
#define CP_BLOOD    10  /* red on black     */

/* ─── Directions ────────────────────────────────────────────────────── */
typedef enum { D_N=0, D_S, D_E, D_W } Dir;
static const int DX[4] = { 0,  0,  1, -1 };
static const int DY[4] = {-1,  1,  0,  0 };
static const Dir OPP[4] = { D_S, D_N, D_W, D_E };

/* ─── Tile ──────────────────────────────────────────────────────────── */
typedef enum {
    T_VOID=0, T_WALL, T_FLOOR,
    T_DOOR_OPEN, T_DOOR_LOCK,
    T_STAIRS, T_ITEM, T_VISCERA
} TileType;

/* ─── Items ─────────────────────────────────────────────────────────── */
typedef enum {
    IT_NONE=0,
    /* general pool */
    IT_BEANS,       /* Old Can of Beans      */
    IT_SUTURES,     /* Homemade Sutures      */
    IT_RAZOR,       /* Occam's Razor         */
    IT_WOAD,        /* Holy Woad             */
    IT_BLOODY,      /* Bloody Hands          */
    IT_SNAIL,       /* Snail-Shell Necklace  */
    IT_LOSTDAYS,    /* Lost Days             */
    IT_GOSPEL,      /* The Nuclear Gospel    */
    IT_CROWN,       /* Crown of Teeth        */
    IT_RUST,        /* Rust                  */
    IT_REMAINS,     /* What Remains          */
    IT_FLAGS,       /* Tattered Flags        */
    IT_HERETIC,     /* Heretic's Heart       */
    IT_LANTERN,     /* Melting Lantern       */
    IT_PROFESSIONAL,/* The Professional      */
    IT_TOWER_PIECE, /* A Piece of the Tower  */
    IT_GRENADES,    /* Sack of Whitefire Grenades (consumable) */
    IT_RSTONE,      /* Right Hand Stone      */
    IT_LSTONE,      /* Left Hand Stone       */
    /* character starters */
    IT_9MM,         /* 9mm pistol            */
    IT_DUFFEL,      /* Duffel bag            */
    IT_T_SHIELD,    /* Tower Shield          */
    IT_I_AXE,       /* Inscribed Axe         */
    IT_SHOTGUN,     /* Homemade Shotgun      */
    IT_EMB_BULLET,  /* Embedded Bullet       */
    IT_TOME,        /* Tome of Kings         */
    IT_PACKAGE,     /* The Package           */
    IT_ASSAULT,     /* Assault Rifle         */
    IT_COUNT
} ItemType;

typedef struct {
    ItemType    type;
    const char *name;
    const char *desc;
    int  max_hp;
    int  hp_heal;
    int  atk;
    int  def;
    int  spd;           /* speed modifier: +1 fast, -1 slow */
    bool gives_ranged;
    int  r_dmg;
    int  r_rng;
    int  r_ammo;        /* -1 = unlimited */
    bool regen;
    float regen_rate;   /* HP per turn */
    bool instakill;
    float ik_ch;
    bool reflect;
    float rf_ch;
    bool bleed;
    bool consumable;
    int  charges;
    bool item_boost;    /* Tome of Kings effect */
    int  extra_slots;   /* Duffel: number of bonus item slots */
    int  momentum;      /* melee knockback tiles added */
    char sym;
    int  cp;
} ItemDef;

/* ─── Characters ────────────────────────────────────────────────────── */
typedef enum {
    CH_JACKAL=0, CH_ZEALOT, CH_FERDINAND,
    CH_RUNBOY, CH_HUA, CH_COUNT
} CharType;

typedef struct {
    CharType    type;
    const char *name;
    const char *desc;
    int  hp, atk, def;
    ItemType start[4];
    int  n_start;
    char sym;
    int  cp;
} CharDef;

/* ─── Enemies ────────────────────────────────────────────────────────── */
typedef enum {
    EN_ORGANELLE=0, EN_BOTFLY,    EN_BLACKLUNG,
    EN_SHADE,       EN_RAT,       EN_TITHE,
    EN_CENSOR,      EN_DRONE,
    /* bosses */
    EN_INSIDEOUT,   EN_TENKNIVES, EN_THRONE,
    EN_COUNT
} EnemyType;

typedef struct {
    EnemyType   type;
    char        name[32];
    int         x, y;
    int         hp, max_hp;
    int         atk, def;
    int         spd;        /* extra moves per turn (0=normal,1=fast) */
    int         spd_tick;
    bool        alive;
    bool        boss;
    bool        alerted;
    bool        phasing;    /* walks through walls */
    bool        ranged;
    int         r_dmg, r_rng, r_cd, r_tick;
    bool        summoner;
    int         s_cd, s_tick;
    int         bleed;      /* bleed ticks remaining */
    int         phase;      /* boss phase */
    const char *hit_msg;
    const char *die_msg;
    char        sym;
    int         cp;
} Enemy;

/* ─── Room ──────────────────────────────────────────────────────────── */
typedef struct {
    TileType tiles[RT_H][RT_W];
    int  w, h;              /* interior dimensions */
    int  conn[4];           /* neighbour room idx or -1 */
    int  door_x[4];
    int  door_y[4];
    bool visited;
    bool cleared;
    bool boss_room;
    bool item_room;
    bool is_corridor;
    bool is_alcove;     /* dead-end pocket, no enemies */
    bool has_stairs;
    int  map_x, map_y;  /* minimap grid position (set by BFS after generation) */
    Enemy    enemies[MAX_ROOM_EN];
    int      n_enemies;
    ItemType items[MAX_ROOM_IT];
    int      ix[MAX_ROOM_IT];
    int      iy[MAX_ROOM_IT];
    int      n_items;
} Room;

/* ─── Floor ─────────────────────────────────────────────────────────── */
typedef struct {
    Room rooms[MAX_ROOMS];
    int  n_rooms;
    int  start_room;
    int  boss_room;
    int  floor_num;
} Floor;

/* ─── Message log ───────────────────────────────────────────────────── */
typedef struct {
    char lines[MAX_MSG][MSG_LEN];
    int  count;
} MsgLog;

/* ─── Projectile ────────────────────────────────────────────────────── */
#define MAX_PROJ 16
typedef struct {
    int  x, y, dx, dy;
    int  dmg, rng_left;
    bool active;
    bool enemy_shot;
    char sym;
    int  cp;
} Proj;

/* ─── Player ────────────────────────────────────────────────────────── */
typedef struct {
    int      x, y;
    int      hp, max_hp;
    int      base_atk, base_def;
    CharType chr;
    ItemType items[MAX_PLR_IT];
    int      charges[MAX_PLR_IT];
    int      n_items;
    /* derived (recalculated after item change) */
    int      atk, def;
    bool     has_ranged;
    int      r_dmg, r_rng, r_ammo;
    float    regen_acc;
    bool     instakill;
    float    ik_ch;
    bool     reflect;
    float    rf_ch;
    bool     bleed_on_hit;
    bool     heretic;       /* double atk, double recv */
    bool     lantern;       /* adjacent enemies take 1 dmg/turn */
    bool     death_save;
    bool     death_save_used;
    bool     item_boost;    /* Tome of Kings */
    bool     lost_days;     /* 20% enemy miss */
    int      spd;           /* speed: +N = N bonus moves, -N = N extra enemy turns */
    int      max_items;     /* derived: BASE_SLOTS + extra_slots from items */
    bool     rstone;
    bool     lstone;
    int      grenades;
    int      momentum;  /* melee knockback tiles (base 1 + items) */
    /* tracking */
    int      kills;
    int      floor_num;
    int      current_room;
    int      turn;
    int      last_dir;      /* last movement direction */
} Player;

/* ─── Game ──────────────────────────────────────────────────────────── */
typedef struct {
    Player  player;
    Floor   floors[NUM_FLOORS];
    Floor  *cf;             /* current floor pointer */
    Proj    projs[MAX_PROJ];
    int     n_projs;
    MsgLog  log;
    bool    game_over;
    bool    won;
    bool    running;
    int     tw, th;         /* terminal dimensions */
    int     gw, gh;         /* game display area */
} Game;

extern Game G;
extern const ItemDef  ITEMS[IT_COUNT];
extern const CharDef  CHARS[CH_COUNT];

/* ─── Prototypes ────────────────────────────────────────────────────── */

/* items.c */
void        apply_item(ItemType it, int slot);
void        recalc_stats(void);
ItemType    random_pool_item(void);

/* map.c */
void  generate_floor(Floor *f, int floor_num);
Room *cur_room(void);
void  enter_room(int idx, int from_dir);
void  lock_doors(Room *r);
void  unlock_doors(Room *r);

/* entity.c */
void  init_player(CharType ct);
bool  player_move(int dx, int dy);
bool  player_shoot(int dx, int dy);
void  player_use_grenade(void);
void  pick_up_items(void);
void  update_enemies(void);
void  update_projs(void);
void  check_room_clear(void);
void  do_regen(void);
void  do_lantern(void);
void  spawn_room_enemies(Room *r, int floor_num);
void  spawn_boss(Room *r, int floor_num);
int   roll(int lo, int hi);
void  log_msg(const char *fmt, ...);

/* render.c */
void  render_init(void);
void  render_cleanup(void);
void  render_game(void);
void  render_title(void);
int   render_char_select(void);
void  render_game_over(void);
void  render_win(void);
void  render_inventory(int *sel);
void  render_help(void);
void  render_map(void);

#endif /* GAME_H */
