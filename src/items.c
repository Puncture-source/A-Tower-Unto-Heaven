#include "game.h"

/* ─── Item definitions ─────────────────────────────────────────────── */
const ItemDef ITEMS[IT_COUNT] = {
    [IT_NONE] = { .name="Nothing", .desc="", .sym=' ', .cp=CP_DEF },

    [IT_BEANS] = {
        .name="Old Can of Beans",
        .desc="A scavenger's meal. Keeps you alive a little longer.",
        .max_hp=15, .hp_heal=10,
        .sym='%', .cp=CP_ITEM
    },
    [IT_SUTURES] = {
        .name="Homemade Sutures",
        .desc="Make do with what is available. It will have to be enough.",
        .regen=true, .regen_rate=0.25f,   /* 1 HP every 4 turns */
        .sym='+', .cp=CP_ITEM
    },
    [IT_RAZOR] = {
        .name="Occam's Razor",
        .desc="One cannot afford to make undue assumptions when shaving.",
        .atk=3,
        .sym='/', .cp=CP_ITEM
    },
    [IT_WOAD] = {
        .name="Holy Woad",
        .desc="Blood and fury are mightier than any armor.",
        .def=5,
        .sym='~', .cp=CP_MAGIC
    },
    [IT_BLOODY] = {
        .name="Bloody Hands",
        .desc="Survival comes at a cost.",
        .atk=6, .def=-3,
        .sym=',', .cp=CP_BLOOD
    },
    [IT_SNAIL] = {
        .name="Snail-Shell Necklace",
        .desc="A trinket of no outward worth. Somehow, it protects you.",
        .max_hp=15,
        .sym='o', .cp=CP_ITEM
    },
    [IT_LOSTDAYS] = {
        .name="Lost Days",
        .desc="A grimy stuffed toy with a limp neck. Still smiling.",
        /* handled via player.lost_days flag */
        .sym='t', .cp=CP_MAGIC
    },
    [IT_GOSPEL] = {
        .name="The Nuclear Gospel",
        .desc="\"And lo, I saw a rider upon a glowing glass horse...\"",
        .gives_ranged=true, .r_dmg=3, .r_rng=7, .r_ammo=-1,
        .sym='B', .cp=CP_DANGER
    },
    [IT_CROWN] = {
        .name="Crown of Teeth",
        .desc="The authority of a decayed queen rests on your brow.",
        .reflect=true, .rf_ch=0.30f,
        .sym='^', .cp=CP_ITEM
    },
    [IT_RUST] = {
        .name="Rust",
        .desc="There's no telling what it once was, but it still has use.",
        .bleed=true,
        .sym='.', .cp=CP_BLOOD
    },
    [IT_REMAINS] = {
        .name="What Remains",
        .desc="Draw upon the power of a soul without rest.",
        /* handled via player.death_save */
        .sym='R', .cp=CP_MAGIC
    },
    [IT_FLAGS] = {
        .name="Tattered Flags",
        .desc="Pulled from a ship stranded on a dune.",
        .atk=2, .def=2, .max_hp=10,
        .sym='f', .cp=CP_ITEM
    },
    [IT_HERETIC] = {
        .name="Heretic's Heart",
        .desc="It is still warm. Let its power become yours. At a cost.",
        /* handled via player.heretic */
        .sym='H', .cp=CP_BOSS
    },
    [IT_LANTERN] = {
        .name="Melting Lantern",
        .desc="As it gives light, so it dies. The puddles it leaves glow.",
        /* handled via player.lantern */
        .sym='l', .cp=CP_ITEM
    },
    [IT_PROFESSIONAL] = {
        .name="The Professional",
        .desc="Spend a single bullet for a single death. Clean, competent.",
        .instakill=true, .ik_ch=0.10f,
        .sym='P', .cp=CP_ITEM
    },
    [IT_TOWER_PIECE] = {
        .name="A Piece of the Tower",
        .desc="A remembrance. The Tower hums through you.",
        .max_hp=5, .regen=true, .regen_rate=0.125f,
        .sym='*', .cp=CP_UI
    },
    [IT_GRENADES] = {
        .name="Sack of Whitefire Grenades",
        .desc="Let them burn. [press E to use]",
        .consumable=true, .charges=3,
        .sym='!', .cp=CP_DANGER
    },
    [IT_RSTONE] = {
        .name="Right Hand Stone",
        .desc="One leads, and one follows. One supports.",
        .atk=3, .def=1,
        .sym=')', .cp=CP_MAGIC
    },
    [IT_LSTONE] = {
        .name="Left Hand Stone",
        .desc="One undermines. Together, they are whole.",
        .atk=1, .def=3,
        .sym='(', .cp=CP_MAGIC
    },
    /* ── Character starters ── */
    [IT_9MM] = {
        .name="9mm Pistol",
        .desc="Jackal's sidearm. Loaded and ready.",
        .gives_ranged=true, .r_dmg=4, .r_rng=8, .r_ammo=30,
        .sym=')', .cp=CP_ITEM
    },
    [IT_DUFFEL] = {
        .name="Duffel Bag",
        .desc="Extra capacity for the long climb. +6 item slots.",
        .extra_slots=6,
        .sym='D', .cp=CP_ITEM
    },
    [IT_T_SHIELD] = {
        .name="Tower Shield",
        .desc="A wall of battered iron.",
        .def=7,
        .sym='[', .cp=CP_ITEM
    },
    [IT_I_AXE] = {
        .name="Inscribed Axe",
        .desc="Ancient script of the Tower runs along the blade.",
        .atk=5, .bleed=true,
        .sym='\\', .cp=CP_ITEM
    },
    [IT_SHOTGUN] = {
        .name="Homemade Shotgun",
        .desc="Short-range. Devastating.",
        .gives_ranged=true, .r_dmg=8, .r_rng=4, .r_ammo=20,
        .sym='|', .cp=CP_ITEM
    },
    [IT_EMB_BULLET] = {
        .name="Embedded Bullet",
        .desc="A permanent reminder of mortality. It has hardened you.",
        .max_hp=25,
        .sym='.', .cp=CP_BLOOD
    },
    [IT_TOME] = {
        .name="Tome of Kings",
        .desc="Items have enhanced effects in worthy hands.",
        .item_boost=true,
        .sym='T', .cp=CP_MAGIC
    },
    [IT_PACKAGE] = {
        .name="The Package",
        .desc="A mystery delivery. Handle with care.",
        /* handled specially: gives 2 random items */
        .sym='?', .cp=CP_MAGIC
    },
    [IT_ASSAULT] = {
        .name="Assault Rifle",
        .desc="Deserter Hua's weapon. Many rounds remain.",
        .gives_ranged=true, .r_dmg=5, .r_rng=10, .r_ammo=60,
        .sym='/', .cp=CP_ITEM
    },
};

/* ─── Character definitions ─────────────────────────────────────────── */
const CharDef CHARS[CH_COUNT] = {
    [CH_JACKAL] = {
        .name="Jackal",
        .desc="Starts with the 9mm and the Duffel.\n"
              "Balanced fighter. Good at keeping distance.",
        .hp=30, .atk=5, .def=2,
        .start={ IT_9MM, IT_DUFFEL },
        .n_start=2,
        .sym='@', .cp=CP_PLAYER
    },
    [CH_ZEALOT] = {
        .name="Zealot",
        .desc="Starts with the Tower Shield, Inscribed Axe, and Shotgun.\n"
              "Slow and brutal. The enemy fears his advance.",
        .hp=40, .atk=7, .def=4,
        .start={ IT_T_SHIELD, IT_I_AXE, IT_SHOTGUN },
        .n_start=3,
        .sym='@', .cp=CP_DANGER
    },
    [CH_FERDINAND] = {
        .name="Ferdinand",
        .desc="Starts with the Embedded Bullet and the Tome of Kings.\n"
              "Tanky. Items are enhanced in his hands.",
        .hp=55, .atk=4, .def=3,
        .start={ IT_EMB_BULLET, IT_TOME },
        .n_start=2,
        .sym='@', .cp=CP_MAGIC
    },
    [CH_RUNBOY] = {
        .name="Running Boy",
        .desc="Starts with the Package. Unlocked by witnessing disaster.\n"
              "Frail but fast. The Package conceals two gifts.",
        .hp=22, .atk=5, .def=1,
        .start={ IT_PACKAGE },
        .n_start=1,
        .sym='@', .cp=CP_ITEM
    },
    [CH_HUA] = {
        .name="Deserter Hua",
        .desc="Starts with the Assault Rifle.\n"
              "High-powered ranged fighter. Keep your distance.",
        .hp=28, .atk=4, .def=2,
        .start={ IT_ASSAULT },
        .n_start=1,
        .sym='@', .cp=CP_UI
    },
};

/* ─── Item pool for random drops ─────────────────────────────────────── */
static const ItemType POOL[] = {
    IT_BEANS, IT_SUTURES, IT_RAZOR, IT_WOAD, IT_BLOODY, IT_SNAIL,
    IT_LOSTDAYS, IT_GOSPEL, IT_CROWN, IT_RUST, IT_REMAINS, IT_FLAGS,
    IT_HERETIC, IT_LANTERN, IT_PROFESSIONAL, IT_TOWER_PIECE, IT_GRENADES,
    IT_RSTONE, IT_LSTONE
};
static const int POOL_SZ = (int)(sizeof(POOL)/sizeof(POOL[0]));

ItemType random_pool_item(void) {
    return POOL[rand() % POOL_SZ];
}

/* ─── Apply item ─────────────────────────────────────────────────────── */
void apply_item(ItemType it, int slot) {
    if (it == IT_NONE) return;
    const ItemDef *d = &ITEMS[it];
    Player *p = &G.player;

    /* consumable handling */
    if (d->consumable) {
        if (slot >= 0 && p->charges[slot] > 0) {
            /* actual use handled by caller (grenades) */
        }
        return;
    }

    /* Package: give two random items, then remove */
    if (it == IT_PACKAGE) {
        log_msg("You open the Package. Inside: two gifts from nowhere.");
        for (int i = 0; i < 2; i++) {
            ItemType gift = random_pool_item();
            /* avoid duplicates with what player already has */
            bool dup = false;
            for (int j = 0; j < p->n_items; j++)
                if (p->items[j] == gift) { dup = true; break; }
            if (!dup && p->n_items < p->max_items) {
                p->items[p->n_items] = gift;
                p->charges[p->n_items] = ITEMS[gift].charges;
                p->n_items++;
                log_msg("  You find: %s.", ITEMS[gift].name);
                apply_item(gift, p->n_items - 1);
            }
        }
        /* remove package slot */
        if (slot >= 0) {
            for (int i = slot; i < p->n_items - 1; i++) {
                p->items[i]   = p->items[i+1];
                p->charges[i] = p->charges[i+1];
            }
            p->n_items--;
        }
        return;
    }

    /* Grenades: add charges */
    if (it == IT_GRENADES) {
        p->grenades += d->charges;
        return;
    }

    /* Flags: special dual-stone bonus */
    if (it == IT_RSTONE) p->rstone = true;
    if (it == IT_LSTONE) p->lstone = true;

    recalc_stats();
}

/* ─── Recalculate derived stats ──────────────────────────────────────── */
void recalc_stats(void) {
    Player *p = &G.player;
    float boost = (p->item_boost) ? 1.5f : 1.0f;

    p->atk = p->base_atk;
    p->def = p->base_def;
    p->max_items    = BASE_SLOTS;
    p->has_ranged   = false;
    p->r_dmg = p->r_rng = p->r_ammo = 0;
    p->regen_acc     = p->regen_acc; /* keep accumulator */
    float regen_sum  = 0.0f;
    p->instakill     = false;
    p->ik_ch         = 0.0f;
    p->reflect       = false;
    p->rf_ch         = 0.0f;
    p->bleed_on_hit  = false;
    p->heretic       = false;
    p->lantern       = false;
    p->death_save    = false;
    p->item_boost    = false;
    p->lost_days     = false;

    for (int i = 0; i < p->n_items; i++) {
        const ItemDef *d = &ITEMS[p->items[i]];
        if (!d) continue;

        p->atk  += (int)(d->atk  * boost);
        p->def  += (int)(d->def  * boost);
        p->max_items += d->extra_slots;
        if (d->max_hp > 0)
            p->max_hp = p->max_hp; /* max_hp applied once at pickup */

        if (d->gives_ranged) {
            p->has_ranged = true;
            if (d->r_dmg > p->r_dmg) {
                p->r_dmg = (int)(d->r_dmg * boost);
                p->r_rng = d->r_rng;
                p->r_ammo = d->r_ammo;
            }
        }
        if (d->regen)      regen_sum  += d->regen_rate;
        if (d->instakill)  { p->instakill = true; p->ik_ch += d->ik_ch; }
        if (d->reflect)    { p->reflect   = true; p->rf_ch += d->rf_ch; }
        if (d->bleed)      p->bleed_on_hit = true;
        if (d->item_boost) p->item_boost   = true;

        if (p->items[i] == IT_HERETIC)   p->heretic   = true;
        if (p->items[i] == IT_LANTERN)   p->lantern   = true;
        if (p->items[i] == IT_REMAINS)   p->death_save = true;
        if (p->items[i] == IT_LOSTDAYS)  p->lost_days = true;
    }

    /* Combined stone bonus */
    if (p->rstone && p->lstone) {
        p->atk += 5;
        p->def += 5;
        p->max_hp += 10;
    }

    /* Heretic: double attack */
    if (p->heretic) p->atk *= 2;

    /* Clamp */
    if (p->atk < 1) p->atk = 1;
    if (p->def < 0) p->def = 0;
    if (p->max_items > MAX_PLR_IT) p->max_items = MAX_PLR_IT;

    /* Regen stored as accumulated rate */
    (void)regen_sum; /* regen_acc ticked in do_regen() per-item */
}
