/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once

// local headers
#include "client_settings.hpp"
#include "endian.hpp"
#include "stream.hpp"
#include "player_client.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>

// standard headers
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>


namespace player
{

/// Audio Player
/**
 * Abstract audio player implementation
 */
class Player
{
public:
    Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~Player();
    /// Called on start, before the first audio sample is sent or any other function is called.
    virtual void start();
    /// Called on stop
    virtual void stop();

protected:
    /// will be run in a thread if needsThread is true
    virtual void worker();
    /// @return true if the worker function should be started in a thread
    virtual bool needsThread() const = 0;

    boost::asio::io_context& io_context_;
    std::atomic<bool> active_;
    std::shared_ptr<Stream> stream_;
    std::thread playerThread_;
    ClientSettings::Player settings_;
    double volCorrection_;
    mutable std::mutex mutex_;
};

} // namespace player
