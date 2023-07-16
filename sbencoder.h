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

#ifndef FLACENCODER_H
#define FLACENCODER_H

#include "audioencoder.h"
#include "local_config.h"

#include <FLAC++/encoder.h>
#include <FLAC++/metadata.h>

namespace NSROOT {

class FrameBuffer;
class FramePacket;

class SBEncoder {
    friend class SBEncoderStream;

public:
    SBEncoder();
    SBEncoder(int buffered);
    ~SBEncoder();

    bool open();
    bool open(uint8_t sampleSize);
    int write(const char* data, int len, unsigned timeout);
    int read(char* data, int maxlen, unsigned timeout);
    void close();

    int streamId() { return m_stream; }

private:
    int encode(const char* data, int len);
    int bytesAvailable() const;
    int writeEncodedData(const char* data, int len);
    int readData(char* data, int maxlen);

private:
    typedef enum {
        INIT,
        ENCODING,
        CLOSING,
        CLOSED
    } Status_t;

    Status_t m_status;
    uint32_t m_start_ms; // time read of encoded data started
    uint32_t m_total; // pcm bytes encoded so far
    int m_bytesPerFrame;
    int m_sampleSize;
    unsigned m_stream;
    FLAC__int32* m_pcm;

    FrameBuffer* m_buffer;
    FramePacket* m_packet;
    int m_consumed;

    class SBEncoderStream : public FLAC::Encoder::Stream {
    public:
        explicit SBEncoderStream(SBEncoder* p)
            : m_p(p)
        {
        }
        virtual FLAC__StreamEncoderWriteStatus write_callback(const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame);

    private:
        SBEncoder* m_p;
    };

    SBEncoderStream* m_encoder;
};

}
#endif // FLACENCODER_H
