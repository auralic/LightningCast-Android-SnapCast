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

// prototype/interface header file
#include "player.hpp"

// local headers
#include "aixlog.hpp"
#include "snap_exception.hpp"
#include "str_compat.hpp"
#include "string_utils.hpp"

// standard headers
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>


using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "Player";

Player::Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : io_context_(io_context), active_(false), stream_(stream), settings_(settings), volCorrection_(1.0)
{
    auto not_empty = [](const std::string& value) -> std::string
    {
        if (!value.empty())
            return value;
        else
            return "<none>";
    };
    LOG(INFO, LOG_TAG) << "Player name: " << not_empty(settings_.player_name) << "\n";

    LOG(INFO, LOG_TAG) << "Sampleformat: " << (settings_.sample_format.isInitialized() ? settings_.sample_format.toString() : stream->getFormat().toString())
                       << ", stream: " << stream->getFormat().toString() << "\n";
}


Player::~Player()
{
    stop();
}


void Player::start()
{
    active_ = true;
    if (needsThread())
        playerThread_ = thread(&Player::worker, this);
}


void Player::stop()
{
    if (active_)
    {
        active_ = false;
        if (playerThread_.joinable())
            playerThread_.join();
    }
}


void Player::worker()
{
}

} // namespace player
