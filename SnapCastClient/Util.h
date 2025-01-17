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

#ifndef _UTIL_H
#define _UTIL_H

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
#include <sys/time.h>

// Enum for LightningCast client commands
enum class LIGHTNINGCASTCLIENTCmd
{
    LIGHTNINGCASTClientCmd_NONE = 0,
    LIGHTNINGCASTClientCmd_START,
    LIGHTNINGCASTClientCmd_STOP,
    LIGHTNINGCASTClientCmd_REPORT_DATA
};

// Function to make a socket non-blocking
int make_socket_non_blocking(int sfd);

// Function to calculate the difference in microseconds between two timespec structures
uint64_t diff_in_us(struct timespec t1, struct timespec t2);

// Function to get the current time as a string
void getTimeNowStr(char *s);

// Macro for debugging
#define DEBUG(format,...)
// #define DEBUG(format,...) {char timeStr[100]={0};getTimeNowStr(timeStr);pid_t pid = gettid();printf("[%s]:[%d] - File: " __FILE__ ", Line: %05d: " format "\n", timeStr,pid, __LINE__, ##__VA_ARGS__);}

// Class for event file descriptor
class EventFD {
    int fd;

public:
    EventFD();
    ~EventFD();

    int Get() const {
        return fd;
    }

    bool Read();

    void Write();
};

#endif
