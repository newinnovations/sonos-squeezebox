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

#include "sonos-status.h"

using namespace NSROOT;

Status::Status(PlayerPtr player)
    : m_player(player)
    , m_title("")
    , m_album("")
    , m_artist("")
    , m_transport_status("")
    , m_transport_state("")
    , m_volume(0)
    , m_reltime("")
    , m_current_track_duration("")
    , m_last_hash(0)
{
    m_uuid = m_player->GetZone()->GetCoordinator()->GetUUID();
    m_name = m_player->GetZone()->GetZoneName();
}

void Status::update()
{
    if (m_player && !m_player->TransportPropertyEmpty()) {
        if (!m_player->GetVolume(m_uuid, &m_volume)) {
            m_volume = 0;
        }
        SONOS::AVTProperty props = m_player->GetTransportProperty();
        SONOS::ElementList vars;
        if (m_player->GetPositionInfo(vars)) {
            m_reltime = vars.GetValue("RelTime");
        } else {
            m_reltime = "-:--:--";
        }
        if (props.CurrentTrackMetaData) {
            m_title = props.CurrentTrackMetaData->GetValue("dc:title");
            m_album = props.CurrentTrackMetaData->GetValue("upnp:album");
            m_artist = props.CurrentTrackMetaData->GetValue("dc:creator");
        }
        m_transport_status = props.TransportStatus;
        m_transport_state = props.TransportState;
        m_current_track_duration = props.CurrentTrackDuration;
    } else {
        m_title = "";
        m_album = "";
        m_artist = "";
        m_transport_status = "";
        m_transport_state = "";
        m_volume = 0;
        m_reltime = "-:--:--";
        m_current_track_duration = "-:--:--";
    }
}

void Status::print()
{
    std::string row("---------------------------------------------------------------------------------------------");
    printf("\n+%s %s ---+\n", row.substr(0, row.length() - m_name.length() - 5).c_str(), m_name.c_str());
    printf("| Title  %-84s |\n", m_title.c_str());
    if (m_album != "") {
        printf("| Album  %-84s |\n", m_album.c_str());
    }
    if (m_artist != "") {
        printf("| Artist %-84s |\n", m_artist.c_str());
    }
    printf("+%s+\n", row.c_str());
    printf("| %-26s | %-24s | vol %3d / 100 | %8s / %8s |\n",
        m_transport_status.c_str(),
        m_transport_state.c_str(),
        m_volume,
        m_reltime.c_str(),
        m_current_track_duration.c_str());
    printf("+%s+\n", row.c_str());
}

size_t Status::hash()
{
    size_t h = std::hash<std::string> {}(m_title);
    h += std::hash<std::string> {}(m_album);
    h += std::hash<std::string> {}(m_artist);
    h += std::hash<std::string> {}(m_transport_status);
    h += std::hash<std::string> {}(m_transport_state);
    h += std::hash<std::string> {}(m_reltime);
    h += std::hash<std::string> {}(m_current_track_duration);
    return h + std::hash<uint8_t> {}(m_volume);
}

bool Status::changed()
{
    size_t h = hash();
    bool c = h != m_last_hash;
    m_last_hash = h;
    return c;
}

void Status::get_mac(uint8_t* mac)
{
    if (m_uuid.length() > 19) {
        for (int i = 0; i < 6; ++i) {
            mac[i] = strtoul(m_uuid.substr(7 + 2 * i, 2).c_str(), 0, 16);
        }
    }
}
