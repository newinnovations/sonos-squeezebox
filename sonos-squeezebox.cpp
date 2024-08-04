// SONOS::Squeezebox -- deploy Sonos in a Logitech Media Server (LMS) streaming environment
//
// Copyright (c) 2023 Martin van der Werff <github (at) newinnovations.nl>
//
// This file is part of SONOS::Squeezebox.
//
// SONOS::Squeezebox is free software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation, either version 3
// of the License, or (at your option) any later version.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <contentdirectory.h>
#include <didlparser.h>
#include <filestreamer.h>
#include <imageservice.h>
#include <sonosplayer.h>
#include <sonossystem.h>

#include "sbstreamer.h"
#include "sonos-status.h"

extern "C" {
unsigned get_squeezebox_stream_id(void);
unsigned get_volume(void);
}

#include <algorithm>
#include <string>
#include <thread>
#include <unistd.h>

void squeezelite(const char* server, uint8_t * mac, const char* name);
static void handleEvent(void* handle);
static const char* getCmd(int argc, char** argv, const std::string& option);
static const char* getCmdOption(int argc, char** argv, const std::string& option);

SONOS::System* gSonos = 0;
SONOS::PlayerPtr gPlayer;
uint8_t gMac[6];
volatile bool gEvent = true;

static std::string urlEncode(std::string str)
{
    std::string new_str = "";
    for (auto& c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            new_str += c;
        } else {
            char bufHex[10];
            snprintf(bufHex, sizeof(bufHex) - 1, "%%%02x", c);
            new_str += bufHex;
        }
    }
    return new_str;
}

void squeezelite_thread(const char* server, std::string name)
{
    squeezelite(server, gMac, name.c_str());
    printf("squeezelite_thread: stopped\n");
}

bool PlaySqueezeBox(unsigned stream_id)
{
    SONOS::RequestBroker::ResourcePtr res(nullptr);
    SONOS::RequestBrokerPtr rb = gSonos->GetRequestBroker(SBSTREAMER_CNAME);
    if (rb) {
        res = rb->GetResource(SBSTREAMER_CNAME);
    }
    if (res) {
        bool hasParam = (res->uri.find("?") != std::string::npos);
        std::string streamURL;
        streamURL.assign(gPlayer->GetControllerUri()).append(res->uri).append(hasParam ? "&" : "?").append("stream=" + std::to_string(stream_id));
        std::string iconURL;
        iconURL.assign(gPlayer->GetControllerUri()).append(res->iconUri);
        std::string _title = res->description;
        _title.replace(res->description.find("%s"), 2, "g7700k");
        return gPlayer->PlayStream(streamURL, _title, iconURL);
    }
    printf("%s: service unavaible\n", __FUNCTION__);
    return false;
}

void flac_test();

int main(int argc, char** argv)
{
    int ret = 0, debug_level = 0;

    if (getCmd(argc, argv, "--debug"))
        debug_level = 4;

    const char* ip = getCmdOption(argc, argv, "--ip");
    const char* room = getCmdOption(argc, argv, "--room");
    const char* filename = getCmdOption(argc, argv, "--file");
    const char* server = getCmdOption(argc, argv, "--server");

    printf("\n\n| SONOS::Squeezebox -- deploy Sonos in a Logitech Media Server (LMS) streaming environment\n|\n");
    printf("| Copyright (c) 2023 Martin van der Werff <github (at) newinnovations.nl>\n\n\n");

    SONOS::System::Debug(debug_level);

    gSonos = new SONOS::System(0, handleEvent);

    if (!ip) {
        printf("Connecting to Sonos ... ");
        fflush(stdout);
        if (!gSonos->Discover()) {
            printf("No devices found (try specifying known Sonos player ip-address).\n");
            return EXIT_FAILURE;
        } else {
            printf("SUCCESS\n\n");
        }
    } else {
        std::string deviceUrl = "http://" + std::string(ip) + ":1400";
        printf("Connecting to Sonos (through player %s) ... ", ip);
        fflush(stdout);
        if (!gSonos->Discover(deviceUrl)) {
            printf("Device is unreachable.\n");
            return EXIT_FAILURE;
        } else {
            printf("SUCCESS\n\n");
        }
    }

    {
        SONOS::RequestBrokerPtr imageService(new SONOS::ImageService());
        gSonos->RegisterRequestBroker(imageService);
        gSonos->RegisterRequestBroker(SONOS::RequestBrokerPtr(new SONOS::SBStreamer(imageService.get())));
        gSonos->RegisterRequestBroker(SONOS::RequestBrokerPtr(new SONOS::FileStreamer()));
    }

    printf("+----------------------------------------------------------------------- devices / players ---+\n");
    printf("| %-35s | %-24s | %-18s | %5s |\n", "player name", "uuid", "host", "port");
    printf("+---------------------------------------------------------------------------------------------+\n");
    SONOS::ZonePlayerList players = gSonos->GetZonePlayerList();
    for (SONOS::ZonePlayerList::const_iterator it = players.begin(); it != players.end(); ++it) {
        printf("| %-35s | %-24s | %-18s | %5d |\n",
            it->first.c_str(), it->second->GetUUID().c_str(), it->second->GetHost().c_str(), it->second->GetPort());
    }
    printf("+---------------------------------------------------------------------------------------------+\n\n");

    printf("+--------------------------------------------------------------------------- zones / rooms ---+\n");
    printf("| %-35s | %-53s |\n", "room name", "coordinating player");
    printf("+---------------------------------------------------------------------------------------------+\n");
    SONOS::ZoneList zones = gSonos->GetZoneList();
    for (SONOS::ZoneList::const_iterator it = zones.begin(); it != zones.end(); ++it) {
        printf("| %-35s | %-53s |\n",
            it->second->GetZoneName().c_str(), it->second->GetCoordinator()->c_str());
    }
    printf("+---------------------------------------------------------------------------------------------+\n\n");

    if (!room) {
        printf("Please specify a room to join with the --room option\n");
        return EXIT_FAILURE;
    }

    printf("Connecting to room %s ... ", room);

    std::string zone = room;
    bool found = false;
    for (SONOS::ZoneList::const_iterator iz = zones.begin(); iz != zones.end(); ++iz) {
        if (iz->second->GetZoneName() == zone) { // zone == room
            found = true;
            if ((gPlayer = gSonos->GetPlayer(iz->second, 0, handleEvent))) {
                printf("SUCCESS");
            } else {
                printf("FAILED to connect\n");
                return EXIT_FAILURE;
            }
            break;
        }
    }
    if (!found) {
        printf("FAILED to find room\n");
        return EXIT_FAILURE;
    }

    SONOS::Status status(gPlayer);
    status.get_mac(gMac);
    printf(" (MAC = %02X:%02X:%02X:%02X:%02X:%02X)\n\n", gMac[0], gMac[1], gMac[2], gMac[3], gMac[4], gMac[5]);

    std::thread* t = 0;
    if (filename) {
        std::string fn(filename);
        std::string extension("none");
        std::string::size_type idx = fn.rfind('.');

        if (idx != std::string::npos) {
            extension = fn.substr(idx + 1);
        }

        std::string url = gPlayer->GetControllerUri() + "/music/track." + extension + "?path=" + urlEncode(fn);

        if (gPlayer->PlayStream(url, "")) {
            printf("Started playing URL %s\n", url.c_str());
        } else {
            printf("Failed to start URL %s\n", url.c_str());
        }
    } else {
        std::string name = "SONOS::" + std::string(room);
        t = new std::thread(squeezelite_thread, server, name);
    }

    unsigned current_stream_id = 0;
    unsigned time_count = 0;
    unsigned current_volume = 0;

    status.update();

    for (;;) {
        unsigned stream_id = get_squeezebox_stream_id();
        if (stream_id != current_stream_id) {
            current_stream_id = stream_id;
            PlaySqueezeBox(stream_id);
        }
        if ((time_count == 3000) || gEvent) {
            gEvent = false;
            status.update();
            if (status.changed()) {
                status.print();
            }
            time_count = 0;
        } else {
            ++time_count;
        }

        unsigned volume = get_volume();
        if(volume != current_volume) {
            SONOS::ZonePtr pl = gPlayer->GetZone();
            for (SONOS::Zone::iterator ip = pl->begin(); ip != pl->end(); ++ip) {
                gPlayer->SetVolume((*ip)->GetUUID(), volume);
            }
            current_volume = volume;
        }
        usleep(10000); // 10ms
    }

    if (t) {
        t->join();
    }

    return ret;
}

static void handleEvent(void* handle)
{
    gEvent = true;
}

static const char* getCmd(int argc, char** argv, const std::string& option)
{
    char** end = argv + argc;
    char** itr = std::find(argv, end, option);
    if (itr != end) {
        return *itr;
    }
    return NULL;
}

static const char* getCmdOption(int argc, char** argv, const std::string& option)
{
    char** end = argv + argc;
    for (char** it = argv; it != end; ++it) {
        if (strncmp(*it, option.c_str(), option.length()) == 0 && (*it)[option.length()] == '=')
            return &((*it)[option.length() + 1]);
    }
    return NULL;
}
