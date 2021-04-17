//
// Created by carson on 5/20/20.
//

#include "stdio.h"
#include "stdlib.h"
#include "server.h"
#include "char_buff.h"
#include "game.h"
#include "repl.h"
#include "pthread.h"
#include <string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h>    //inet_addr
#include <unistd.h>    //write*/
#include <netdb.h>

static game_server *SERVER;

static pthread_mutex_t lock;
static char_buff* inputs[2];

int init_server() {
    if (SERVER == NULL) {
        SERVER = calloc(1, sizeof(struct game_server));
        return 1;
    } else {
        printf("Server already started");
        return 0;
    }
}

void* handle_client_connect(void* args) {

    // STEP 8 - This is the big one: you will need to re-implement the REPL code from
    // the repl.c file, but with a twist: you need to make sure that a player only
    // fires when the game is initialized and it is there turn.  They can broadcast
    // a message whenever, but they can't just shoot unless it is their turn.
    //
    // The commands will obviously not take a player argument, as with the system level
    // REPL, but they will be similar: load, fire, etc.
    //
    // You must broadcast informative messages after each shot (HIT! or MISS!) and let
    // the player print out their current board state at any time.
    //
    // This function will end up looking a lot like repl_execute_command, except you will
    // be working against network sockets rather than standard out, and you will need
    // to coordinate turns via the game::status field.

    int player = *(int*)args;
    int socket = SERVER->player_sockets[player];
    
    free(args);

    int data_len = 1;
    char data[1024];

    char_buff* input = cb_create(1024);
    char_buff* output = cb_create(1024);

    inputs[player] = input;

    cb_reset(input);
    cb_reset(output);

    cb_append(output, "Welcome to the battleBit server Player ");
    cb_append_int(output, player);
    cb_append(output, "\r\n");

    server_broadcast(output, player);
    cb_reset(output);

    while (data_len > 0) {

        data_len = recv(socket, data, 1024, 0);

        if (data_len <= 0) continue;
        
        cb_append(input, data);
        send(socket, data, data_len, 0);

        char complete = 0;

        if (data_len > 0) { complete = data[data_len - 1] == '\n'; }
        memset(data, 0, data_len);

        if (complete == 1) {

            char* command = cb_tokenize(input, " \r\n");

            pthread_mutex_lock(&lock);

            struct game* g = game_get_current();

            if (strcmp(command, "exit") == 0) {
            
                pthread_mutex_unlock(&lock);
                break;
            }
                
            else if (strcmp(command, "?") == 0) {

                cb_append(output, "? - show help\r\n");
                cb_append(output, "load <string> - load a ship layout\r\n");
                cb_append(output, "show - shows the board\r\n");
                cb_append(output, "fire [0-7] [0-7] - fires at the given position\r\n");
                cb_append(output, "say <string> - Send the string to all players as part of a chat\r\n");
                cb_append(output, "exit - quit the server\r\n");

                server_send(socket, output);
            }

            else if (g->status == CREATED) {

                cb_append(output, "Waiting for second player...\r\n");

                server_send(socket, output);
            }
            
            else if (strcmp(command, "say") == 0) {

                cb_append(output, "Player ");
                cb_append_int(output, player);
                cb_append(output, " says:");

                char* token;

                while ((token = cb_next_token(input)) != 0) {
                
                    cb_append(output, " ");
                    cb_append(output, token);
                }
                    
                cb_append(output, "\r\n");

                server_broadcast(output, player);
            }

            else if (strcmp(command, "load") == 0) {

                if (g->status != INITIALIZED) {

                    cb_append(output, "Game already started, can not change board!\r\n");
                    server_send(socket, output);
                }

                else {

                    if (game_load_board(g, player, cb_next_token(input)) == -1) {
                    
                        cb_append(output, "Board layout incorrect!\r\n");
                        server_send(socket, output);
                    }

                    else {

                        if (g->players[player ? 0 : 1].ships != 0) {

                            cb_append(output, "All players loaded their ships, player 0 starts!\r\n");

                            g->status = PLAYER_0_TURN;
                            server_broadcast(output, player);
                        }

                        else {

                            cb_append(output, "Player ");
                            cb_append_int(output, player);
                            cb_append(output, " has loaded the ships.\r\n");

                            server_broadcast(output, player);
                        }
                    }
                }
            }

            else if (g->status == INITIALIZED) {

                cb_append(output, "Please wait for the game to start\r\n");
                server_send(socket, output);
            }

            else if (strcmp(command, "show") == 0) {

                repl_print_board(game_get_current(), player, output);
                server_send(socket, output);
            }

            else if (strcmp(command, "fire") == 0) {

                if ((player == 0 && g->status == PLAYER_1_TURN) ||
                    (player == 1 && g->status == PLAYER_0_TURN)) {

                    cb_append(output, "It is not your turn to fire!\r\n");
                    server_send(socket, output);
                }

                else {

                    int x = atoi(cb_next_token(input));
                    int y = atoi(cb_next_token(input));

                    if (x < 0 || x >= BOARD_DIMENSION || y < 0 || y >= BOARD_DIMENSION) {

                        cb_append(output, "Invalid coordinate: ");
                        cb_append_int(output, x);
                        cb_append(output, " ");
                        cb_append_int(output, y);
                        cb_append(output, "\r\n");

                        server_send(socket, output);
                    }

                    else {

                        int result = game_fire(game_get_current(), player, x, y);

                        cb_append(output, "Player: ");
                        cb_append_int(output, player);
                        cb_append(output, " fired at ");
                        cb_append_int(output, x);
                        cb_append(output, " ");
                        cb_append_int(output, y);
                        cb_append(output, ": ");

                        if (result) cb_append(output, "HIT!\r\n");
                        else cb_append(output, "Miss\r\n");

                        if (g->status == PLAYER_0_WINS || g->status == PLAYER_1_WINS) {

                            cb_append(output, "PLAYER ");
                            cb_append_int(output, player);
                            cb_append(output, "WINS!!\r\n");

                            g->status == INITIALIZED;
                        }

                        else {

                            if (g->status == PLAYER_0_TURN) {

                                g->status = PLAYER_1_TURN;
                                cb_append(output, "Player 1, it is your turn\r\n");
                            }

                            else {

                                g->status = PLAYER_0_TURN;
                                cb_append(output, "Player 0, it is your turn\r\n");
                            }
                        }

                        server_broadcast(output, player);
                    }
                }
            }

            else {

                cb_append(output, "Unknown command: ");
                cb_append(output, command);
                cb_append(output, "\r\n");
                server_send(socket, output);
            }

            cb_reset(output);
            cb_reset(input);
            

            pthread_mutex_unlock(&lock);
        }
    }

    cb_append(output, "Player ");
    cb_append_int(output, player);
    cb_append(output, " has left the game!");

    close(socket);
    socket = 0;

    server_broadcast(output, player);
}

void server_send(int socket, char_buff* msg) {

    cb_append(msg, "battleBit (? for help) > ");
    send(socket, msg->buffer, msg->size, 0);

}

void server_broadcast(char_buff *msg, int player) {

    cb_append(msg, "battleBit (? for help) > ");

    if (SERVER->player_sockets[0] != 0) {
    
        send(SERVER->player_sockets[0], msg->buffer, msg->size, 0);
        if (strlen(inputs[0]->buffer) > 0 && player) send(SERVER->player_sockets[0], inputs[0]->buffer, inputs[0]->size, 0);
    }
        
    if (SERVER->player_sockets[1] != 0) {
    
        send(SERVER->player_sockets[1], msg->buffer, msg->size, 0);
        if (strlen(inputs[1]->buffer) > 0 && !player) send(SERVER->player_sockets[1], inputs[1]->buffer, inputs[1]->size, 0);
    } 
}

void* run_server(void* ddd) {

    // STEP 7 - implement the server code to put this on the network.
    // Here you will need to initalize a server socket and wait for incoming connections.
    //
    // When a connection occurs, store the corresponding new client socket in the SERVER.player_sockets array
    // as the corresponding player position.
    //
    // You will then create a thread running handle_client_connect, passing the player number out
    // so they can interact with the server asynchronously

    struct sockaddr_in server;
    struct sockaddr_in client;
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

        perror("server socket: ");
        return;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(9876);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 8);

    if ((bind(sock, (struct sockaddr*)&server, sizeof(struct sockaddr))) == -1) {

        perror("bind : ");
        return;
    }

    if ((listen(sock, 2)) == -1) {
        
        perror("listen");
        return;
    }

    game_get_current()->status = CREATED;

    int player = 0;

    while (player < 2) {

        int addr_len;

        if ((SERVER->player_sockets[player] = accept(sock, (struct sockaddr*)&client, &addr_len)) == -1) {

            perror("accept");
            return;
        }

        int* args = malloc(sizeof(int));
        *args = player;

        pthread_create(&SERVER->player_threads[player++], NULL, handle_client_connect, args);
    }

    game_get_current()->status = INITIALIZED;
}

int server_start() {

    // STEP 6 - using a pthread, run the run_server() function asynchronously, so you can still
    // interact with the game via the command line REPL

    if (pthread_mutex_init(&lock, NULL) != 0) {

        printf("\nCould not create mutex\n");
        return -1;
    }

    if (!init_server()) return -1;

    int result = pthread_create(&SERVER->server_thread, NULL, run_server, NULL);

    printf("Server Created!\n");

    return 1;
}