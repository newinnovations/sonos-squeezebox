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

#ifndef SBSTREAMER_H
#define SBSTREAMER_H

#include "locked.h"
#include "requestbroker.h"

#include <vector>

#define SBSTREAMER_CNAME "squeezebox"
#define SBSTREAMER_URI "/music/squeezebox.flac"

namespace NSROOT {

class SBStreamer : public RequestBroker {
public:
    SBStreamer(RequestBroker* imageService = nullptr);
    ~SBStreamer() override { }
    virtual bool HandleRequest(handle* handle) override;

    const char* CommonName() override { return SBSTREAMER_CNAME; }
    RequestBroker::ResourcePtr GetResource(const std::string& title) override;
    RequestBroker::ResourceList GetResourceList() override;
    RequestBroker::ResourcePtr RegisterResource(const std::string& title, const std::string& description, const std::string& path, StreamReader* delegate) override;
    void UnregisterResource(const std::string& uri) override;

private:
    ResourceList m_resources;
    LockedNumber<int> m_playbackCount;

    void streamSqueezeBox(handle* handle, int stream);

    void Reply400(handle* handle);
    void Reply429(handle* handle);

    void readParameters(const std::string& streamUrl, std::vector<std::string>& params);
    std::string getParamValue(const std::vector<std::string>& params, const std::string& name);
};
}
#endif /* SBSTREAMER_H */
