#include "game.h"

/* ─── Name tables ─────────────────────────────────────────────────────── */
static const char *VERB_NAMES[VERB_COUNT] = {
    "REBUKE","FORGET","ECHO","MEND","CHANT","FEED","SEAL","LIE"
};
static const char *FUEL_NAMES[FUEL_COUNT] = {
    "Glimmer","Memory","Ash","Flesh","Item","Turns"
};

/* ─── Fuel availability (non-static — used by render_combat) ──────────── */
bool fuel_available(FuelType f) {
    Player *p = &G.player;
    switch (f) {
        case FUEL_GLIMMER: return p->glimmer >= 2;
        case FUEL_MEMORY:  return p->n_memories > 0;
        case FUEL_ASH:     return p->ash >= 10;
        case FUEL_FLESH:   return p->hp > 5;
        case FUEL_ITEM:    return p->n_items > 0;
        case FUEL_TURNS:   return true;
        default: return false;
    }
}

/* ─── Consume fuel ────────────────────────────────────────────────────── */
static void consume_fuel(FuelType f) {
    Player *p = &G.player;
    switch (f) {
        case FUEL_GLIMMER:
            p->glimmer -= 2;
            if (p->glimmer < 0) p->glimmer = 0;
            break;
        case FUEL_MEMORY:
            if (p->n_memories > 0) {
                log_msg("You sacrifice: \"%s\".", p->memories[p->n_memories-1]);
                p->n_memories--;
            }
            break;
        case FUEL_ASH:
            p->ash -= 10;
            if (p->ash < 0) p->ash = 0;
            break;
        case FUEL_FLESH:
            p->hp -= 4;
            break;
        case FUEL_ITEM:
            for (int i = 0; i < p->n_items; i++) {
                if (p->charges[i] > 0) { p->charges[i]--; break; }
            }
            break;
        case FUEL_TURNS: /* multi-round — no immediate cost */ break;
        default: break;
    }
}

/* ─── Aspect helpers ──────────────────────────────────────────────────── */
static bool has_aspect(Enemy *e, Aspect a) {
    for (int i = 0; i < e->n_aspects; i++)
        if (e->aspects[i] == a) return true;
    return false;
}

static void remove_aspect(Enemy *e, Aspect a) {
    for (int i = 0; i < e->n_aspects; i++) {
        if (e->aspects[i] == a) {
            for (int j = i; j < e->n_aspects - 1; j++)
                e->aspects[j] = e->aspects[j+1];
            e->n_aspects--;
            return;
        }
    }
}

/* ─── Verb resolution ─────────────────────────────────────────────────── */
/* Returns tension damage dealt (0 = no damage) */
static int do_resolve_verb(Enemy *e, Verb v, FuelType fuel, bool *hunger_fed) {
    Player *p = &G.player;
    int tension_dmg = 0;

    /* Ash misfire: 34-66 → 15% chance of wrong verb */
    if (p->ash >= 34 && p->ash < 67 && (rand() % 7 == 0)) {
        v = (Verb)(rand() % VERB_COUNT);
        log_msg("Your intent misfires. The words come out wrong.");
    }
    /* Ash chaos: 67+ → 10% chance of total silence */
    if (p->ash >= 67 && (rand() % 10 == 0)) {
        log_msg("The static swallows your invocation.");
        p->ash += 2;
        if (p->ash > 100) p->ash = 100;
        return 0;
    }
    /* Husk: only MEND and SEAL work */
    if (p->husk && v != VERB_MEND && v != VERB_SEAL) {
        log_msg("The Husk cannot remember how to %s.", VERB_NAMES[(int)v]);
        return 0;
    }

    bool is_stone     = has_aspect(e, ASP_STONE);
    bool is_loop      = has_aspect(e, ASP_LOOP);
    bool is_judgement = has_aspect(e, ASP_JUDGEMENT);
    bool is_echo      = has_aspect(e, ASP_ECHO);
    bool is_hunger    = has_aspect(e, ASP_HUNGER);

    switch (v) {

        /* ── REBUKE ── direct tension reduction */
        case VERB_REBUKE:
            if (is_stone) {
                log_msg("STONE holds. The Rebuke finds no purchase.");
                if (is_judgement) { p->ash += 5; if (p->ash>100) p->ash=100; }
                break;
            }
            switch (fuel) {
                case FUEL_GLIMMER: tension_dmg = 3; break;
                case FUEL_MEMORY:  tension_dmg = 4; break;
                case FUEL_ASH:     tension_dmg = 2; p->ash += 5; if (p->ash>100) p->ash=100; break;
                case FUEL_FLESH:   tension_dmg = 2; break;
                case FUEL_ITEM:    tension_dmg = 2; break;
                case FUEL_TURNS:   tension_dmg = 1; break;
                default: break;
            }
            if (has_aspect(e, ASP_SHAME)) tension_dmg++;
            log_msg("You REBUKE the %s. Tension -%d.", e->name, tension_dmg);
            break;

        /* ── FORGET ── strip an aspect */
        case VERB_FORGET:
            if (is_stone) {
                log_msg("STONE resists. You cannot erase what is immovable.");
                if (is_judgement) { p->ash += 5; if (p->ash>100) p->ash=100; }
                break;
            }
            if (is_loop && p->ash > 33) {
                log_msg("The LOOP reflects your FORGET. Ash +8.");
                p->ash += 8;
                if (p->ash > 100) p->ash = 100;
                break;
            }
            if (e->n_aspects > 0) {
                /* Forget the first aspect */
                const char *anames[] = {"","SHAME","LOOP","HUNGER","JUDGEMENT","STATIC","ECHO","STONE"};
                Aspect removed = e->aspects[0];
                remove_aspect(e, removed);
                log_msg("You erase %s from the %s.", anames[(int)removed], e->name);
            } else {
                log_msg("Nothing left to forget.");
            }
            break;

        /* ── ECHO ── reflect enemy's last action */
        case VERB_ECHO: {
            int echo_amt = e->last_tension_dealt > 0 ? e->last_tension_dealt : 1;
            if (is_echo) echo_amt++;
            if (p->ash > 33 && fuel == FUEL_ASH && (rand() % 4 == 0)) {
                log_msg("The ECHO turns on you. -%d Flesh.", echo_amt);
                p->hp -= echo_amt;
            } else {
                tension_dmg = echo_amt;
                log_msg("You ECHO the wound back. Tension -%d.", tension_dmg);
            }
            break;
        }

        /* ── MEND ── heal or purge ash */
        case VERB_MEND:
            switch (fuel) {
                case FUEL_GLIMMER: { int h=roll(1,6); p->hp+=h; log_msg("MEND: +%d Flesh.",h); break; }
                case FUEL_MEMORY:  { int h=roll(6,12); p->hp+=h; log_msg("MEND: +%d Flesh (deep).",h); break; }
                case FUEL_ASH:     p->ash-=20; if(p->ash<0)p->ash=0; p->hp+=5; log_msg("MEND: Ash -20, +5 Flesh."); break;
                case FUEL_FLESH:   p->ash-=10; if(p->ash<0)p->ash=0; log_msg("MEND: traded Flesh for clarity."); break;
                default:           p->hp+=2; log_msg("MEND: +2 Flesh."); break;
            }
            if (p->hp > p->max_hp) p->hp = p->max_hp;
            /* Husk: MEND deals 1 tension to prevent STONE softlock */
            if (p->husk) { tension_dmg = 1; log_msg("The MEND finds a crack. Tension -1."); }
            if (is_judgement) { p->ash+=3; if(p->ash>100)p->ash=100; log_msg("JUDGEMENT: Ash +3."); }
            break;

        /* ── CHANT ── multi-round Ash buffer */
        case VERB_CHANT:
            p->chanting    = true;
            p->chant_turns = (fuel == FUEL_TURNS) ? 5 : 3;
            log_msg("You begin a CHANT. Ash gain halved for %d rounds.", p->chant_turns);
            break;

        /* ── FEED ── satisfy HUNGER or bribe */
        case VERB_FEED:
            if (is_hunger) {
                *hunger_fed = true;
                tension_dmg = 1;
                log_msg("You FEED the %s. Its hunger stills.", e->name);
            } else if (!e->boss && (rand() % 5 == 0)) {
                e->sealed = true;
                log_msg("The %s considers your offering. It hesitates.", e->name);
            } else {
                log_msg("The %s has no use for your offering.", e->name);
            }
            break;

        /* ── SEAL ── lock it from acting */
        case VERB_SEAL:
            if (is_loop) {
                log_msg("The LOOP breaks the Seal immediately.");
                break;
            }
            if (p->ash >= 67 && (rand() % 3 == 0)) {
                log_msg("The Seal collapses under static. Ash +5.");
                p->ash += 5;
                if (p->ash > 100) p->ash = 100;
                break;
            }
            e->sealed = true;
            log_msg("You SEAL the %s. It cannot act this round.", e->name);
            break;

        /* ── LIE ── twist reality */
        case VERB_LIE: {
            int chance = 70 - (p->ash / 33) * 20;
            if (chance < 10) chance = 10;
            if ((rand() % 100) < chance) {
                tension_dmg = 2 + (is_echo ? 1 : 0);
                log_msg("Reality reshapes. Tension -%d.", tension_dmg);
            } else {
                p->ash += 8;
                if (p->ash > 100) p->ash = 100;
                log_msg("The LIE collapses. Ash +8.");
                if (is_echo) { p->ash+=4; if(p->ash>100)p->ash=100; log_msg("ECHO amplifies the failure."); }
            }
            break;
        }

        default: break;
    }

    e->last_tension_dealt = tension_dmg;
    return tension_dmg;
}

/* ─── Threshold events ────────────────────────────────────────────────── */
static void fire_threshold_event(Enemy *e) {
    Room *r = cur_room();
    switch (e->type) {
        case EN_ORGANELLE:
            log_msg("The Organelle divides. Two where there was one.");
            spawn_room_enemies(r, G.player.floor_num);
            break;
        case EN_BOTFLY:
            log_msg("The Botfly screams. Ash +5.");
            G.player.ash += 5;
            break;
        case EN_BLACKLUNG:
            log_msg("Static floods your mind. Ash +8.");
            G.player.ash += 8;
            break;
        case EN_SHADE:
            log_msg("The Shade loops your last action. Ash +6.");
            G.player.ash += 6;
            break;
        case EN_RAT:
            log_msg("The Rat calls out. Something skitters closer.");
            spawn_room_enemies(r, G.player.floor_num);
            break;
        case EN_TITHE:
            if (G.player.n_memories > 0) {
                G.player.n_memories--;
                log_msg("The Tithe takes a memory. It is simply gone.");
            } else {
                G.player.ash += 10;
                log_msg("The Tithe finds nothing. Ash +10.");
            }
            break;
        case EN_CENSOR:
            log_msg("The Censor seals the words. Ash +12.");
            G.player.ash += 12;
            break;
        case EN_DRONE:
            log_msg("The Drone glitches. The walls look wrong. Ash +8.");
            G.player.ash += 8;
            break;
        case EN_INSIDEOUT:
            if (e->n_aspects < 4) {
                e->aspects[e->n_aspects++] = ASP_ECHO;
                log_msg("The Inside-Out-Man transforms. Gains ECHO.");
            }
            break;
        case EN_TENKNIVES:
            log_msg("Ten Knives — phase 2. The blades begin to echo.");
            e->phase = 2;
            e->tension = e->max_tension;
            if (e->n_aspects < 4) e->aspects[e->n_aspects++] = ASP_ECHO;
            break;
        case EN_THRONE:
            log_msg("The Throne summons its servants.");
            spawn_room_enemies(r, G.player.floor_num);
            break;
        default: break;
    }
    if (G.player.ash > 100) G.player.ash = 100;
}

/* ─── Enemy panel action ──────────────────────────────────────────────── */
static void enemy_panel_act(Enemy *e) {
    Player *p = &G.player;
    int pressure = e->atk > 0 ? e->atk : 1;

    /* ASP_LOOP: reflect intent if player is tainted */
    if (has_aspect(e, ASP_LOOP) && p->ash > 33) {
        int gain = p->chanting ? 5 : 10;
        p->ash += gain;
        if (p->ash > 100) p->ash = 100;
        log_msg("The LOOP returns your intent. Ash +%d.", gain);
        return;
    }

    if (p->ash < 67) {
        int gain = p->chanting ? (pressure / 2 + 1) : pressure;
        p->ash += gain;
        if (p->ash > 100) p->ash = 100;
        log_msg("The %s presses. Ash +%d.", e->name, gain);
    } else {
        /* High Ash: enemy tears through directly into Flesh */
        int dmg = 1 + roll(0, pressure);
        p->hp -= dmg;
        log_msg("The %s tears through you. -%d Flesh.", e->name, dmg);
        if (p->hp <= 0) {
            if (p->death_save && !p->death_save_used) {
                p->hp = 1;
                p->death_save_used = true;
                log_msg("Something keeps you from dissolving.");
            } else {
                G.game_over = true;
            }
        }
    }
}

/* ─── Unwrite enemy ───────────────────────────────────────────────────── */
static void unwrite_enemy(Enemy *e) {
    e->alive  = false;
    e->hp     = 0;
    G.player.kills++;
    if (e->die_msg) log_msg("%s", e->die_msg);
    else log_msg("You unwrite the %s. It stops existing.", e->name);
    Room *r = cur_room();
    if (r->tiles[e->y][e->x] == T_FLOOR)
        r->tiles[e->y][e->x] = T_VISCERA;
}

/* ─── apply_ash_effects ───────────────────────────────────────────────── */
void apply_ash_effects(void) {
    Player *p = &G.player;
    if (p->ash >= 100) {
        p->ash  = 100;
        p->husk = true;
    } else {
        p->husk = false;
    }
    /* Husk slowly consumes Flesh even outside combat */
    if (!G.in_combat && p->husk) {
        p->hp--;
        if (p->hp <= 0) {
            if (p->death_save && !p->death_save_used) {
                p->hp = 1;
                p->death_save_used = true;
                log_msg("The Husk eats you alive. Barely holding on.");
            } else {
                G.game_over = true;
            }
        }
    }
}

/* ─── Main combat panel ───────────────────────────────────────────────── */
void enter_combat_panel(Enemy *e) {
    if (!e || !e->alive) return;
    Player *p = &G.player;

    G.in_combat    = true;
    G.combat_round = 0;

    /* Ensure tension is initialised */
    if (e->tension <= 0 && e->max_tension > 0) e->tension = e->max_tension;
    if (e->tension <= 0) e->tension = e->max_tension = 3;

    bool running = true;

    while (running && e->alive && !G.game_over) {
        G.combat_round++;
        bool hunger_fed = false;
        e->sealed = false;

        /* ASP_STATIC: random verb silenced this round */
        int silenced = -1;
        if (has_aspect(e, ASP_STATIC) && (rand() % 3 == 0))
            silenced = rand() % VERB_COUNT;

        int actions = 1 + (p->spd > 0 ? p->spd : 0);
        if (p->husk) actions = 1;

        for (int ac = 0; ac < actions && running && !G.game_over; ac++) {

            /* ── Step 1: choose verb ── */
            Verb chosen_verb = VERB_COUNT;
            bool fled = false;

            while (chosen_verb == VERB_COUNT && !fled && !G.game_over) {
                render_combat(e, silenced, 0, VERB_COUNT);
                int ch = getch();

                if (ch >= '1' && ch <= '8') {
                    Verb v = (Verb)(ch - '1');
                    if ((int)v == silenced) {
                        log_msg("Static. The thought won't form.");
                    } else if (p->husk && v != VERB_MEND && v != VERB_SEAL) {
                        log_msg("The Husk cannot remember how to %s.", VERB_NAMES[(int)v]);
                    } else {
                        chosen_verb = v;
                    }
                } else if (ch == 27) { /* ESC = flee */
                    int flee_chance = 60;
                    for (int ti = 0; ti < e->n_triggers; ti++)
                        if (e->triggers[ti] == TRIG_SCENT) flee_chance -= 20;
                    if ((rand() % 100) < flee_chance) {
                        log_msg("You slip away from the encounter.");
                        running = false;
                        fled = true;
                    } else {
                        log_msg("You cannot flee. The %s has your scent.", e->name);
                        p->ash += 5;
                        if (p->ash > 100) p->ash = 100;
                        enemy_panel_act(e);
                        if (G.game_over) break;
                    }
                }
            }
            if (!running || fled || G.game_over) break;

            /* ── Step 2: choose fuel ── */
            FuelType chosen_fuel = FUEL_COUNT;
            bool cancelled = false;

            while (chosen_fuel == FUEL_COUNT && !cancelled && !G.game_over) {
                render_combat(e, silenced, 1, chosen_verb);
                int ch = getch();
                switch (ch) {
                    case 'g': case 'G': chosen_fuel = FUEL_GLIMMER; break;
                    case 'm': case 'M': chosen_fuel = FUEL_MEMORY;  break;
                    case 'a': case 'A': chosen_fuel = FUEL_ASH;     break;
                    case 'f': case 'F': chosen_fuel = FUEL_FLESH;   break;
                    case 'i': case 'I': chosen_fuel = FUEL_ITEM;    break;
                    case 't': case 'T': chosen_fuel = FUEL_TURNS;   break;
                    case 27:            cancelled = true; break;
                    default: break;
                }
                if (chosen_fuel != FUEL_COUNT && !fuel_available(chosen_fuel)) {
                    log_msg("Not enough %s.", FUEL_NAMES[(int)chosen_fuel]);
                    chosen_fuel = FUEL_COUNT;
                }
            }
            if (cancelled) { ac--; continue; }
            if (G.game_over) break;

            /* ── Pre-resolve guard: FORGET with no aspects wastes fuel ── */
            if (chosen_verb == VERB_FORGET && e->n_aspects == 0) {
                log_msg("There is nothing left to erase.");
                ac--; continue;
            }

            /* ── Resolve ── */
            consume_fuel(chosen_fuel);
            int tdmg = do_resolve_verb(e, chosen_verb, chosen_fuel, &hunger_fed);
            e->tension -= tdmg;
            if (e->tension < 0) e->tension = 0;

            /* Threshold */
            if (!e->threshold_fired && e->tension > 0 && e->tension <= e->threshold) {
                e->threshold_fired = true;
                fire_threshold_event(e);
                if (G.game_over) break;
            }

            /* Death */
            if (e->tension <= 0) {
                unwrite_enemy(e);
                running = false;
                break;
            }
        }

        if (!running || !e->alive || G.game_over) break;

        /* ASP_HUNGER end-of-round growth */
        if (!hunger_fed && has_aspect(e, ASP_HUNGER)) {
            e->tension++;
            if (e->tension > e->max_tension) e->tension = e->max_tension;
            log_msg("The %s's hunger grows. Tension +1.", e->name);
        }

        /* Enemy acts */
        if (running && e->alive && !e->sealed && !G.game_over)
            enemy_panel_act(e);

        /* Chant tick */
        if (p->chanting) {
            p->chant_turns--;
            if (p->chant_turns <= 0) { p->chanting = false; p->chant_turns = 0; }
        }

        apply_ash_effects();
    }

    G.in_combat = false;
    check_room_clear();
}
