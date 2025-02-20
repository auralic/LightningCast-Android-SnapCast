/***
    Copyright (C) AURALIC Holdings Inc. - All Rights Reserved
    This file is part of LightningCastDevice

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <string>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <list>
#include <map>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <poll.h>
#include "Util.h"
#include "player_client.hpp"
#include "snapcast_client.hpp"

static bool stop = false;
// Function to initialize signal handlers
void init_sig(int sig)
{
    struct sigaction action;

    switch (sig) {
        case SIGPIPE:
            action.sa_handler = [](int) {
                std::cerr << "Received SIGPIPE, ignoring..." << std::endl;
            };
            break;
        case SIGTERM:
        case SIGINT:
            action.sa_handler = [](int) {
                stop = true;
            };
            break;
        default:
            return;
    }

    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(sig, &action, nullptr);
}

int main(int argc, char *argv[])
{
    init_sig(SIGPIPE);
    init_sig(SIGTERM);
    init_sig(SIGINT);

    if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        std::cout << "Usage: " << argv[0] << " [pcm_name]"  << std::endl;
        return 0;
    }

    std::string pcm_name = "default";
    if(argc == 2)
    {
        pcm_name = argv[1];
    }

    SnapCastClient snapCastClient;
    PlayerClient playerClient(snapCastClient);

    std::thread playerClientThread([&]() {playerClient.handle();});
    std::thread snapcastClientThread([&]() {snapCastClient.handle(pcm_name);});

    while(!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    playerClient.Exit();
    snapCastClient.exit();

    if (playerClientThread.joinable()) {
        playerClientThread.join();
    }
    if (snapcastClientThread.joinable()) {
        snapcastClientThread.join();
    }
    return 0;
}
