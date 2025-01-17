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
#include "string_utils.hpp"

// standard headers
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#ifndef WINDOWS
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#if !defined(WINDOWS) && !defined(FREEBSD)
#include <sys/sysinfo.h>
#endif
#ifdef MACOS
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOTypes.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#endif
#ifdef ANDROID
#include <sys/system_properties.h>
#endif
#ifdef WINDOWS
#include <chrono>
#include <direct.h>
#include <iphlpapi.h>
#include <versionhelpers.h>
#include <windows.h>
#include <winsock2.h>
#endif


namespace strutils = utils::string;


#ifndef WINDOWS
static std::string execGetOutput(const std::string& cmd)
{
    std::shared_ptr<::FILE> pipe(popen((cmd + " 2> /dev/null").c_str(), "r"),
                                 [](::FILE* stream)
                                 {
        if (stream != nullptr)
            pclose(stream);
    });
    if (!pipe)
        return "";
    char buffer[1024];
    std::string result;
    while (feof(pipe.get()) == 0)
    {
        if (fgets(buffer, 1024, pipe.get()) != nullptr)
            result += buffer;
    }
    return strutils::trim(result);
}
#endif


#ifdef ANDROID
static std::string getProp(const std::string& key, const std::string& def = "")
{
    std::string result(def);
    char cresult[PROP_VALUE_MAX + 1];
    if (__system_property_get(key.c_str(), cresult) > 0)
        result = cresult;
    return result;
}
#endif


static std::string getOS()
{
    static std::string os;

    if (!os.empty())
        return os;

    os = execGetOutput("lsb_release -d");
    if ((os.find(':') != std::string::npos) && (os.find("lsb_release") == std::string::npos))
        os = strutils::trim_copy(os.substr(os.find(':') + 1));

    if (os.empty())
    {
        os = strutils::trim_copy(execGetOutput("grep /etc/os-release /etc/openwrt_release -e PRETTY_NAME -e DISTRIB_DESCRIPTION"));
        if (os.find('=') != std::string::npos)
        {
            os = strutils::trim_copy(os.substr(os.find('=') + 1));
            os.erase(std::remove(os.begin(), os.end(), '"'), os.end());
            os.erase(std::remove(os.begin(), os.end(), '\''), os.end());
        }
    }
    if (os.empty())
    {
        utsname u;
        uname(&u);
        os = u.sysname;
    }

    strutils::trim(os);
    return os;
}


static std::string getHostName()
{

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    return hostname;
}


static std::string getArch()
{
    std::string arch;

    arch = execGetOutput("arch");
    if (arch.empty())
        arch = execGetOutput("uname -i");
    if (arch.empty() || (arch == "unknown"))
        arch = execGetOutput("uname -m");

    return strutils::trim_copy(arch);
}

// Seems not to be used
// static std::chrono::seconds uptime()
// {
// #ifndef WINDOWS
// #ifndef FREEBSD
//     struct sysinfo info;
//     sysinfo(&info);
//     return std::chrono::seconds(info.uptime);
// #else
//     std::string uptime = execGetOutput("sysctl kern.boottime");
//     if ((uptime.find(" sec = ") != std::string::npos) && (uptime.find(",") != std::string::npos))
//     {
//         uptime = strutils::trim_copy(uptime.substr(uptime.find(" sec = ") + 7));
//         uptime.resize(uptime.find(","));
//         timeval now;
//         gettimeofday(&now, NULL);
//         try
//         {
//             return std::chrono::seconds(now.tv_sec - cpt::stoul(uptime));
//         }
//         catch (...)
//         {
//         }
//     }
//     return 0s;
// #endif
// #else
//     return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(GetTickCount()));
// #endif
// }


/// http://stackoverflow.com/questions/2174768/generating-random-uuids-in-linux
static std::string generateUUID()
{
    static bool initialized(false);
    if (!initialized)
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        initialized = true;
    }
    std::stringstream ss;
    ss << std::setfill('0') << std::hex << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4)
       << (std::rand() % 0xffff) << "-" << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4)
       << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff);
    return ss.str();
}


/// https://gist.github.com/OrangeTide/909204
static std::string getMacAddress(int sock)
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[16384];
    int success = 0;

    if (sock < 0)
        return "";

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) != 0)
        return "";

    struct ifreq* it = ifc.ifc_req;
    for (int i = 0; i < ifc.ifc_len;)
    {
/// some systems have ifr_addr.sa_len and adjust the length that way, but not mine. weird */

        size_t len = sizeof(*it);

        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
        {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) // don't count loopback
            {
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
                {
                    success = 1;
                    break;
                }
                else
                {
                    std::stringstream ss;
                    ss << "/sys/class/net/" << ifr.ifr_name << "/address";
                    std::ifstream infile(ss.str().c_str());
                    std::string line;
                    if (infile.good() && std::getline(infile, line))
                    {
                        strutils::trim(line);
                        if ((line.size() == 17) && (line[2] == ':'))
                            return line;
                    }
                }
            }
        }
        else
        { /* handle error */
        }

        it = (struct ifreq*)((char*)it + len);
        i += len;
    }

    if (!success)
        return "";

    char mac[19];

    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)ifr.ifr_hwaddr.sa_data[0], (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2], (unsigned char)ifr.ifr_hwaddr.sa_data[3], (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    return mac;
}

static std::string getHostId(const std::string& defaultId = "")
{
    std::string result = strutils::trim_copy(defaultId);

    if (!result.empty()                    // default provided
        && (result != "00:00:00:00:00:00") // default mac returned by getMaxAddress if it fails
        && (result != "02:00:00:00:00:00") // the Android API will return "02:00:00:00:00:00" for WifiInfo.getMacAddress()
        && (result != "ac:de:48:00:11:22") // iBridge interface on new MacBook Pro (later 2016)
    )
        return result;


    //#else
    //	// on embedded platforms it's
    //  // - either not there
    //  // - or not unique, or changes during boot
    //  // - or changes during boot
    //	std::ifstream infile("/var/lib/dbus/machine-id");
    //	if (infile.good())
    //		std::getline(infile, result);
    //#endif
    strutils::trim(result);
    if (!result.empty())
        return result;

    /// The host name should be unique enough in a LAN
    return getHostName();
}

struct comm_header {
    uint32_t key{0};
    uint32_t data_size{0};
};
