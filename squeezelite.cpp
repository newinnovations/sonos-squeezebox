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

extern "C" {
#include "squeezelite.h"
#include "output_sonos.h"
}

#include <signal.h>

static void sighandler(int signum)
{
    slimproto_stop();
    signal(signum, SIG_DFL); // second signal will cause non gracefull shutdown
}

void squeezelite(const char* server, uint8_t * mac, const char* name)
{
    unsigned rates[MAX_SUPPORTED_SAMPLERATES] = { 0 };

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGHUP, sighandler);

    output_init_sonos(lWARN, OUTPUTBUF_SIZE, 0 /*output_params*/, rates, 0 /*rate_delay*/);
    decode_init(lWARN, 0 /*include_codecs,*/, "" /*exclude_codecs*/);
    stream_init(lWARN, STREAMBUF_SIZE);

    slimproto(lWARN, (char*)server, mac, name, 0 /*namefile*/, 0 /*modelname*/, 0 /*maxSampleRate*/);

    stream_close();
    decode_close();
    output_close_sonos();

    exit(0);
}
