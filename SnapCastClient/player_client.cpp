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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <map>
#include <sstream>
#include <sys/time.h>
#include "Util.h"
#include "json/json.h"
#include "player_client.hpp"
#include "utils.hpp"
using namespace std;

#define SNAPCASTPLAYERSOCKET "./socketForSnapcastPlayer"
#define LIGHTNINGCAST_COMM_HEADER_KEY         0x9876

// Function to handle client operations
void PlayerClient::handle()
{
    remove(SNAPCASTPLAYERSOCKET);
    int listener;
    fd_set read_fds,master;
    int fdmax;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("opening stream socket");
        exit(0);
    }
    struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, SNAPCASTPLAYERSOCKET);
    if (bind(listener, (struct sockaddr *) &server, sizeof(struct sockaddr_un))) {
        perror("binding stream socket");
        exit(0);
    }
    listen(listener, 16);
    make_socket_non_blocking(listener);

    FD_SET(listener, &master);
    FD_SET(wake_fd.Get(), &master);
    fdmax = listener > wake_fd.Get() ? listener : wake_fd.Get();

    bool stop = false;
    while(!stop)
    {
        read_fds = master;
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("PlayerClient select() error!");
            exit(1);
        }
        for(int i = 0; i <= fdmax; i++)
        {
            if(FD_ISSET(i, &read_fds))
            {
                if(i == listener)
                {
                    struct sockaddr_storage ss;
                    socklen_t slen = sizeof(ss);
                    int fd;
                    if((fd = accept(listener, (struct sockaddr *)&ss, &slen)) == -1)
                    {
                        perror("PlayerClient accept() error!");
                    }
                    else
                    {
                        make_socket_non_blocking(fd);
                        FD_SET(fd, &master);
                        if(fd > fdmax)
                        {
                            fdmax = fd;
                        }
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            client_fd = fd;
                        }
                    }
                }
                else if (i == wake_fd.Get())
                {
                    wake_fd.Read();
                    stop = true;
                    break;
                }
                else
                {
                    if(!doRead(i))
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        shutdown(i, SHUT_RDWR);
                        close(i);
                        FD_CLR(i, &master);
                        client_fd = -1;
                    }
                }
            }
        }
    }

    close(listener);
}

// Function to read data from the client
bool PlayerClient::doRead(const int fd)
{
    ssize_t result;
    comm_header hi;

    memset(&hi, 0x00, sizeof(comm_header));
    uint8_t *hi_buf = (uint8_t *)&hi;
    size_t hi_length = 0;
    while (hi_length < sizeof(comm_header))
    {
        result = read(fd, hi_buf + hi_length, sizeof(comm_header) - hi_length);
        if (result < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            perror("read < 0");
            return false;
        }
        else if(result == 0)
        {
            return false;
        }
        hi_length += result;
    }
    if (ntohl(hi.key) != (uint32_t)LIGHTNINGCAST_COMM_HEADER_KEY)
    {
        return false;
    }
    hi.data_size = ntohl(hi.data_size);
    std::unique_ptr<uint8_t[]> buffer;
    size_t bufferSize = hi.data_size;
    buffer.reset(new uint8_t[bufferSize]);
    uint8_t *buf = buffer.get();
    size_t length = 0;
    while (length < (size_t)hi.data_size) {
        result = read(fd, buf + length, hi.data_size - length);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return false;
        } else if (result == 0) {
            return false;
        }
        length += result;
    }
    return doParse(fd, buf, length);
}

// Function to parse data from the client
bool PlayerClient::doParse(const int fd, const uint8_t *buf, size_t length)
{
    Json::Reader reader;
    Json::Value root;
    std::string data((const char *)buf, length);
    if (!reader.parse(data, root, false))
    {
        return false;
    }
    LIGHTNINGCASTCLIENTCmd recv_cmd = (LIGHTNINGCASTCLIENTCmd)root["Cmd"].asInt();
    string snapserver_ipaddr;
    switch (recv_cmd)
    {
    case LIGHTNINGCASTCLIENTCmd::LIGHTNINGCASTClientCmd_START:
        snapserver_ipaddr = root["SnapServerIPAddr"].asString();
        start(snapserver_ipaddr);
        break;
    case LIGHTNINGCASTCLIENTCmd::LIGHTNINGCASTClientCmd_STOP:
        stop();
        return false;
    default:
        return false;
    }
    return true;
}

// Function to start the client
void PlayerClient::start(const string &snapserver_ipaddr)
{
    bool result = snapCastClient.start(snapserver_ipaddr);
    Json::Value rootS;
    Json::FastWriter writer;
    rootS["Cmd"] = (int)LIGHTNINGCASTCLIENTCmd::LIGHTNINGCASTClientCmd_START;
    rootS["Result"] = result ? 1 : 0;
    std::string json_str = writer.write(rootS);

    comm_header hi;
    memset(&hi, 0x00, sizeof(comm_header));
    hi.key = htonl(LIGHTNINGCAST_COMM_HEADER_KEY);
    hi.data_size = htonl(json_str.size());

    uint8_t *sendBuf = new uint8_t[sizeof(comm_header)+json_str.size()+1];
    memset(sendBuf, 0x00, sizeof(comm_header)+json_str.size()+1);
    memcpy(sendBuf, &hi, sizeof(comm_header));
    memcpy(sendBuf+sizeof(comm_header), json_str.c_str(), json_str.size());
    processSend(sendBuf, sizeof(comm_header)+json_str.size());
    delete[] sendBuf;
}

// Function to stop the client
void PlayerClient::stop()
{
    snapCastClient.stop();
    Json::Value rootS;
    Json::FastWriter writer;
    rootS["Cmd"] = (int)LIGHTNINGCASTCLIENTCmd::LIGHTNINGCASTClientCmd_STOP;
    std::string json_str = writer.write(rootS);

    comm_header hi;
    memset(&hi, 0x00, sizeof(comm_header));
    hi.key = htonl(LIGHTNINGCAST_COMM_HEADER_KEY);
    hi.data_size = htonl(json_str.size());

    uint8_t *sendBuf = new uint8_t[sizeof(comm_header)+json_str.size()+1];
    memset(sendBuf, 0x00, sizeof(comm_header)+json_str.size()+1);
    memcpy(sendBuf, &hi, sizeof(comm_header));
    memcpy(sendBuf+sizeof(comm_header), json_str.c_str(), json_str.size());
    processSend(sendBuf, sizeof(comm_header)+json_str.size());
    delete[] sendBuf;
}

// Function to process sending data to the client
void PlayerClient::processSend(const uint8_t *buf, const size_t length)
{
    std::lock_guard<std::mutex> lock(mutex);
    if(client_fd <= 0)
    {
        return;
    }
    comm_header hi;
    memset(&hi, 0x00, sizeof(comm_header));
    hi.key = htonl(LIGHTNINGCAST_COMM_HEADER_KEY);
    hi.data_size = htonl(length);

    std::unique_ptr<uint8_t[]> sendBuf;
    size_t bufferSize = sizeof(comm_header)+length;
    sendBuf.reset(new uint8_t[bufferSize]);
    uint8_t *buffer = sendBuf.get();
    memcpy(buffer, &hi, sizeof(comm_header));
    memcpy(buffer+sizeof(comm_header), buf, length);

    size_t bytes_left = bufferSize;
    ssize_t written_bytes = 0;
    uint8_t *ptr = sendBuf.get();
    while(bytes_left > 0)
    {
        written_bytes = ::write(client_fd, ptr, bytes_left);
        if (written_bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // perror("send too fast , try again.");
                continue;
            }
            else
            {
                perror("send() from peer error");
                break;
            }
        }
        else if (written_bytes == 0)
        {
            break;
        }
        bytes_left -= written_bytes;
        ptr += written_bytes;
    }
}

// Function to exit the client
void PlayerClient::Exit()
{
    wake_fd.Write();
}
