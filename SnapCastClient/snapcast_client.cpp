/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl
    Modifications Copyright (C) AURALIC Holdings Inc. - All Rights Reserved

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

// local headers
#include "Util.h"
#include "json/json.h"
#include "popl.hpp"
#include "controller.hpp"
#include "alsa_player.hpp"
#include "client_settings.hpp"
#include "aixlog.hpp"
#include "snap_exception.hpp"
#include "str_compat.hpp"
#include "snapcast_client.hpp"

// 3rd party headers
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

// standard headers
#include <iostream>
#ifndef WINDOWS
#include <csignal>
#include <sys/resource.h>
#endif


using namespace std;
using namespace popl;
using namespace player;

using namespace std::chrono_literals;

static constexpr auto LOG_TAG = "Snapclient";

// Method to start the SnapCast client
bool SnapCastClient::doStart()
{
    is_start = false;
    int exitcode = EXIT_SUCCESS;
    try
    {
        ClientSettings settings;

        settings.server.host = snapserver_ipaddr;
        settings.player.pcm_device.name = pcm_name;

        AixLog::Filter logfilter;
        auto filters = utils::string::split(settings.logging.filter, ',');
        for (const auto& filter : filters)
            logfilter.add_filter(filter);
        string logformat = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)";
        AixLog::Log::init<AixLog::SinkCout>(logfilter, logformat);

        boost::asio::io_context io_context;

        // Create event file descriptor
        efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd == -1) {
            return false;
        }
        boost::asio::posix::stream_descriptor descriptor(io_context, efd);
        descriptor.async_wait(
            boost::asio::posix::stream_descriptor::wait_read,
            [&](const boost::system::error_code& ec)
            {
            if (!ec) {
                uint64_t value;
                ssize_t bytes_read = ::read(efd, &value, sizeof(value));
                LOG(INFO, LOG_TAG) << "Eventfd triggered value: " << value << "\n";
            } else {
                LOG(INFO, LOG_TAG) << "Failed to wait for Eventfd, error: " << ec.message() << "\n";
            }
            io_context.stop();
        });

        is_start = true;
        LockfinishCmd();

        auto controller = make_shared<Controller>(io_context, settings);
        controller->start();

        int num_threads = 0;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n)
            threads.emplace_back([&] { io_context.run(); });
        io_context.run();
        for (auto& t : threads)
            t.join();
        ::close(efd);
    }
    catch (const std::exception& e)
    {
        LOG(FATAL, LOG_TAG) << "Exception: " << e.what() << std::endl;
        return false;
    }

    LOG(NOTICE, LOG_TAG) << "Snapclient terminated." << endl;
    return true;
}

// Method to handle commands
void SnapCastClient::handle(const std::string &pcm_name)
{
    this->pcm_name = pcm_name;
    std::unique_lock<std::mutex> lock(mutex);
    while(true)
    {
        switch (cmd) 
        {
        case SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_START:
            lock.unlock();
            doStart();
            lock.lock();
            break;
        case SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_STOP:
            finishCmd();
            break;
        case SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE:
            cond.wait(lock, [&] { return ready; });
            ready = false;
            break;
        case SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_EXIT:
            finishCmd();
            return;
        }
    }
}

// Method to start the SnapCast client
bool SnapCastClient::start(const std::string &_snapserver_ipaddr)
{
    std::unique_lock<std::mutex> lock(mutex);
    snapserver_ipaddr = _snapserver_ipaddr;
    cmd = SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_START;
    ready = true;
    ready_sync = false;
    is_start = false;
    cond.notify_one();
    while (cmd != SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE)
    {
        cond_sync.wait(lock, [&] { return ready_sync; });
    }
    return is_start;
}

// Method to stop the SnapCast client
void SnapCastClient::stop()
{
    std::unique_lock<std::mutex> lock(mutex);
    cmd = SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_STOP;
    ready = true;
    ready_sync = false;
    if(efd != -1)
    {
        eventfd_t value = 1;
        ssize_t nbytes = write(efd, &value, sizeof(value));
    }
    cond.notify_one();
    while (cmd != SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE)
    {
        cond_sync.wait(lock, [&] { return ready_sync; });
    }
}

// Method to finish the current command
void SnapCastClient::finishCmd()
{
    cmd = SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE;
    ready_sync = true;
    cond_sync.notify_one();
}

// Method to finish the current command with locking
void SnapCastClient::LockfinishCmd()
{
    std::unique_lock<std::mutex> lock(mutex);
    cmd = SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_NONE;
    ready_sync = true;
    cond_sync.notify_one();
}

// Method to exit the SnapCast client
void SnapCastClient::exit()
{
    std::unique_lock<std::mutex> lock(mutex);
    cmd = SNAPCASTCLIENTCmd::SNAPCASTCLIENTCmd_EXIT;
    ready = true;
    ready_sync = false;
    if(efd != -1)
    {
        eventfd_t value = 1;
        ssize_t nbytes = write(efd, &value, sizeof(value));
    }
    cond.notify_one();
}
