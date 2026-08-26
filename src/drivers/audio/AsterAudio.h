#pragma once

#include <Arduino.h>

struct AsterAudioChannelStats
{
    int16_t minimum = 0;
    int16_t maximum = 0;

    uint32_t peak = 0;
    uint32_t rms = 0;
};

struct AsterAudioStats
{
    AsterAudioChannelStats micLeft;
    AsterAudioChannelStats micRight;
    AsterAudioChannelStats reference;

    size_t frames = 0;
};

class AsterAudioClass
{
public:
    static constexpr uint32_t SAMPLE_RATE = 24000;
    static constexpr uint8_t BITS_PER_SAMPLE = 16;
    static constexpr uint8_t TDM_SLOT_COUNT = 4;

    bool beginMicrophone();

    bool readMicrophoneLevels(
        uint32_t &micLeft,
        uint32_t &micRight,
        uint32_t &reference,
        uint32_t timeoutMs = 50
    );

    bool readMicrophoneStats(
        AsterAudioStats &stats,
        uint32_t timeoutMs = 50
    );

    bool isMicrophoneReady() const;

private:
    void *_rxChannel = nullptr;
    const void *_microphoneCodec = nullptr;

    bool _microphoneReady = false;
};

extern AsterAudioClass AsterAudio;
