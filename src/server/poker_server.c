#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

#define BASE_PORT 2201
#define NUM_PORTS 6
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
} player_t;

game_state_t game; //global variable to store our game state info (this is a huge hint for you)

int main(int argc, char **argv) {
    int server_fds[NUM_PORTS], client_socket, player_count = 0;
    int opt = 1;
    struct sockaddr_in server_address;
    player_t players[MAX_PLAYERS];
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);

    //Setup the server infrastructre and accept the 6 players on ports 2201, 2202, 2203, 2204, 2205, 2206

    int rand_seed = argc == 2 ? atoi(argv[1]) : 0;
    init_game_state(&game, 100, rand_seed);
    shuffle_deck(game.deck);

    //Setup the server infrastructure and accept the 6 players on ports 2201, 2202, 2203, 2204, 2205, 2206
    for(int i = 0; i< NUM_PORTS; i++) {
        server_fds[i] = socket(AF_INET, SOCK_STREAM, 0); // create new socket
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        memset(&server_address, 0, addrlen); //change the server address
        server_address.sin_port = htons(BASE_PORT + i);
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;

        bind(server_fds[i], (struct sockaddr *)&server_address, addrlen);
        listen(server_fds[i], 1);
    }

    for(int i =0; i < NUM_PORTS; i++) {
        client_socket = accept(server_fds[i], (struct sockaddr*)&players[player_count].address, &addrlen);
        if(client_socket < 0) {
            perror("accept failed");
            continue;
        }
        
        players[player_count].socket = client_socket;
        game.sockets[player_count] = client_socket;
        player_count++;
    }


    //Join state?
    server_join(&game);

    //READY
    server_ready(&game);

    while (1) {
        
        server_deal(&game);       
        
        // PREFLOP BETTING
        game.round_stage = ROUND_PREFLOP;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game.player_status[i] != PLAYER_LEFT) {
                server_packet_t info_packet;
                build_info_packet(&game, i, &info_packet);
                send(game.sockets[i], &info_packet, sizeof(server_packet_t), 0);
            }
        }
        server_bet(&game);

        // PLACE FLOP CARDS
        if(game.round_stage != ROUND_SHOWDOWN) {
            game.round_stage = ROUND_FLOP;
            server_community(&game);
        }
        
        //round is set to showdown early if only one player ready

        // FLOP BETTING
        for(int i = 0; i < MAX_PLAYERS; i++) {
            game.current_bets[i] = 0; //Reset bets after PREFLOP
        }
        if(game.round_stage != ROUND_SHOWDOWN) {
            for (int i = 0; i < MAX_PLAYERS; i++) {

                if (game.player_status[i] != PLAYER_LEFT) {
                    server_packet_t info_packet;
                    build_info_packet(&game, i, &info_packet);
                    send(game.sockets[i], &info_packet, sizeof(server_packet_t), 0);
                }
            }
            game.highest_bet = 0;
            server_bet(&game);
        }

        // PLACE TURN CARDS
        if(game.round_stage != ROUND_SHOWDOWN) {
            game.round_stage = ROUND_TURN;
            server_community(&game);
        }

        // TURN BETTING
        for(int i = 0; i < MAX_PLAYERS; i++) {
            game.current_bets[i] = 0; //Reset bets after FLOP
        }
        if(game.round_stage != ROUND_SHOWDOWN) {
            for (int i = 0; i < MAX_PLAYERS; i++) {

                if (game.player_status[i] != PLAYER_LEFT) {
                    server_packet_t info_packet;
                    build_info_packet(&game, i, &info_packet);
                    send(game.sockets[i], &info_packet, sizeof(server_packet_t), 0);
                }

            }
            game.highest_bet = 0;
            server_bet(&game);
        }

        // PLACE RIVER CARDS
        if(game.round_stage != ROUND_SHOWDOWN) {
            game.round_stage = ROUND_RIVER;
            server_community(&game);
        }

        // RIVER BETTING
        for(int i = 0; i < MAX_PLAYERS; i++) {
            game.current_bets[i] = 0; //Reset bets after TURN
        }
        if(game.round_stage != ROUND_SHOWDOWN) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game.player_status[i] != PLAYER_LEFT) {
                    server_packet_t info_packet;
                    build_info_packet(&game, i, &info_packet);
                    send(game.sockets[i], &info_packet, sizeof(server_packet_t), 0);
                }

                game.current_bets[i] = 0; //Reset bets after RIVER
            }

            game.highest_bet = 0;
            server_bet(&game);
        }

        // ROUND_SHOWDOWN
        for(int i = 0; i < MAX_PLAYERS; i++) {
            game.current_bets[i] = 0; //Reset bets before end
        }
        game.round_stage = ROUND_SHOWDOWN;
        server_end(&game);

        if(game.round_stage == ROUND_JOIN) {
            game.round_stage = ROUND_SHOWDOWN;
            break; //server end sets this to round join if halt packet sent
        }

        //reset game if playing again
        reset_game_state(&game);
        shuffle_deck(game.deck);
    }

    printf("[Server] Shutting down.\n");

    // Close all fds (you're welcome)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(server_fds[i]);
        if (game.player_status[i] != PLAYER_LEFT) {
            close(game.sockets[i]);
        }
    }

    return 0;
}