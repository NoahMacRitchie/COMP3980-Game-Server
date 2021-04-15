#include "./state_functions.h"
#include "./game_state.h"
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BUF_SIZE 1024
#define REQ_TYPE_BYTE_INDEX 4
#define CONTEXT_BYTE_INDEX 5
#define PAYLOAD_LEN_INDEX 6
#define PAYLOAD_FIRST_BTYE_INDEX 7
#define PAYLOAD_SECOND_BTYE_INDEX 8

#define PROTOCOL_VERSION 1
#define TTT 1
#define RPS 2

#define SUCCESS_STATUS_CODE 10
#define UPDATE_STATUS_CODE 20

#define CLIENT_INVALID_REQUEST 30
#define CLIENT_INVALID_UID 31
#define CLIENT_INVALID_TYPE 32
#define CLIENT_INVALID_CONTEXT 33
#define CLIENT_INVALID_PAYLOAD 34

#define SERVER_ERROR 30

#define GAME_INVALID_ACTION 50
#define GAME_ACTION_OUT_OF_TURN 51

#define CONFIRMATION_VALUE 1
#define INFORMATION_VALUE 1
#define META_ACTION_VALUE 1
#define GAME_ACTION_VALUE 1

#define TEAM_X 1
#define TEAM_O 2

#define ROCK 1
#define PAPER 2
#define SCISSORS 3

#define WIN 1
#define LOSS 2
#define TIE 3
#define GAME_CONTINUES 4


#define START_GAME_UPDATE_VALUE 1
#define MOVE_MADE_UPDATE_VALUE 2
#define END_OF_GAME_UPDATE_VALUE 3

#define CONFIRM_RULESET_VALUE 1
#define MAKE_MOVE_VALUE 1
#define QUIT_VALUE 1


void success_msg(int fd, uint8_t req_type, uint8_t payload_size, uint8_t * payload){
    ssize_t success_msg_size = 3 + payload_size;
    uint8_t success[success_msg_size];

    success[0] = SUCCESS_STATUS_CODE;
    success[1] = req_type;
    success[2] = payload_size;
    if(payload_size != 0){
        memcpy(success + 3, payload, payload_size);
    }
    printf("success 2: ");
    for (int i = 0; i < success_msg_size; i++) {
        printf("%u ", success[i]);
    }
    printf("\n");
    printf("Writing success to %d\n", fd);
    write(fd, success, success_msg_size);
}

void update_msg(int fd, uint8_t context, uint8_t payload_size, uint8_t* payload){
    ssize_t update_msg_size = 3 + payload_size;
    uint8_t update[update_msg_size];

    update[0]= UPDATE_STATUS_CODE;
    update[1]= context;
    update[2] = payload_size;

    printf("\n");
    if(payload_size != 0){
        memcpy(update + 3, payload, payload_size);
    }
    printf("update : ");
    for (int i = 0; i < update_msg_size; i++) {
        printf("%u ", update[i]);
    }
    printf("Writing update to %d\n", fd);
    write(fd, update, update_msg_size);
}

void error_msg(int fd, uint8_t error_code, uint8_t req_type ){
    uint8_t payload_size = 0;
    ssize_t error_msg_size = 3;
    uint8_t error[error_msg_size];

    error[0]= error_code;
    error[1]= req_type;
    error[2] = payload_size;
    printf("Writing error to %d\n", fd);
    write(fd, error, error_msg_size);
}

void fd_to_uid(uint8_t* uid_dest, int fd) {
    uint32_t uid = fd;
    uid_dest[0] = (uid >> 24) & 0xFF;
    uid_dest[1] = (uid >> 16) & 0xFF;
    uid_dest[2] = (uid >> 8) & 0xFF;
    uid_dest[3] = uid & 0xFF;
    printf("fd_to_uid = %u %u %u %u\n", uid_dest[0], uid_dest[1], uid_dest[2], uid_dest[3]);
}

void accept_connection(Environment * env)
{
    GameEnvironment * game_env = (GameEnvironment *)env;
    int server = game_env->server_fd;
    int current_max_fd = game_env->current_max_fd;

    int client = accept(server, NULL, NULL);
    printf("accepted client %d\n", client);
    FD_SET(client, game_env->current_sockets);

    if (client > current_max_fd && client < FD_SETSIZE) {
        game_env->current_max_fd = client;
    }
}

int handle_connection(Environment * env, int client)
{
    GameEnvironment *game_env = (GameEnvironment *)env;

    ssize_t num_read;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    num_read = read(client, buf, BUF_SIZE);
    game_env->received_packet_len = num_read;
    game_env->active_fd = client;
    // Assume client disconnected
    if(num_read == 0){
        FD_CLR(client, game_env->current_sockets); 
        close(client);
        remove_client_from_queue(env);
        clean_game_states(env);
        return RECEIVE_PACKET;
    }
    printf("num_read: %ld buf:  \n", num_read);
    for (int i = 0; i < num_read; i++) {
        printf("%d ", buf[i]);
    }
    printf("\n");
    memset(game_env->received_packet, '\0', num_read);
    memcpy(game_env->received_packet, buf, num_read);

    printf("buf2: \n");
    for (int i = 0; i < num_read; i++) {
        printf("%d \n", game_env->received_packet[i]);
    }
    printf("\n");
    return HANDLE_PACKET;
    // printf("game env -> received packet: %d,%d,%d,%d,%d,%d\n",game_env->received_packet[0],game_env->received_packet[1],game_env->received_packet[2],game_env->received_packet[3],game_env->received_packet[4],game_env->received_packet[5]);
}
int receive_packet(Environment *env)
{
    GameEnvironment * game_env = (GameEnvironment *) env;
    printf("ENTERED STATE: receive_packet\n");
    printf("active fd: %d, waitingRPS: %d, waitingTTT: %d\n",game_env->active_fd, game_env->waiting_player_RPS, game_env->waiting_player_TTT);
    // printf("sleeping...\n");
    // sleep(1);
    

    int server_fd = game_env->server_fd;
    fd_set ready_sockets = *game_env->current_sockets;

    int current_max_fd = game_env->current_max_fd;

    while (true)
    {
        fd_set ready_sockets = *game_env->current_sockets;

        if (select(current_max_fd, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("select");
        }

        for (int i = 0; i < current_max_fd; i++)
        {
            if (FD_ISSET(i, &ready_sockets))
            {
                if (i == server_fd)
                {
                    accept_connection(env);
                    return RECEIVE_PACKET;
                }
                else
                {
                    return handle_connection(env, i);
                }
            }
        }
    }
}


int handle_packet(Environment *env)
{
    printf("ENTERED STATE: handle_packet\n");
    GameEnvironment *game_env = (GameEnvironment *)env;

    int client = game_env->active_fd;
    char *received_packet = game_env->received_packet;
    switch(received_packet[REQ_TYPE_BYTE_INDEX]){
        case 1:  return CONFIRMATION_REQUEST;
        case 2:  return INFORMATION_REQUEST;
        case 3:  return META_REQUEST;
        case 4:  return GAME_ACTION_REQUEST;
        error_msg(client,CLIENT_INVALID_TYPE,received_packet[REQ_TYPE_BYTE_INDEX]);
        default: return RECEIVE_PACKET;


    }


    // bool has_game = set_active_game(env, client);

    // if (receive_packet[0] == 'J' && receive_packet[1] == 'G' && !has_game)
    // {
    //     if (game_env->waiting_player == EMPTY_QUEUE)
    //     {
    //         game_env->waiting_player = client;
    //         return RECEIVE_PACKET;
    //     }
    //     return STARTING_GAME;
    // }

    // if (receive_packet[0] == 'A' && has_game)
    // {

    //     return ATTEMPTED_MOVE;
    // }

    // return ERROR_STATE;
}

int confirmation_request(Environment *env){
    printf("ENTERED STATE: confirmation_request\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    if(received_packet[CONTEXT_BYTE_INDEX] == CONFIRM_RULESET_VALUE){
        return CONFIRM_RULESET;
    }

    error_msg(game_env->active_fd, CLIENT_INVALID_CONTEXT ,received_packet[REQ_TYPE_BYTE_INDEX]);
    printf("SEND invalid context error\n");
    return RECEIVE_PACKET;
    

}
int information_request(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    printf("ENTERED STATE: information_request\n");
    printf("NO information requests are supported at this time\n");

    error_msg(game_env->active_fd, CLIENT_INVALID_REQUEST,received_packet[REQ_TYPE_BYTE_INDEX]);
    return RECEIVE_PACKET;

}
int meta_request(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    if(received_packet[CONTEXT_BYTE_INDEX] == QUIT_VALUE){
        return QUIT_GAME;
    }

    error_msg(game_env->active_fd, CLIENT_INVALID_CONTEXT,received_packet[REQ_TYPE_BYTE_INDEX]);
    return RECEIVE_PACKET;

}
int game_action_request(Environment *env){
    printf("ENTERED STATE: game_action_request\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;

    if(received_packet[CONTEXT_BYTE_INDEX] == MAKE_MOVE_VALUE){
        return MAKE_MOVE;
    }

    error_msg(game_env->active_fd, CLIENT_INVALID_CONTEXT,received_packet[REQ_TYPE_BYTE_INDEX]);
    return RECEIVE_PACKET;

}

int confirm_ruleset(Environment *env){
    printf("ENTERED STATE: confirm_ruleset\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    ssize_t packet_len = game_env->received_packet_len;
    if(packet_len != 9){
        printf("Invalid confirm_ruleset message length\n");
        error_msg(game_env->active_fd, CLIENT_INVALID_REQUEST,received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }
    if(received_packet[PAYLOAD_FIRST_BTYE_INDEX] != 1){
        printf("Invalid protocol \n");
    }

    if(received_packet[PAYLOAD_SECOND_BTYE_INDEX] != TTT &&
        received_packet[PAYLOAD_SECOND_BTYE_INDEX] != RPS){
        printf("Invalid protocol \n");
        error_msg(game_env->active_fd, CLIENT_INVALID_PAYLOAD,received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }
    printf("Packet is good. Should join game, then start game\n");
    return JOIN_GAME;
}


int join_game(Environment *env){
    printf("ENTERED STATE: join_game\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    int placement_status;
    int active_fd = game_env->active_fd;
    if(received_packet[PAYLOAD_SECOND_BTYE_INDEX] == TTT){
        placement_status = add_new_client(env, game_env->active_fd, TTT);
    } else if(received_packet[PAYLOAD_SECOND_BTYE_INDEX] == RPS){
        placement_status = add_new_client(env, game_env->active_fd, RPS);
    }

    if (placement_status == FAILED_TO_PLACE){
        error_msg(game_env->active_fd, CLIENT_INVALID_REQUEST,received_packet[REQ_TYPE_BYTE_INDEX]);
    } else if(placement_status == PLACED_IN_QUEUE){
        printf("Client id:%d placed in queue\n", game_env->active_fd);

        uint8_t uid[4];
        fd_to_uid(uid, active_fd);
        success_msg(active_fd, CONFIRMATION_VALUE, 4, uid);
    } else if(placement_status == PLACED_IN_GAME){
        printf("Client id:%d placed in game\n", game_env->active_fd);

        uint8_t uid[4];
        fd_to_uid(uid, active_fd);
        success_msg(active_fd, CONFIRMATION_VALUE, 4, uid);
        set_active_game(env, game_env->active_fd);

        return START_GAME;
    } else if(placement_status == ALL_GAMES_FULL){
        error_msg(game_env->active_fd, SERVER_ERROR,received_packet[REQ_TYPE_BYTE_INDEX]);
    }
    return RECEIVE_PACKET;
}



int start_game_TTT(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    TTTGameState *gs =  (TTTGameState *) game_env->gamestates[game_env->active_game_state_index];
    int player1 = gs->game_state.player_fds[PLAYER_ONE];
    int player2 = gs->game_state.player_fds[PLAYER_TWO];
    uint8_t player1_payload[]  = {TEAM_X};
    uint8_t player2_payload[]  = {TEAM_O};

    update_msg(player1, START_GAME_UPDATE_VALUE, 1, player1_payload);
    update_msg(player2, START_GAME_UPDATE_VALUE, 1, player2_payload);    
    return RECEIVE_PACKET;
}

int start_game_RPS(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    RPSGameState *gs =  (RPSGameState *) game_env->gamestates[game_env->active_game_state_index];
   
    int player1 = gs->game_state.player_fds[PLAYER_ONE];
    int player2 = gs->game_state.player_fds[PLAYER_TWO];
    char *payload; 

    update_msg(player1, START_GAME_UPDATE_VALUE, 0, payload);
    update_msg(player2, START_GAME_UPDATE_VALUE, 0, payload);
    return RECEIVE_PACKET;
}

int start_game(Environment *env){
    printf("ENTERED STATE: start_game\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState *gs =  game_env->gamestates[game_env->active_game_state_index];
    printf("active index:%d\n",game_env->active_game_state_index);
    printf("game_type:%d\n", gs->game_type);
    if(gs->game_type == TTT){
        return start_game_TTT(env);
    } else if(gs->game_type == RPS){
        return start_game_RPS(env);
    }
    printf("Invalid game type while starting game, exiting...\n");
    exit(EXIT_FAILURE);
}

int quit_game(Environment *env){
    printf("ENTERED STATE: quit_game\n");

}
bool is_end_game_RPS(Environment * env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState *gs = game_env->gamestates[game_env->active_game_state_index];
    int player_num = gs->player_fds[PLAYER_ONE] == game_env->active_fd ? PLAYER_ONE : PLAYER_TWO;
    RPSGameState * gs_RPS = (RPSGameState *)gs;

    if(gs_RPS->moves[PLAYER_ONE] != -1 && gs_RPS->moves[PLAYER_TWO != -1]){
        return true;
    }
    return false;

}


int make_move_RPS(Environment *env){
    printf("parsing RPS move...\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;

    if(received_packet[PAYLOAD_LEN_INDEX] != 1){
        error_msg(game_env->active_fd, CLIENT_INVALID_PAYLOAD, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }
    uint8_t attempted_move = received_packet[PAYLOAD_FIRST_BTYE_INDEX];

    if(attempted_move != ROCK 
    && attempted_move != PAPER
    && attempted_move!= SCISSORS){
        error_msg(game_env->active_fd, CLIENT_INVALID_PAYLOAD, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }

    GameState *gs = game_env->gamestates[game_env->active_game_state_index];
    int player_num = gs->player_fds[PLAYER_ONE] == game_env->active_fd ? PLAYER_ONE : PLAYER_TWO;
    RPSGameState * gs_RPS = (RPSGameState *)gs;

    if(gs_RPS->moves[player_num] != -1){
        printf("Client has already sent a move sending GAME_ACTION_OUT_OF_TURN...\n");
        error_msg(game_env->active_fd, GAME_ACTION_OUT_OF_TURN, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    } 
    gs_RPS->moves[player_num] = attempted_move;

    uint8_t *payload;
    success_msg(game_env->active_fd, GAME_ACTION_VALUE, 0 , payload);

    if(is_end_game_RPS(env)){

        return END_GAME;
    }
    return RECEIVE_PACKET;
}

bool make_move_if_valid_TTT(GameEnvironment *game_env)
{
    char PLAYER_SYMBOLS[2] = {'X', 'O'};
    GameState *gs = game_env->gamestates[game_env->active_game_state_index];
    TTTGameState * gs_TTT = (TTTGameState *)gs;
    char *received_packet = game_env->received_packet;
    int current_player_fd = gs_TTT->current_player;

    char *board = gs_TTT->game_board;

    uint8_t cell = received_packet[PAYLOAD_FIRST_BTYE_INDEX];
    
    if (board[cell] != '-') {
        printf("INVALID: board space not empty\n");
        error_msg(game_env->active_fd, GAME_INVALID_ACTION, received_packet[REQ_TYPE_BYTE_INDEX]);
        return false;
    }
    int player_num = gs->player_fds[PLAYER_ONE] == game_env->active_fd ? PLAYER_ONE : PLAYER_TWO;
    board[cell] = PLAYER_SYMBOLS[player_num];
    gs_TTT->move_count++;
    gs_TTT->last_cell = cell;
    int opponent = gs_TTT->game_state.player_fds[(player_num + 1) % 2];
    gs_TTT->current_player = opponent;
    return true;

}

int is_end_game_TTT(Environment * env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState *gs = game_env->gamestates[game_env->active_game_state_index];
    TTTGameState * gs_TTT = (TTTGameState *)gs;

   
    int move_count = gs_TTT->move_count;

    char WINNING_MOVES[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
    char PLAYER_SYMBOLS[2] = {'X', 'O'};
    char *board = gs_TTT->game_board;

    int player_num = gs->player_fds[PLAYER_ONE] == game_env->active_fd ? PLAYER_ONE : PLAYER_TWO;
    char player_char = PLAYER_SYMBOLS[player_num];
    int moves_in_a_row = 0;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (board[WINNING_MOVES[i][j]] == player_char)
            {
                moves_in_a_row++;
            }
        }
        if (moves_in_a_row == 3)
        {
            return WIN;
        }
        moves_in_a_row = 0;
    }

    //check move count 9
    if (move_count == 9)
    {
        return TIE;
    }

   
    return GAME_CONTINUES;

}

void print_board(char *board)
{
    for (int i = 0; i < 9; i++)
    {
        if (i == 3 | i == 6)
            printf("\n");
        printf("%c", board[i]);
    }
    printf("\n");
}

int make_move_TTT(Environment *env){
    printf("parsing TTT move...\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;

    if(received_packet[PAYLOAD_LEN_INDEX] != 1){
        error_msg(game_env->active_fd, CLIENT_INVALID_PAYLOAD, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }

    uint8_t attempted_move = received_packet[PAYLOAD_FIRST_BTYE_INDEX];

    if(attempted_move < 0 || attempted_move > 8){
        error_msg(game_env->active_fd, CLIENT_INVALID_PAYLOAD, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }

    GameState *gs = game_env->gamestates[game_env->active_game_state_index];
    int player_num = gs->player_fds[PLAYER_ONE] == game_env->active_fd ? PLAYER_ONE : PLAYER_TWO;
    TTTGameState * gs_TTT = (TTTGameState *)gs;

    if(gs_TTT->current_player != game_env->active_fd){
        printf("CLient is not current player sending GAME_ACTION_OUT_OF_TURN...\n");
        error_msg(game_env->active_fd, GAME_ACTION_OUT_OF_TURN, received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    } 
    bool made_move = make_move_if_valid_TTT(game_env);

    if(made_move == false) return RECEIVE_PACKET;

    char * success_payload;

    success_msg(game_env->active_fd, GAME_ACTION_VALUE, 0 , success_payload);
    //update?
    int end_game_status = is_end_game_TTT(env);
    int current_player = game_env->active_fd;
    int opponent = gs_TTT->game_state.player_fds[(player_num + 1) % 2];
    uint8_t cell = gs_TTT->last_cell;
    print_board(gs_TTT->game_board);

    if(end_game_status == GAME_CONTINUES){
        uint8_t payload[1];
        payload[0] = cell;
        update_msg(opponent, MOVE_MADE_UPDATE_VALUE, 1, payload);
        return RECEIVE_PACKET;
    } else if(end_game_status == WIN){
        uint8_t loser_payload[2];
        uint8_t winner_payload[2];

        loser_payload[0] = LOSS;
        winner_payload[0] = WIN;

        loser_payload[1] = cell;
        winner_payload[1] = cell;
        update_msg(current_player, END_OF_GAME_UPDATE_VALUE, 2, winner_payload);  
        update_msg(opponent, END_OF_GAME_UPDATE_VALUE, 2, loser_payload);      
    } else if(end_game_status == TIE) {
        uint8_t tied_payload[2];
        tied_payload[0] = TIE;
        tied_payload[1] = cell;
        update_msg(current_player, END_OF_GAME_UPDATE_VALUE, 2, tied_payload);  
        update_msg(opponent, END_OF_GAME_UPDATE_VALUE, 2, tied_payload); 
    }
    return END_GAME;;

}
int make_move(Environment *env){
    printf("ENTERED STATE: make_move\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;

    if(set_active_game(env, game_env->active_fd) == false){
        error_msg(game_env->active_fd, CLIENT_INVALID_REQUEST,received_packet[REQ_TYPE_BYTE_INDEX]);
        return RECEIVE_PACKET;
    }
    GameState *gs =  game_env->gamestates[game_env->active_game_state_index];
    if(gs->game_type == TTT){
        return make_move_TTT(env);
    } else if(gs->game_type == RPS){
        return make_move_RPS(env);
    }
    
    printf("Invalid game type while making move, exiting...\n");
    exit(EXIT_FAILURE);

}

int get_placement_RPS(int my_move, int opponents_move){
    int next_move = (my_move % 3) + 1;
    //FIXME: broken logic
    if(my_move == opponents_move){
        return TIE;
    }
    if (opponents_move == next_move){
        return LOSS;
    }

    return WIN;
}

void end_game_RPS(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState *gs =  game_env->gamestates[game_env->active_game_state_index];
    RPSGameState * gs_RPS = (RPSGameState *)gs;
    int player1_fd = gs->player_fds[PLAYER_ONE];
    int player2_fd = gs->player_fds[PLAYER_TWO];

    int player1_move = gs_RPS->moves[PLAYER_ONE];
    int player2_move = gs_RPS->moves[PLAYER_TWO];

    int8_t player1_payload[2];
    int8_t player2_payload[2];

    player1_payload[0] = get_placement_RPS(player1_move, player2_move);
    player1_payload[1] = player2_move;

    player2_payload[0]= get_placement_RPS(player2_move, player1_move);
    player2_payload[1] = player1_move;



    update_msg(player1_fd, END_OF_GAME_UPDATE_VALUE, 2, player1_payload);
   
    update_msg(player2_fd, END_OF_GAME_UPDATE_VALUE, 2, player2_payload);
    FD_CLR(player1_fd, game_env->current_sockets);

    FD_CLR(player2_fd, game_env->current_sockets); 

    close(player1_fd);

    
    close(player2_fd);
    clean_game_states(env);

    
}

void end_game_TTT(Environment *env){
    GameEnvironment *game_env = (GameEnvironment *)env;
    GameState *gs =  game_env->gamestates[game_env->active_game_state_index];

    close(gs->player_fds[PLAYER_ONE]);
    close(gs->player_fds[PLAYER_TWO]);
    clean_game_states(env);

}

int end_game(Environment *env){
    printf("ENTERED STATE: end_game\n");
    GameEnvironment *game_env = (GameEnvironment *)env;
    char *received_packet = game_env->received_packet;
    
    GameState *gs =  game_env->gamestates[game_env->active_game_state_index];
    if(gs->game_type == TTT){
        end_game_TTT(env);
        return RECEIVE_PACKET;
    } else if(gs->game_type == RPS){
        end_game_RPS(env);
        return RECEIVE_PACKET;
       
    }
    
    printf("Invalid game type while making move, exiting...\n");
    exit(EXIT_FAILURE);
}