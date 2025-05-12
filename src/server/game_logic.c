#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

//Feel free to add your own code. I stripped out most of our solution functions but I left some "breadcrumbs" for anyone lost

// for debugging
void print_game_state( game_state_t *game){
    (void) game;
}

void init_deck(card_t deck[DECK_SIZE], int seed){ //DO NOT TOUCH THIS FUNCTION
    srand(seed);
    int i = 0;
    for(int r = 0; r<13; r++){
        for(int s = 0; s<4; s++){
            deck[i++] = (r << SUITE_BITS) | s;
        }
    }
}

void shuffle_deck(card_t deck[DECK_SIZE]){ //DO NOT TOUCH THIS FUNCTION
    for(int i = 0; i<DECK_SIZE; i++){
        int j = rand() % DECK_SIZE;
        card_t temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

//You dont need to use this if you dont want, but we did.
void init_game_state(game_state_t *game, int starting_stack, int random_seed){
    memset(game, 0, sizeof(game_state_t));
    init_deck(game->deck, random_seed);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->player_stacks[i] = starting_stack;
    }

    game->next_card = 0;
    game->dealer_player = 0;
    game->highest_bet = 0;
    game->pot_size = 0;
    game->current_player = 0;
    game->dealer_player = 0;
    game->round_stage = ROUND_INIT;
    for(int i = 0; i < MAX_COMMUNITY_CARDS; i++) {
        game->community_cards[i] = NOCARD;
        game->current_bets[i] = 0;
    }
}

void reset_game_state(game_state_t *game) {
    //Call this function between hands.
    //You should add your own code, I just wanted to make sure the deck got shuffled.
    game->next_card = 0;
    game->highest_bet = 0;
    game->pot_size = 0;
    game->current_player = 0;
    game->round_stage = ROUND_INIT;
    for(int i = 0; i < MAX_COMMUNITY_CARDS; i++) {
        game->community_cards[i] = NOCARD;
    }

    for(int i = 0; i < MAX_PLAYERS; i++) {
        game->current_bets[i] = 0;
        game->player_hands[i][0] = NOCARD;
        game->player_hands[i][1] = NOCARD;
    }

    

    //find first dealer player
    int j = 1;
    for(int i = (game->dealer_player + 1) % MAX_PLAYERS; j < MAX_PLAYERS; i = (i+1) % MAX_PLAYERS) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->dealer_player = i;
            break;
        }
        j++;
    } 

     //find first player after dealer
    j = 1;
    for(int i = (game->dealer_player + 1) % MAX_PLAYERS; j < MAX_PLAYERS; i = (i+1) % MAX_PLAYERS) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->current_player = i;
            break;
        }
        j++;
    }
}

void server_join(game_state_t *game) {
    //This function was called to get the join packets from all players
    for(int i = 0; i < MAX_PLAYERS; i++) {
        client_packet_t packet;
        ssize_t bytes_read;
        
        if(game->sockets[i] == -1) continue;
        bytes_read = recv(game->sockets[i], &packet, sizeof(client_packet_t), 0);
        if(bytes_read <= 0) {
            continue;
        }

        if(packet.packet_type != JOIN) {
            printf("\nExpected join packet from player %d\n", i);
            game->player_status[i] = PLAYER_LEFT;
            close(game->sockets[i]);
        }
    }
}

int server_ready(game_state_t *game) {
    //This function updated the dealer and checked ready/leave status for all players
    for(int i = 0; i < MAX_PLAYERS; i++) {
        client_packet_t packet;
        ssize_t bytes_read = recv(game->sockets[i], &packet, sizeof(client_packet_t), 0);

        if(packet.packet_type == LEAVE) {
            close(game->sockets[i]);
            game->sockets[i] = -1;
            game->player_status[i] = PLAYER_LEFT;
            game->player_hands[i][0] = NOCARD;
            game->player_hands[i][1] = NOCARD;
            continue;
        } else if (bytes_read < 0) {
            close(game->sockets[i]);
            game->sockets[i] = -1;
            game->player_status[i] = PLAYER_LEFT;
            game->player_hands[i][0] = NOCARD;
            game->player_hands[i][1] = NOCARD;
            continue;
        }   else if (packet.packet_type == READY) {
                game->player_status[i] = PLAYER_ACTIVE;
                game->num_players++;
        }
    }

    //find first dealer player
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->dealer_player = i;
            break;
        }
    } 

     //find first player after dealer
    int j = 1;
    for(int i = (game->dealer_player + 1) % MAX_PLAYERS; j < MAX_PLAYERS; i = (i+1) % MAX_PLAYERS) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            game->current_player = i;
            break;
        }
        j++;
    }
    return 0;
}

//This was our dealing function with some of the code removed (I left the dealing so we have the same logic)
void server_deal(game_state_t *game) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->player_status[i] == PLAYER_ACTIVE) {
            game->player_hands[i][0] = game->deck[game->next_card++];
            game->player_hands[i][1] = game->deck[game->next_card++];
        }
    }
}

int server_bet(game_state_t *game) {
    //This was our function to determine if everyone has called or folded
    int active_count = 0;
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) active_count++;
    }

    int players_matched = 0; // num of players that have same bet
    int iterations = 0;
    
    //TODO: Find out about All in players and i guess skip over them for now when betting

    while(active_count > 1 && players_matched < active_count) {
        client_packet_t client_packet;
        ssize_t bytes_read = recv(game->sockets[game->current_player], &client_packet, sizeof(client_packet_t), 0);
        
        server_packet_t response_packet;
        memset(&response_packet, 0, sizeof(server_packet_t));

        switch (client_packet.packet_type) {
            case RAISE: {
                int requested_raise = client_packet.params[0];

                if(requested_raise + game->current_bets[game->current_player] > game->highest_bet 
                    && game->player_stacks[game->current_player] > requested_raise) {     

                    game->highest_bet = requested_raise + game->current_bets[game->current_player];
                    game->pot_size += requested_raise;
                    game->player_stacks[game->current_player] -= requested_raise;
                    game->current_bets[game->current_player] += requested_raise;
                    response_packet.packet_type = ACK;
                    players_matched = 1; //bc new bet has to be called to

                    if(game->player_stacks[game->current_player] == 0) {
                        game->player_status[game->current_player] = PLAYER_ALLIN;
                    }


                } else {
                    response_packet.packet_type = NACK;
                }
                break;
            }
            case CALL: {
                if(game->current_bets[game->current_player] < game->highest_bet) {
                    int call_amount = game->highest_bet - game->current_bets[game->current_player];
                    
                    if(game->player_stacks[game->current_player] > call_amount) {
                        game->player_stacks[game->current_player] -= call_amount;
                        game->current_bets[game->current_player] = game->highest_bet;
                        game->pot_size += call_amount;
                        response_packet.packet_type = ACK;
                        players_matched++;
                    } else {
                        int all_in_amount = game->player_stacks[game->current_player];
                        game->current_bets[game->current_player] += all_in_amount;
                        game->player_stacks[game->current_player] = 0;
                        game->player_status[game->current_player] = PLAYER_ALLIN;
                        game->pot_size += all_in_amount;
                        response_packet.packet_type = ACK;
                    }
                } else {
                    response_packet.packet_type = NACK;
                }
                break;
            }
            case CHECK:
                if(game->current_bets[game->current_player] == game->highest_bet) {
                    response_packet.packet_type = ACK;
                    players_matched++;
                } else {
                    response_packet.packet_type = NACK;
                }
                break;
                
            case FOLD: 
                if(active_count > 1) {
                    game->player_status[game->current_player] = PLAYER_FOLDED;
                    active_count--;
                    response_packet.packet_type = ACK;
                    // if(game->round_stage != ROUND_SHOWDOWN) {
                    //     game->player_hands[game->current_player][0] = NOCARD;
                    //     game->player_hands[game->current_player][1] = NOCARD;
                    // }
                } else {
                    response_packet.packet_type = NACK;
                }
                break;

            default: {
                response_packet.packet_type = NACK;
                break;
            }
        }

        //send the ACK/NACK packet
        send(game->sockets[game->current_player], &response_packet, sizeof(server_packet_t), 0);

        if(response_packet.packet_type == ACK) {
            //find next active player 
            int j = 0;
            for(int i = (game->current_player + 1) % MAX_PLAYERS; j < MAX_PLAYERS; i = (i+1) % MAX_PLAYERS) {
                if(game->player_status[i] == PLAYER_ACTIVE) {
                    game->current_player = i;
                    break;
                            }
                j++;
            }

            if(players_matched < active_count) {
                //resend info to everyone
                for(int i = 0; i < MAX_PLAYERS; i++) {
                    if(game->player_status[i] != PLAYER_LEFT) {
                        server_packet_t info_packet;
                        build_info_packet(game, i, &info_packet);
                        send(game->sockets[i], &info_packet, sizeof(server_packet_t), 0);
                    }
                }
            }

        } else { //if NACK, then relisten for packet
            continue;
        }
        
        if(check_betting_end(game) && iterations >= active_count) {
            int z = 1;
            //find first player after dealer again
            for(int i = (game->dealer_player + 1) % MAX_PLAYERS; z < MAX_PLAYERS; i = (i+1) % MAX_PLAYERS) {
                if(game->player_status[i] == PLAYER_ACTIVE) {
                    game->current_player = i;
                    break;
                }
                z++;
            }
            break;
        }
        iterations++;
    }
    
    if(active_count == 1) {
        game->round_stage = ROUND_SHOWDOWN;
    }
    return 0;
}

// Returns 1 if all bets are the same among active players
int check_betting_end(game_state_t *game) {
    int active_count = 0;
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            active_count++;
        }
    }
    if(active_count == 1) return 1;

    int highest_bet = game->highest_bet;
    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ACTIVE) {
            if(game->current_bets[i] != highest_bet) {
                return 0;
            }
        }
    }
    return 1; 
}

void server_community(game_state_t *game) {
    //This function checked the game state and dealt new community cards if needed;
    switch(game->round_stage) {
        case ROUND_FLOP:
            game->community_cards[0] = game->deck[game->next_card++];
            game->community_cards[1] = game->deck[game->next_card++];
            game->community_cards[2] = game->deck[game->next_card++];
            break;
        case ROUND_TURN:
            game->community_cards[3] = game->deck[game->next_card++];
            break;
        case ROUND_RIVER:
            game->community_cards[4] = game->deck[game->next_card++];
            break;
        default:
            break;
    }
}

void server_end(game_state_t *game) {
    //This function sends the end packet
    int ready_count = 0;
    int winner = find_winner(game);
    game->player_stacks[winner] += game->pot_size;

    //make and send new END packets
    server_packet_t end_packet;
    memset(&end_packet, 0, sizeof(server_packet_t));
    build_end_packet(game, winner, &end_packet);

    for(int i = 0; i < MAX_PLAYERS; i++) {
        if(game->player_status[i] != PLAYER_LEFT) {
            send(game->sockets[i], &end_packet, sizeof(server_packet_t), 0);
        }
    }

    //Reset game, recount and wait for ready / leave packets
    game->num_players = 0;
    for(int i = 0; i < MAX_PLAYERS; i++) {
        client_packet_t packet;
        ssize_t bytes_read = recv(game->sockets[i], &packet, sizeof(client_packet_t), 0);

        if(packet.packet_type == LEAVE) {
            close(game->sockets[i]);
            game->sockets[i] = -1;
            game->player_status[i] = PLAYER_LEFT;
            continue;
        } else if (bytes_read < 0) {
            close(game->sockets[i]);
            game->sockets[i] = -1;
            game->player_status[i] = PLAYER_LEFT;
            continue;
        } else if (packet.packet_type == READY) {
            game->player_status[i] = PLAYER_ACTIVE;
            game->num_players++;
        }
    }


    //HALT packet
    if(game->num_players < 2) {
        server_packet_t halt_packet;
        memset(&halt_packet, 0, sizeof(halt_packet));
        halt_packet.packet_type = HALT;
        for(int i = 0; i < MAX_PLAYERS; i++) {
            if(game->player_status[i] == PLAYER_ACTIVE) {
                send(game->sockets[i], &halt_packet, sizeof(halt_packet), 0);
                close(game->sockets[i]);
                break;
            }
        } 

        game->round_stage = ROUND_JOIN;
        //printf("\none player left, sent halt and closed connection.\n");
    }
}

int compare_cards(const void *a, const void *b) {
    card_t card1 = *(card_t *)a;
    card_t card2 = *(card_t *)b;

    int rank1 = RANK(card1);
    int rank2 = RANK(card2);

    if(rank1 != rank2) {
        return rank1 - rank2;
    }

    int suit1 = SUITE(card1);
    int suit2 = SUITE(card2);
    return suit1 - suit2;
}

int evaluate_hand(game_state_t *game, player_id_t pid) {
    //We wrote a function to compare a "value" for each players hand (to make comparison easier)
    //Feel free to not do this.
    int best_value = 0;

    //get the full hands
    card_t hand[7];
    hand[0] = game->player_hands[pid][0];
    hand[1] = game->player_hands[pid][1];
    for(int i = 0; i < 5; i++) {
        hand[2 + i] = game->community_cards[i];
    }

    //sort hand off rank
    qsort(hand, 7, sizeof(card_t), compare_cards);

    int suit_ranks[4][7] = {0}; //up to 7 cards in a suit
    int suit_counts[4] = {0}; //counts how mant of each suit we have

    for(int i = 0; i < 7; i++) {
        int suit = SUITE(hand[i]);
        int rank = RANK(hand[i]);
        suit_ranks[suit][suit_counts[suit]++] = rank;
    }

    //now time to check hand order 
    //STRAIGHT FLUSH
    for(int s = 0; s < 4; s++) {
        if(suit_counts[s] >=5) { //need 5 cards of same suit for straight flush
            int consecutive = 1;
            for(int j = 1; j < suit_counts[s]; j++) {
                if(suit_ranks[s][j] == suit_ranks[s][j-1] + 1) {
                    consecutive++;
                    if(consecutive == 5) {
                        return 30; //STRAIGHT FLUSH
                    }
                } else {
                    consecutive = 1;
                }
            }
            if(consecutive == 4 && suit_ranks[s][0] == TWO && suit_ranks[s][suit_counts[s] - 1] == ACE) {
                return 30; //STRAIGHT FLUSH
            }
        }
    }

    //4 of a kind
    int rank_count[13] = {0};
    //count ranks from player hand
    for(int i = 0; i < 2; i++) {
        rank_count[RANK(game->player_hands[pid][i])]++;
    }


    //count ranks from community
    for(int i = 0; i < MAX_COMMUNITY_CARDS; i++) {
        rank_count[RANK(game->community_cards[i])]++;
    }

    //check for four of a kind
    for(int r = 0; r < 13; r++) {
        if(rank_count[r] == 4) {
            return 29;
        }
    }

    //FULL HOUSE
    int three_of_a_kind = -1;
    int pair = -1;
    for(int r = 12; r >=0; r--) {
        if(rank_count[r] >= 3 && three_of_a_kind == -1) {
            three_of_a_kind = r;
            rank_count[r] -= 3;
            break;
        }
    }
    for(int r = 12; r >=0; r--) {
        if(rank_count[r] >= 2) {
            pair = r;
            break;
        }
    }
    if(three_of_a_kind != -1 && pair != -1) {
        return 28;
    } 
    //reset this to correct rank count if no full house
    if(three_of_a_kind != -1) {
        rank_count[three_of_a_kind] += 3;
    }

    //FLUSH
    for(int s = 0; s < 4; s++) {
        if(suit_counts[s] >= 5) {
            return 27;
        }
    }

    //THREE OF A KIND = 25
    for(int r = 0; r < 13; r++) {
        if(rank_count[r] >= 3) {
            return 25;
        }
    }


    //TWO PAIR = 24
    int pair_count = 0;
    for(int r = 0; r<13; r++) {
        if(rank_count[r] == 2) {
            pair_count++;
        }
    }
    if(pair_count >= 2) {
        return 24;
    }

    //ONE PAIR = 23
    for(int r = 0; r<13; r++) {
        if(rank_count[r] == 2) {
            return 23;
        }
    }

    //HIGH CARD = value of the card with RANK2 == 0 and ACE == 12
    int highest_rank = -1;
    for(int i = 0; i < 7; i++) {
        int rank = RANK(hand[i]);
        if(rank > highest_rank) {
            highest_rank = rank;
        }
    }
    return highest_rank;
}

int compare_hands(game_state_t *game, player_id_t pid1, player_id_t pid2) {
    card_t hand1[7], hand2[7];

    //player 1 hand
    hand1[0] = game->player_hands[pid1][0];
    hand1[1] = game->player_hands[pid1][1];
    for(int i = 0; i < 5; i++) {
        hand1[i + 2] = game->community_cards[i];
    }

    //player 2 hand
    hand2[0] = game->player_hands[pid2][0];
    hand2[1] = game->player_hands[pid2][1];
    for(int i = 0; i < 5; i++) {
        hand2[i + 2] = game->community_cards[i];
    }

    qsort(hand1, 7, sizeof(card_t), compare_cards);
    qsort(hand2, 7, sizeof(card_t), compare_cards);

    for(int i = 6; i >= 0; i--) {
        if(RANK(hand1[i]) != RANK(hand2[i])) {
            return RANK(hand1[i]) - RANK(hand2[i]);
        }
    }

    return 0;
}

int find_winner(game_state_t *game) {
    //We wrote this function that looks at the game state and returns the player id for the best 5 card hand.
    //STRAIGHT FLUSH = 30
    //4 OF A KIND = 29
    //FULL HOUSE = 28
    //FLUSH = 27
    //STRAIGHT = 26
    //THREE OF A KIND = 25
    //TWO PAIR = 24
    //ONE PAIR = 23
    //HIGH CARD = value of the card with RANK2 == 0 and ACE == 12

    int active_count = 0;
    for(int i = 0; i< MAX_PLAYERS; i++) {
        if(game->player_status[i] == PLAYER_ALLIN) {
            game->player_status[i] = PLAYER_ACTIVE;
        }
        if(game->player_status[i] == PLAYER_ACTIVE) {
            active_count++;
        }
    }
    if(active_count == 1) {
        for(int i = 0; i < MAX_PLAYERS; i++) {
            if(game->player_status[i] == PLAYER_ACTIVE) {
                return i;
            }
        }
    }


    int best_hand_value = -1;
    int winner = -1;
    int hand_values[6] = {-1};
    
    for(int i = 0; i < MAX_PLAYERS; i++) {

        if(game->player_status[i] == PLAYER_ACTIVE) {
            int hand_value = evaluate_hand(game, i);
            hand_values[i] = hand_value;

            if(hand_value > best_hand_value) {
                best_hand_value = hand_value;
                winner = i;
            } else if(hand_value == best_hand_value) {
                //tiebreaker
                int comparison = compare_hands(game, winner, i);
                if(comparison < 0) {
                    winner = i;
                }
            }
        }
    }
    return winner;
}