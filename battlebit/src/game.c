//
// Created by carson on 5/20/20.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

// STEP 9 - Synchronization: the GAME structure will be accessed by both players interacting
// asynchronously with the server.  Therefore the data must be protected to avoid race conditions.
// Add the appropriate synchronization needed to ensure a clean battle.

static game * GAME = NULL;

void game_init() {

    if (GAME) free(GAME);

    GAME = malloc(sizeof(game));
    GAME->status = CREATED;
    game_init_player_info(&GAME->players[0]);
    game_init_player_info(&GAME->players[1]);
}

void game_init_player_info(player_info *player_info) {
    player_info->ships = 0;
    player_info->hits = 0;
    player_info->shots = 0;
}

int game_fire(game *game, int player, int x, int y) {

    // Step 5 - This is the crux of the game.  You are going to take a shot from the given player and
    // update all the bit values that store our game state.
    //
    //  - You will need up update the players 'shots' value
    //  - you You will need to see if the shot hits a ship in the opponents ships value.  If so, record a hit in the
    //    current players hits field
    //  - If the shot was a hit, you need to flip the ships value to 0 at that position for the opponents ships field
    //
    //  If the opponents ships value is 0, they have no remaining ships, and you should set the game state to
    //  PLAYER_1_WINS or PLAYER_2_WINS depending on who won.

    int opponent = player ? 0 : 1;
    unsigned long long bit = xy_to_bitval(x, y);

    game->players[player].shots |= bit;

    if (game->players[opponent].ships & bit) {
    
        game->players[player].hits |= bit;

        game->players[opponent].ships ^= bit;

        if (!game->players[opponent].ships) {
        
            if (opponent) game->status = PLAYER_0_WINS;
            else game->status = PLAYER_1_WINS;
        }

        return 1;
    }

    return 0;
}

unsigned long long int xy_to_bitval(int x, int y) {

    // Step 1 - implement this function.  We are taking an x, y position
    // and using bitwise operators, converting that to an unsigned long long
    // with a 1 in the position corresponding to that x, y
    //
    // x:0, y:0 == 0b00000...0001 (the one is in the first position)
    // x:1, y: 0 == 0b00000...10 (the one is in the second position)
    // ....
    // x:0, y: 1 == 0b100000000 (the one is in the eighth position)
    //
    // you will need to use bitwise operators and some math to produce the right
    // value.

    return (1ull << x) << (y * 8);
}

struct game * game_get_current() {
    return GAME;
}



int game_load_board(struct game *game, int player, char * spec) {

    // Step 2 - implement this function.  Here you are taking a C
    // string that represents a layout of ships, then testing
    // to see if it is a valid layout (no off-the-board positions
    // and no overlapping ships)
    //

    // if it is valid, you should write the corresponding unsigned
    // long long value into the Game->players[player].ships data
    // slot and return 1
    //
    // if it is invalid, you should return -1

    static const char ship_types[10] = { 'C', 'c', 'B', 'b', 'D', 'd', 'S', 's', 'P', 'p' };

    if (strlen(spec) != 15) return -1;

    player_info local_player;
    local_player.ships = 0;

    unsigned char used_ships = 0;

    for (unsigned char i = 0; i < 15; i += 3) {

        char ship = spec[i];
        char x = spec[i + 1];
        char y = spec[i + 2];

        char direction = -1;
        char length = -1;

        if (x < 48 || x > 55 || y < 48 || y > 55) return -1;

        x -= 48; y -= 48;

        for (int j = 0; j < 10; j++) {
        
            if (ship == ship_types[j]) {

                if (used_ships & 1 << (j / 2)) return -1;
                used_ships |= 1 << (j / 2);

                direction = j % 2;
                length = 5 - (j < 6 ? j / 2 : (j - 2) / 2);

                break;
            }
        }

        if (direction == -1) return -1;

        else if (direction == 0) {

            if (add_ship_horizontal(&local_player, x, y, length) == -1) return -1;
        }
            
        else if (add_ship_vertical(&local_player, x, y, length) == -1) return -1;
    }

    game->players[player].ships = local_player.ships;

    return 1;
}

int add_ship_horizontal(player_info *player, int x, int y, int length) {

    // implement this as part of Step 2
    // returns 1 if the ship can be added, -1 if not
    // hint: this can be defined recursively

    if (x + length > 7) return -1;

    for (unsigned char i = 0; i < length; i++) {

        unsigned long long bit = xy_to_bitval(x + i, y);

        if (player->ships & bit) return -1;
        else player->ships |= bit;
    }

    return 1;
}

int add_ship_vertical(player_info *player, int x, int y, int length) {

    // implement this as part of Step 2
    // returns 1 if the ship can be added, -1 if not
    // hint: this can be defined recursively

    if (y + length > 7) return -1;

    for (unsigned char i = 0; i < length; i++) {

        unsigned long long bit = xy_to_bitval(x, y + i);

        if (player->ships & bit) return -1;
        else player->ships |= bit;
    }

    return 1;
}