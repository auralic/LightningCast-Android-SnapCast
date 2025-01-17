cmake_minimum_required(VERSION 3.10)

project(SnapCastClient)

set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(.)
include_directories(../boost_1_86_0)

# Source files
set(SOURCES
    SnapCastClient.cpp
    Util.cpp
    jsoncpp.cpp
    alsa_player.cpp
    player_client.cpp
    controller.cpp
    pcm_decoder.cpp
    snapcast_client.cpp
    time_provider.cpp
    client_connection.cpp
    null_decoder.cpp
    player.cpp
    sample_format.cpp
    stream.cpp
)

# Executable
add_executable(SnapCastClient ${SOURCES})

# Link libraries
target_link_libraries(SnapCastClient asound pthread)
