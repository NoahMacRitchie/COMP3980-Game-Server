
#include "game_state.h"

GameState *game_state_create(int client1, int client2, int type){
    GameState *gs;
   
    if(type == RPS){
        gs = malloc(sizeof(RPSGameState));
        RPSGameState * gs_RPS = (RPSGameState *)gs;
        gs_RPS->moves[PLAYER_ONE] = -1;
        gs_RPS->moves[PLAYER_TWO] = -1;
    } else if(type == TTT){
        gs = malloc(sizeof(TTTGameState));

        TTTGameState * gs_TTT = (TTTGameState *)gs;

        gs_TTT->move_count = 0;
        gs_TTT->current_player = client1; // PLAYER ONE IS X AND SHOULD START

        // Initialize empty board
        for (int i = 0; i < 9; i++)
        {
            gs_TTT->game_board[i] = '-';
        }
    } else {
        printf("INVALID GAME TYPE Exiting...");
        exit(EXIT_FAILURE);
    }

    gs->game_type = type;
    gs->player_fds[PLAYER_ONE] = client1;
    gs->player_fds[PLAYER_TWO] = client2;
    gs->voice_connected[PLAYER_ONE] = false;
    gs->voice_connected[PLAYER_TWO] = false;

    return gs;
}

void game_state_destroy(GameState *gs){
    if(gs->game_type == TTT){
        TTTGameState * gs_TTT = (TTTGameState *)gs;
        free(gs_TTT);
    } else if(gs->game_type == RPS){
        RPSGameState * gs_RPS = (RPSGameState *)gs;
        free(gs_RPS);
    } else {
        free(gs);
    }
    
}

bool set_active_game(Environment *env, int client){
    GameEnvironment * game_env = (GameEnvironment *) env;
    GameState ** states = game_env->gamestates;

    for (int i = 0; i < NUM_GAMES; i++)
    {
        GameState * gamestate = states[i];
        if (gamestate != NULL && (gamestate->player_fds[0] == client || gamestate->player_fds[1] == client)) {
            game_env->active_game_state_index = i;
            return true;
        }
    }

    printf("didnt find gamestate\n");
    return false;
}

int find_my_game_index(Environment *env, int my_fd){
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState **states = game_env->gamestates;
    for (int i = 0; i < NUM_GAMES; i++)
    {
        GameState *gs = states[i];

        if (gs != NULL && (gs->player_fds[0] == my_fd || gs->player_fds[1] == my_fd))
        {
            return i;
        }
    }
    return -1;
}

int add_new_client(Environment *env, int cfd, int game_type) {
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState **states = game_env->gamestates;

    // Check if client is trying to join a game while already being in a game
    printf("Check if client is trying to join a game while already being in a game\n");
    for (int i = 0; i < NUM_GAMES; i++)
    {
        GameState *gs = states[i];

        if (gs != NULL && (gs->player_fds[0] == cfd || gs->player_fds[1] == cfd))
        {
            return FAILED_TO_PLACE;
        }
    }
    // Check if there is a client waiting for a game
    printf("Check if there is a client waiting for a game\n");
    if(game_type == TTT){
    
        if (game_env->waiting_player_TTT == -1)
        {
            game_env->waiting_player_TTT = cfd;
            return PLACED_IN_QUEUE;
        }
    } else if(game_type == RPS){
        if (game_env->waiting_player_RPS == -1)
        {
            game_env->waiting_player_RPS = cfd;
            return PLACED_IN_QUEUE;
        }
    }
    // Create a game with waiting client and the new client
    printf("Create a game with waiting client and the new client\n");
    for (int i = 0; i < NUM_GAMES; i++)
    {
        if (states[i] == NULL)
        {
            printf("create new gamestate\n");
            GameState *new_gs;
            if(game_type == TTT){
                printf("creating TTT\n");
                new_gs = game_state_create(cfd, game_env->waiting_player_TTT, TTT);
                game_env->waiting_player_TTT = -1;
            } else if(game_type == RPS){
                printf("creating RPS\n");
                new_gs = game_state_create(cfd, game_env->waiting_player_RPS, RPS);
                game_env->waiting_player_RPS = -1;
            }
            
            game_env->gamestates[i] = new_gs;
            game_env->active_game_state_index = i;
            return PLACED_IN_GAME;
        }
    }

    return ALL_GAMES_FULL;
}

void remove_client_from_queue(Environment * env) {
    GameEnvironment * game_env = (GameEnvironment *) env;
    if(game_env->active_fd == game_env->waiting_player_RPS){
        printf("removed disconnected client from RPS queue\n");
        game_env->waiting_player_RPS = -1;
        close(game_env->waiting_player_RPS);
        FD_CLR(game_env->waiting_player_RPS, game_env->current_sockets);

    }
    if(game_env->active_fd == game_env->waiting_player_TTT){
        printf("removed disconnected client from TTT queue\n");
        game_env->waiting_player_TTT = -1;
        close(game_env->waiting_player_TTT);
        FD_CLR(game_env->waiting_player_TTT, game_env->current_sockets);
    }
}

void clean_game_states(Environment * env) {
    GameEnvironment * game_env = (GameEnvironment *) env;
    GameState ** states = game_env->gamestates;

    for (int i = 0; i < NUM_GAMES; i++) {
        GameState * gamestate = states[i];
        if (gamestate != NULL) {
            int player1 = gamestate->player_fds[PLAYER_ONE];
            int player2 = gamestate->player_fds[PLAYER_TWO];

            if (fcntl(player1, F_GETFD) == DEAD_SOCKET || fcntl(player2, F_GETFD) == DEAD_SOCKET) {
                close(player1);
                close(player2);

                FD_CLR(player1, game_env->current_sockets);
                FD_CLR(player2, game_env->current_sockets);

                game_state_destroy(gamestate);
                game_env->gamestates[i] = NULL;
            }
        }
    }
}