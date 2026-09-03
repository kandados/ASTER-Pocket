#pragma once

#include <Arduino.h>

struct AsterVoiceStats
{
    int16_t minimum = 0;
    int16_t maximum = 0;
    uint32_t peak = 0;
    uint32_t rms = 0;
};

class AsterVoiceClass
{
public:
    bool record(uint32_t durationMs);

    bool recordUntilSilence(
        uint32_t maxDurationMs = 10000,
        uint32_t finalSilenceMs = 900,
        uint32_t waitForVoiceMs = 3000
    );

    void clear();

    bool hasRecording() const;

    const int16_t *data() const;

    size_t sampleCount() const;

    size_t byteCount() const;

    uint32_t durationMs() const;

    const AsterVoiceStats &stats() const;

    bool dumpBase64() const;

private:
    int16_t *_buffer = nullptr;

    size_t _sampleCount = 0;

    uint32_t _durationMs = 0;

    AsterVoiceStats _stats;
};

extern AsterVoiceClass AsterVoice;
