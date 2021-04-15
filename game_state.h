#include <dcfsm/fsm.h>
#include <stdlib.h>
#include <stdio.h>
#include <dcfsm/fsm.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#define NUM_GAMES 20
#define TTT 1
#define RPS 2

#define FAILED_TO_PLACE 0
#define PLACED_IN_QUEUE 1
#define PLACED_IN_GAME 2
#define ALL_GAMES_FULL 3


#define DEAD_SOCKET -1
#define EMPTY_QUEUE -1

#define PLAYER_ONE 0
#define PLAYER_TWO 1

#define VALID 1
#define INVALID 0

typedef struct
{
    int game_type;
    int player_fds[2];
    struct sockaddr_in player_addresses[2];
    bool voice_connected[2];
} GameState;


typedef struct
{
    GameState game_state;
    int move_count;
    int winning_player;
    int current_player;
    uint8_t last_cell;
    char game_board[9];
} TTTGameState;

typedef struct
{
    GameState game_state;
    char moves [2];
    int winning_player;
} RPSGameState;


typedef struct
{
    Environment common;

    int server_fd;

    int waiting_player_TTT;
    int waiting_player_RPS;
    
    char received_packet[1024];
    ssize_t received_packet_len;

    int current_max_fd;
    fd_set * current_sockets;

    int active_fd;
    int active_game_state_index;
    GameState *gamestates[NUM_GAMES];
   
} GameEnvironment;

GameState *game_state_create(int client1, int client2, int type);
void game_state_destroy(GameState *gs);
bool set_active_game(Environment *env, int client);
int add_new_client(Environment *env, int cfd, int game_type);
void remove_client_from_queue(Environment * env);
void clean_game_states(Environment * env);
int find_my_game_index(Environment *env, int my_fd);