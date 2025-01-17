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

#ifndef _PLAYERCLIENT_H
#define _PLAYERCLIENT_H

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <mutex>
#include <condition_variable>
#include "Util.h"
#include "snapcast_client.hpp"

// Class for PlayerClient
class PlayerClient
{
public:
    SnapCastClient &snapCastClient;
    int client_fd;
    std::mutex mutex;
    std::condition_variable cond;
    bool ready;
    bool alsa_result;
    EventFD wake_fd;

    // Constructor
    PlayerClient(SnapCastClient &_snapCastClient):snapCastClient(_snapCastClient),client_fd(-1),ready(false),alsa_result(false)
    {
    }

    // Destructor
    ~PlayerClient()
    {
    }

    // Function to handle client operations
    void handle();

    // Function to read data from the client
    bool doRead(const int fd);

    // Function to parse data from the client
    bool doParse(const int fd, const uint8_t *buf, size_t length);

    // Function to process sending data to the client
    void processSend(const uint8_t *buf, const size_t length);

    // Function to start the client
    void start(const std::string &url);

    // Function to stop the client
    void stop();

    // Function to exit the client
    void Exit();
};

#endif
