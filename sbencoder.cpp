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

#include "sbencoder.h"
#include "framebuffer.h"
#include "private/byteorder.h"
#include <unistd.h>

#define SAMPLES 1024
#define FRAME_BUFFER_SIZE 256

extern "C" {
uint32_t get_sb_time_ms(void);
unsigned get_squeezebox_stream_id(void);
} // extern "C"

using namespace NSROOT;

SBEncoder::SBEncoder(int stream)
    : m_status(INIT)
    , m_start_ms(0)
    , m_total(0)
    , m_bytesPerFrame(0)
    , m_sampleSize(0)
    , m_stream(stream)
    , m_pcm(nullptr)
    , m_buffer(nullptr)
    , m_packet(nullptr)
    , m_consumed(0)
    , m_encoder(nullptr)
{
    m_buffer = new FrameBuffer(FRAME_BUFFER_SIZE);
    m_encoder = new SBEncoderStream(this);
}

SBEncoder::~SBEncoder()
{
    m_encoder->finish();
    delete m_encoder;
    if (m_pcm != nullptr) {
        delete[] m_pcm;
    }
    if (m_packet) {
        m_buffer->freePacket(m_packet);
    }
    delete m_buffer;
}

bool SBEncoder::open()
{
    return open(16);
}

bool SBEncoder::open(uint8_t sampleSize)
{
    if (m_status != INIT) {
        printf("SBEncoder::open(stream=%d) -- already opened\n", m_stream);
        return false;
    }

    AudioFormat m_format;
    m_format.byteOrder = AudioFormat::LittleEndian;
    m_format.sampleType = AudioFormat::SignedInt;
    m_format.sampleSize = sampleSize;
    m_format.sampleRate = 44100;
    m_format.channelCount = 2;
    m_format.codec = "audio/pcm";

    m_encoder->set_verify(true);
    m_encoder->set_compression_level(5);
    m_encoder->set_channels(m_format.channelCount);
    m_encoder->set_bits_per_sample(m_format.sampleSize);
    m_encoder->set_sample_rate(m_format.sampleRate);

    m_bytesPerFrame = m_format.bytesPerFrame();
    m_sampleSize = m_format.sampleSize;

    m_buffer->clear();
    if (m_packet)
        m_buffer->freePacket(m_packet);
    m_packet = nullptr;

    if (m_pcm != nullptr)
        delete[] m_pcm;
    m_pcm = new FLAC__int32[SAMPLES * m_format.channelCount];

    FLAC__StreamEncoderInitStatus init_status = m_encoder->init();
    if (init_status == FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        m_status = ENCODING;
        return true;
    }
    printf("SBEncoder::open(stream=%d) -- FLAC encoder error %s\n", m_stream, FLAC__StreamEncoderInitStatusString[init_status]);
    m_status = CLOSED;
    return false;
}

int SBEncoder::bytesAvailable() const
{
    if (m_packet)
        return (m_packet->size - m_consumed);
    return m_buffer->bytesAvailable();
}

void SBEncoder::close()
{
    m_encoder->finish();
    m_status = CLOSED;
}

int SBEncoder::readData(char* data, int maxlen)
{
    if (m_packet == nullptr) {
        m_packet = m_buffer->read();
        m_consumed = 0;
    }
    if (m_packet) {
        int s = m_packet->size - m_consumed;
        int r = (maxlen < s ? maxlen : s);
        memcpy(data, m_packet->data + m_consumed, r);
        m_consumed += r;
        if (m_consumed >= m_packet->size) {
            m_buffer->freePacket(m_packet);
            m_packet = nullptr;
        }
        return r;
    }
    return 0;
}

int SBEncoder::encode(const char* data, int len)
{
    bool ok = true;
    int samples = len / m_bytesPerFrame;
    while (ok && samples > 0) {
        int need = (samples > SAMPLES ? SAMPLES : static_cast<int>(samples));
        // convert the packed little-endian PCM samples into an interleaved FLAC__int32 buffer for libFLAC
        for (int i = 0; i < (need * 2 /*m_format.channelCount*/); i++) {
            switch (m_sampleSize) {
            case 8:
                m_pcm[i] = (unsigned char)(*data) - 128;
                data += 1;
                break;
            case 16:
                m_pcm[i] = read16le(data);
                data += 2;
                break;
            case 24:
                m_pcm[i] = read24le(data);
                data += 3;
                break;
            case 32:
                m_pcm[i] = read32le(data);
                data += 4;
                break;
            default:
                m_pcm[i] = 0;
            }
        }
        // feed samples to encoder
        ok = m_encoder->process_interleaved(m_pcm, need);
        samples -= need;
    }
    return len;
}

int SBEncoder::writeEncodedData(const char* data, int len)
{
    return m_buffer->write(data, len);
}

FLAC__StreamEncoderWriteStatus SBEncoder::SBEncoderStream::write_callback(const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame)
{
    int r = m_p->writeEncodedData((const char*)buffer, (int)bytes);
    return (r == (int)bytes ? FLAC__STREAM_ENCODER_WRITE_STATUS_OK : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR);
}

int SBEncoder::read(char* data, int maxlen, unsigned timeout)
{
    for (;;) {
        if (m_status == CLOSED) {
            printf("SBEncoder::read: encoder is closed\n");
            return 0;
        }
        if (m_stream != get_squeezebox_stream_id()) {
            printf("SBEncoder::read: stream mismatch (%d != %d)\n", m_stream, get_squeezebox_stream_id());
            usleep(1000); // 1 ms
            return 0;
        }
        if (bytesAvailable()) {
            if (!m_start_ms) {
                m_start_ms = get_sb_time_ms();
            }
            return readData(data, maxlen);
        } else if (m_status == CLOSING) {
            printf("All data consumed\n");
            close();
            return 0;
        }
        if (timeout) {
            if (!timeout--) {
                printf("SBEncoder::read: timeout\n");
                return 0;
            }
        }
        usleep(1000); // 1 ms
    }
}

int SBEncoder::write(const char* data, int len, unsigned timeout)
{
    for (;;) {
        if (m_status != ENCODING) {
            printf("SBEncoder::write: encoder not active\n");
            return 0;
        }
        if (m_stream != get_squeezebox_stream_id()) {
            printf("SBEncoder::write: stream mismatch (%d != %d)\n", m_stream, get_squeezebox_stream_id());
            usleep(1000); // 1 ms
            return 0;
        }
        if (len == 0) {
            printf("Reached end of stream\n");
            m_status = CLOSING;
            return 0;
        }
        uint32_t encoded_ms = (uint32_t)((uint64_t)m_total / (uint64_t)m_bytesPerFrame * (uint64_t)1000 / (uint64_t)44100);
        uint32_t played_ms = m_start_ms ? get_sb_time_ms() - m_start_ms : 0;
        if (encoded_ms < (played_ms + 2000)) {
            m_total += len;
            return encode(data, len);
        }
        if (timeout) {
            if (!timeout--) {
                printf("SBEncoder::write: timeout\n");
                return 0;
            }
        }
        usleep(1000); // 1 ms
    }
}
