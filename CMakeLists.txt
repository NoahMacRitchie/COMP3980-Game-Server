cmake_minimum_required(VERSION 3.17)
project(Gameserver C)

set(CMAKE_C_STANDARD 11)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)


add_library(state_functions STATIC state_functions.c)
target_link_libraries(state_functions)

add_library(game_state STATIC game_state.c)
target_link_libraries(game_state)

add_executable(server server.c)
target_link_libraries(server dcfsm dc pthread state_functions game_state)

add_executable(client client.c)
target_link_libraries(client dcfsm dc pthread state_functions)