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

#include "sbstreamer.h"

#include "data/datareader.h"
#include "imageservice.h"
#include "private/tokenizer.h"
#include "private/urlencoder.h"
#include "requestbroker.h"
#include "sbencoder.h"

#include <cstring>
#include <mutex>
#include <unistd.h>

#define SBSTREAMER_ICON "/pulseaudio.png"
#define SBSTREAMER_CONTENT "audio/flac"
#define SBSTREAMER_DESC "Audio stream from %s"
#define SBSTREAMER_TIMEOUT 10000
#define SBSTREAMER_MAX_PLAYBACK 3
#define SBSTREAMER_CHUNK 16384

using namespace NSROOT;

volatile SBEncoder* g_enc = 0;
std::mutex g_enc_mutex;

extern "C" {
void encode_squeezebox_audio(const char* data, int len)
{
    int count = 0;
    g_enc_mutex.lock();
    for (;;) {
        if (g_enc) {
            int written = ((SBEncoder*)g_enc)->write(data, len, SBSTREAMER_TIMEOUT);
            if (written != len) {
                printf("encode_squeezebox_audio: write() failed %d != %d\n", written, len);
            }
            g_enc_mutex.unlock();
            return;
        } else {
            g_enc_mutex.unlock();
            if (count++ > 100) { // 10s
                printf("encode_squeezebox_audio: timeout waiting for stream request\n");
                return;
            }
            usleep(100000); // 100 ms
            g_enc_mutex.lock();
        }
    }
}

void close_squeezebox_audio()
{
    g_enc_mutex.lock();
    if (g_enc) {
        ((SBEncoder*)g_enc)->write(0, 0, SBSTREAMER_TIMEOUT);
    }
    g_enc_mutex.unlock();
}
} // extern "C"

SBStreamer::SBStreamer(RequestBroker* imageService /*= nullptr*/)
    : RequestBroker()
    , m_resources()
    , m_playbackCount(0)
{
    ResourcePtr img(nullptr);
    if (imageService) {
        img = imageService->RegisterResource(SBSTREAMER_CNAME, "Icon for " SBSTREAMER_CNAME,
            SBSTREAMER_ICON, DataReader::Instance());
    }
    ResourcePtr ptr = ResourcePtr(new Resource());
    ptr->uri = SBSTREAMER_URI;
    ptr->title = SBSTREAMER_CNAME;
    ptr->description = SBSTREAMER_DESC;
    ptr->contentType = SBSTREAMER_CONTENT;
    if (img) {
        ptr->iconUri.assign(img->uri).append("?id=" LIBVERSION);
    }
    m_resources.push_back(ptr);
}

bool SBStreamer::HandleRequest(handle* handle)
{
    if (!IsAborted()) {
        const std::string& requrl = RequestBroker::GetRequestURI(handle);
        if (requrl.compare(0, strlen(SBSTREAMER_URI), SBSTREAMER_URI) == 0) {
            switch (RequestBroker::GetRequestMethod(handle)) {
            case RequestBroker::Method_GET: {
                std::vector<std::string> params;
                readParameters(requrl, params);
                int stream = atoi(getParamValue(params, "stream").c_str());
                streamSqueezeBox(handle, stream);
                return true;
            }
            case RequestBroker::Method_HEAD: {
                std::string resp;
                resp.assign(RequestBroker::MakeResponseHeader(RequestBroker::Status_OK))
                    .append("Content-Type: audio/flac\r\n")
                    .append("\r\n");
                RequestBroker::Reply(handle, resp.c_str(), resp.length());
                return true;
            }
            default:
                return false; // unhandled method
            }
        }
    }
    return false;
}

RequestBroker::ResourcePtr SBStreamer::GetResource(const std::string& title)
{
    (void)title;
    return m_resources.front();
}

RequestBroker::ResourceList SBStreamer::GetResourceList()
{
    ResourceList list;
    for (ResourceList::iterator it = m_resources.begin(); it != m_resources.end(); ++it)
        list.push_back((*it));
    return list;
}

RequestBroker::ResourcePtr SBStreamer::RegisterResource(const std::string& title,
    const std::string& description,
    const std::string& path,
    StreamReader* delegate)
{
    (void)title;
    (void)description;
    (void)path;
    (void)delegate;
    return ResourcePtr();
}

void SBStreamer::UnregisterResource(const std::string& uri)
{
    (void)uri;
}

void SBStreamer::streamSqueezeBox(handle* handle, int stream)
{
    printf("Sonos requested stream %d\n", stream);

    m_playbackCount.Add(1);

    if (m_playbackCount.Load() > SBSTREAMER_MAX_PLAYBACK) {
        printf("ERROR: overloaded http (load=%d)\n", m_playbackCount.Load());
        Reply429(handle);
    } else {
        std::string resp;
        resp.assign(RequestBroker::MakeResponseHeader(RequestBroker::Status_OK))
            .append("Content-Type: audio/flac\r\n")
            .append("Transfer-Encoding: chunked\r\n")
            .append("\r\n");

        if (RequestBroker::Reply(handle, resp.c_str(), resp.length())) {
            SBEncoder* enc = new SBEncoder(stream);
            enc->open();
            {
                g_enc_mutex.lock();
                if (g_enc) {
                    if (((SBEncoder*)g_enc)->streamId() == stream) {
                        g_enc_mutex.unlock();
                        printf("Sonos requested stream that is already playing -- rejecting this request\n");
                        Reply429(handle);
                        m_playbackCount.Sub(1);
                        return;
                    } else {
                        ((SBEncoder*)g_enc)->close();
                    }
                }
                g_enc = enc;
                g_enc_mutex.unlock();
            }
            char* buf = new char[SBSTREAMER_CHUNK + 16];
            int r = 0;
            while (!IsAborted() && (r = enc->read(buf + 7, SBSTREAMER_CHUNK, SBSTREAMER_TIMEOUT)) > 0) {
                char str[8];
                snprintf(str, sizeof(str), "%05x\r\n", (unsigned)r & 0xfffff);
                memcpy(buf, str, 7);
                memcpy(buf + r + 7, "\r\n", 2);
                if (!RequestBroker::Reply(handle, buf, r + 7 + 2)) {
                    break;
                }
            }
            RequestBroker::Reply(handle, "0\r\n\r\n", 5);
            {
                g_enc_mutex.lock();
                enc->close();
                if (g_enc == enc) {
                    g_enc = 0;
                }
                g_enc_mutex.unlock();
            }
            delete enc;
            delete[] buf;
        }
    }

    m_playbackCount.Sub(1);

    printf("Done serving stream %d to Sonos\n", stream);
}

void SBStreamer::Reply400(handle* handle)
{
    std::string resp;
    resp.append(RequestBroker::MakeResponseHeader(RequestBroker::Status_Bad_Request)).append("\r\n");
    RequestBroker::Reply(handle, resp.c_str(), resp.length());
}

void SBStreamer::Reply429(handle* handle)
{
    std::string resp;
    resp.append(RequestBroker::MakeResponseHeader(RequestBroker::Status_Too_Many_Requests)).append("\r\n");
    RequestBroker::Reply(handle, resp.c_str(), resp.length());
}

void SBStreamer::readParameters(const std::string& streamUrl, std::vector<std::string>& params)
{
    size_t s = streamUrl.find('?');
    if (s != std::string::npos) {
        tokenize(streamUrl.substr(s + 1), "&", params, true);
    }
}

std::string SBStreamer::getParamValue(const std::vector<std::string>& params, const std::string& name)
{
    size_t lval = name.length() + 1;
    for (const std::string& str : params) {
        if (str.length() > lval && str.at(name.length()) == '=' && str.compare(0, name.length(), name) == 0) {
            return urldecode(str.substr(lval));
        }
    }
    return std::string();
}
