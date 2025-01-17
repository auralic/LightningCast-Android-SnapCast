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
#include "sample_format.hpp"
#include "pcm_device.hpp"

// standard headers
#include <string>



struct ClientSettings
{
    struct Server
    {
        std::string host{""};
        size_t port{1704};
    };

    struct Player
    {
        std::string player_name{"alsa"};
        std::string parameter{""};
        int latency{0};
        player::PcmDevice pcm_device;
        SampleFormat sample_format;
    };

    struct Logging
    {
        std::string sink{"stdout"};
        std::string filter{"*:fatal"};
    };

    size_t instance{1};
    std::string host_id;

    Server server;
    Player player;
    Logging logging;
};
