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

#ifndef _SNAPCASTCLIENT_H
#define _SNAPCASTCLIENT_H

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
#include <mutex>
#include <condition_variable>

// Enum for SnapCast client commands
enum class SNAPCASTCLIENTCmd
{
    SNAPCASTCLIENTCmd_NONE = 0,
    SNAPCASTCLIENTCmd_START,
    SNAPCASTCLIENTCmd_STOP,
    SNAPCASTCLIENTCmd_EXIT
};

// SnapCastClient class definition
class SnapCastClient
{
public:
    std::mutex mutex; // Mutex for thread synchronization
    std::condition_variable cond; // Condition variable for command handling
    std::condition_variable cond_sync; // Condition variable for synchronization
    bool ready; // Flag indicating readiness
    bool ready_sync; // Flag indicating synchronization readiness
    SNAPCASTCLIENTCmd cmd; // Current command
    std::string snapserver_ipaddr; // Snapserver IP address
    bool is_start; // Flag indicating if the client is started
    int efd; // Event file descriptor
    std::string pcm_name; // PCM device name

    SnapCastClient():ready(false),ready_sync(false),cmd(SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE),is_start(false),efd(-1)
    {
    }
    ~SnapCastClient()
    {
    }

    // Method to handle commands
    void handle(const std::string &pcm_name);
    // Method to finish the current command
    void finishCmd();
    // Method to finish the current command with locking
    void LockfinishCmd();
    // Method to start the client
    bool start(const std::string &_snapserver_ipaddr);
    // Method to stop the client
    void stop();
    // Method to perform the start operation
    bool doStart();
    // Method to exit the client
    void exit();
};

#endif
