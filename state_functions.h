#ifndef TICTACTOE_STATE_H
#define TICTACTOE_STATE_H

#include <netinet/in.h>
#include <dcfsm/fsm.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum
{
    RECEIVE_PACKET = FSM_APP_STATE_START,
    ATTEMPTED_MOVE,
    STARTING_GAME,
    HANDLE_PACKET,
    CONFIRMATION_REQUEST,
    INFORMATION_REQUEST,
    META_REQUEST,
    GAME_ACTION_REQUEST,
    CONFIRM_RULESET,
    QUIT_GAME,
    MAKE_MOVE,
    JOIN_GAME,
    START_GAME,
    END_GAME,
} States;

#define BUF_SIZE 1024

int receive_packet(Environment *env);
int handle_packet(Environment *env);
int confirmation_request(Environment *env);
int information_request(Environment *env);
int meta_request(Environment *env);
int game_action_request(Environment *env);
int confirm_ruleset(Environment *env);
int quit_game(Environment *env);
int make_move(Environment *env);
int join_game(Environment *env);
int start_game(Environment *env);
int end_game(Environment *env);


#endif