#include <stdio.h>
#include <stdlib.h>
#include <dcfsm/fsm.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "game_state.h"
#include "state_functions.h"
#define PORT 2034
#define BACKLOG 5
#define STARTING_UID_INDEX 4
#define UDP_BUF_SIZE 5010
int get_server_socket()
{
    struct sockaddr_in addr;
    int sfd;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    listen(sfd, BACKLOG);

    return sfd;
}

GameEnvironment *game_environment_create(int sfd, fd_set *rfsd)
{
    GameEnvironment *game_env = calloc(sizeof(GameEnvironment), 1);
    game_env->active_game_state_index = -1;
    game_env->server_fd = sfd;
    game_env->waiting_player_TTT = -1;
    game_env->waiting_player_RPS = -1;
    game_env->current_sockets = rfsd;
    game_env->current_max_fd = FD_SETSIZE;

    for (int i = 0; i < NUM_GAMES; i++)
    {
        game_env->gamestates[i] = NULL;
    }

    return game_env;
}
int get_uid_from_msg(uint8_t * msg){
    // pointy arrows go brrrrrr
    // https://stackoverflow.com/questions/8173037/convert-4-bytes-char-to-int32-in-c
    int uid  = (msg[STARTING_UID_INDEX] << 24) | (msg[STARTING_UID_INDEX+1] << 16) | (msg[STARTING_UID_INDEX+2] << 8) | msg[STARTING_UID_INDEX+3];
    return uid;
}
int get_udp_socket(){
   
    int socket_desc;
    struct sockaddr_in server_addr;
    
    // Create UDP socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return -1;
    }
    
    printf("Socket created successfully\n");
    
    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2034);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");

    return socket_desc;
}

void* udp_worker_loop(void* env){
    printf("udp_worker_loop\n");
    GameEnvironment *game_env = (GameEnvironment *) env;
    int udp_sock = get_udp_socket();
    
    // struct sockaddr_in * clients[2] = { NULL, NULL };

    uint8_t client_message[UDP_BUF_SIZE];
    struct sockaddr_in client_addr;
    // client_addr.sin_port = -1;
    int client_struct_length = sizeof(client_addr);
    while(1) {
        // Clean buffers:
        memset(client_message, 0, sizeof(client_message));

        // Receive client's message:
        if (recvfrom(udp_sock, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&client_addr, &client_struct_length) < 0) {
            printf("Couldn't receive\n");
            continue;
        }

        printf("Received message from IP: %s and port: %i\n",
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        int uid = get_uid_from_msg(client_message);
        int game_index = find_my_game_index(env, uid);

        if(game_index == -1) continue;

        GameState *gs = game_env->gamestates[game_index];
        int player_num = gs->player_fds[PLAYER_ONE] == uid ? PLAYER_ONE : PLAYER_TWO;
       
        int opponent_num = (player_num + 1) % 2;

        if(gs->voice_connected[player_num] == false){
            printf("set player%d\n", player_num);
            gs->player_addresses[player_num] = client_addr;
            gs->voice_connected[player_num] = true;
        }
        printf("uid: %d p_num: %d opp_num: %d\n", uid, player_num, opponent_num);
        if(gs->voice_connected[opponent_num] == false) continue;

        struct sockaddr_in opponent_addr = gs->player_addresses[opponent_num];
        
        
        printf("Recieved bytes: ");
        for (int i = 0; i < 16; i++) {
            printf("%u ", client_message[i]);
        }
        printf("\n");
        printf("sending\n");
        if (sendto(udp_sock, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&opponent_addr, client_struct_length) < 0) {
            printf("Can't send\n");
            continue;
        }
    }
    
    // Close the socket:
    close(udp_sock);
}

int main(int argc, char *argv[])
{
  int server_fd;
    server_fd = get_server_socket();
    fd_set current_sockets;

    FD_ZERO(&current_sockets);
    FD_SET(server_fd, &current_sockets);

    GameEnvironment *game_env = game_environment_create(server_fd, &current_sockets);
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_worker_loop, game_env);


    StateTransition transitions[] =
        {
            {FSM_INIT, RECEIVE_PACKET, &receive_packet},
            {RECEIVE_PACKET, FSM_EXIT, NULL},
            {RECEIVE_PACKET, HANDLE_PACKET, &handle_packet},
            {RECEIVE_PACKET, RECEIVE_PACKET, &receive_packet},
            {HANDLE_PACKET, CONFIRMATION_REQUEST, &confirmation_request},
            {HANDLE_PACKET, INFORMATION_REQUEST, &information_request},
            {HANDLE_PACKET, META_REQUEST, &meta_request},
            {HANDLE_PACKET, GAME_ACTION_REQUEST, &game_action_request},
            {HANDLE_PACKET, RECEIVE_PACKET, &receive_packet},
            {CONFIRMATION_REQUEST, CONFIRM_RULESET, &confirm_ruleset},
            {CONFIRM_RULESET, RECEIVE_PACKET, &receive_packet},
            {CONFIRMATION_REQUEST, RECEIVE_PACKET, &receive_packet},
            {CONFIRM_RULESET, JOIN_GAME, &join_game},
            {JOIN_GAME, RECEIVE_PACKET, &receive_packet},
            {JOIN_GAME, START_GAME, &start_game},
            {START_GAME, RECEIVE_PACKET, &receive_packet},
            {META_REQUEST, QUIT_GAME, &quit_game},
            {GAME_ACTION_REQUEST, MAKE_MOVE, &make_move},
            {MAKE_MOVE, RECEIVE_PACKET, &receive_packet},
            {MAKE_MOVE, END_GAME, &end_game},
            {END_GAME, RECEIVE_PACKET, &receive_packet},

            {FSM_IGNORE, FSM_IGNORE, NULL},
        };
    int code;
    int start_state;
    int end_state;
    start_state = FSM_INIT;
    end_state = RECEIVE_PACKET;

    code = fsm_run((Environment *)game_env, &start_state, &end_state, transitions);

    if (code != 0)
    {
        fprintf(stderr, "Cannot move from %d to %d\n", start_state, end_state);

        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}