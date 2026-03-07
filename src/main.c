#include "game.h"

/* ─── Global state ──────────────────────────────────────────────────── */
Game G;

/* ─── Input: shoot mode ─────────────────────────────────────────────── */
static bool handle_shoot_mode(void) {
    log_msg("Shoot direction? [hjkl/arrows]");
    render_game();
    int ch = getch();
    int dx = 0, dy = 0;
    switch (ch) {
        case 'h': case KEY_LEFT:  dx=-1; break;
        case 'l': case KEY_RIGHT: dx= 1; break;
        case 'k': case KEY_UP:    dy=-1; break;
        case 'j': case KEY_DOWN:  dy= 1; break;
        case 'y': dx=-1; dy=-1; break;
        case 'u': dx= 1; dy=-1; break;
        case 'b': dx=-1; dy= 1; break;
        case 'n': dx= 1; dy= 1; break;
        default: log_msg("Cancelled."); return false;
    }
    return player_shoot(dx, dy);
}

/* ─── One turn ──────────────────────────────────────────────────────── */
static bool handle_input(int ch) {
    /* Returns true if a turn was taken */
    int dx = 0, dy = 0;
    switch (ch) {
        /* Movement */
        case 'h': case KEY_LEFT:  dx=-1; break;
        case 'l': case KEY_RIGHT: dx= 1; break;
        case 'k': case KEY_UP:    dy=-1; break;
        case 'j': case KEY_DOWN:  dy= 1; break;
        case 'y': dx=-1; dy=-1; break;
        case 'u': dx= 1; dy=-1; break;
        case 'b': dx=-1; dy= 1; break;
        case 'n': dx= 1; dy= 1; break;
        case '.': case ' ':   /* wait */
            G.player.turn++;
            return true;

        /* Shoot */
        case 'f': return handle_shoot_mode();

        /* Grenade */
        case 'e': player_use_grenade(); return false;

        /* Pick up */
        case 'g': case ',':
            pick_up_items();
            G.player.turn++;
            return true;

        /* Inventory */
        case 'i': {
            int sel = 0;
            render_inventory(&sel);
            return false;
        }

        /* Map */
        case 'm': case 'M':
            render_map();
            return false;

        /* Help */
        case '?':
            render_help();
            return false;

        /* Quit */
        case 'q': case 'Q':
            G.running = false;
            return false;

        default:
            return false;
    }

    if (dx != 0 || dy != 0) {
        return player_move(dx, dy);
    }
    return false;
}

/* ─── Main game loop ────────────────────────────────────────────────── */
static void game_loop(void) {
    G.running = true;
    G.game_over = false;
    G.won = false;

    while (G.running && !G.game_over) {
        render_game();
        int ch = getch();
        bool turned = handle_input(ch);

        if (turned) {
            /* Fast player: extra moves before enemies act */
            int bonus = G.player.spd;
            while (bonus > 0 && G.running && !G.game_over) {
                render_game();
                ch = getch();
                if (handle_input(ch)) bonus--;
            }

            /* Enemies act; slow player gives them extra turns */
            int enemy_turns = 1 + (G.player.spd < 0 ? -G.player.spd : 0);
            for (int t = 0; t < enemy_turns && !G.game_over; t++)
                update_enemies();

            do_regen();
            do_lantern();
            check_room_clear();
        }
    }
}

/* ─── New game ──────────────────────────────────────────────────────── */
static bool new_game(void) {
    /* Character select */
    int sel = render_char_select();
    if (sel < 0) return false;

    /* Init */
    memset(&G.floors, 0, sizeof(G.floors));
    memset(&G.log, 0, sizeof(G.log));
    G.n_projs = 0;

    init_player((CharType)sel);

    /* Generate floor 0 */
    generate_floor(&G.floors[0], 0);
    G.cf = &G.floors[0];
    enter_room(G.cf->start_room, -1);

    log_msg("The Tower stretches above you. Begin the climb.");
    log_msg("You are %s.", CHARS[sel].name);

    return true;
}

/* ─── Entry point ───────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned)time(NULL));
    render_init();

    while (1) {
        render_title();
        if (!new_game()) break;

        game_loop();

        if (G.won)
            render_win();
        else if (G.game_over)
            render_game_over();
        /* Loop: play again? title handles this */
    }

    render_cleanup();
    return 0;
}
