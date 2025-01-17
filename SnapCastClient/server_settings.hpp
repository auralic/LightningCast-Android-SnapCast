/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef MESSAGE_SERVER_SETTINGS_HPP
#define MESSAGE_SERVER_SETTINGS_HPP

// local headers
#include "json_message.hpp"


namespace msg
{

class ServerSettings : public JsonMessage
{
public:
    ServerSettings() : JsonMessage(message_type::kServerSettings)
    {
        setBufferMs(0);
        setLatency(0);
    }

    ~ServerSettings() override = default;

    int32_t getBufferMs()
    {
        return get("bufferMs", 0);
    }

    int32_t getLatency()
    {
        return get("latency", 0);
    }

    void setBufferMs(int32_t bufferMs)
    {
        msg["bufferMs"] = bufferMs;
    }

    void setLatency(int32_t latency)
    {
        msg["latency"] = latency;
    }
};
} // namespace msg


#endif
