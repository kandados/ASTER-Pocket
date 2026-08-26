#include "AsterAudio.h"
#include "AsterAudioWireControl.h"

#include <Arduino.h>

#include <cmath>
#include <limits>

extern "C" {
#include "driver/i2s_tdm.h"
#include "es7210_adc.h"
#include "audio_codec_if.h"
#include "esp_codec_dev_types.h"
}

namespace
{
    constexpr gpio_num_t PIN_I2S_MCLK = GPIO_NUM_42;
    constexpr gpio_num_t PIN_I2S_BCLK = GPIO_NUM_9;
    constexpr gpio_num_t PIN_I2S_WS   = GPIO_NUM_45;
    constexpr gpio_num_t PIN_I2S_DIN  = GPIO_NUM_10;

    constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    constexpr size_t AUDIO_FRAME_COUNT = 128;

    constexpr uint8_t SLOT_MIC_LEFT  = 0;
    constexpr uint8_t SLOT_REFERENCE = 1;
    constexpr uint8_t SLOT_MIC_RIGHT = 2;

    constexpr i2s_tdm_slot_mask_t TDM_SLOT_MASK =
        static_cast<i2s_tdm_slot_mask_t>(
            I2S_TDM_SLOT0 |
            I2S_TDM_SLOT1 |
            I2S_TDM_SLOT2 |
            I2S_TDM_SLOT3
        );
}

AsterAudioClass AsterAudio;

bool AsterAudioClass::beginMicrophone()
{
    if (_microphoneReady)
    {
        return true;
    }

    Serial.println(
        "[AsterAudio] Inicializando..."
    );

    // -----------------------------------------------------
    // I2S RX en modo TDM
    // -----------------------------------------------------

    i2s_chan_handle_t rxChannel = nullptr;

    i2s_chan_config_t channelConfig =
        I2S_CHANNEL_DEFAULT_CONFIG(
            I2S_PORT,
            I2S_ROLE_MASTER
        );

    esp_err_t result =
        i2s_new_channel(
            &channelConfig,
            nullptr,
            &rxChannel
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR creando I2S RX: %d\n",
            static_cast<int>(result)
        );

        return false;
    }

    i2s_tdm_clk_config_t clockConfig =
        I2S_TDM_CLK_DEFAULT_CONFIG(
            SAMPLE_RATE
        );

    clockConfig.mclk_multiple =
        I2S_MCLK_MULTIPLE_256;

    clockConfig.bclk_div =
        8;

    i2s_tdm_slot_config_t slotConfig =
        I2S_TDM_PHILIP_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO,
            TDM_SLOT_MASK
        );

    slotConfig.total_slot =
        TDM_SLOT_COUNT;

    i2s_tdm_config_t tdmConfig{};

    tdmConfig.clk_cfg =
        clockConfig;

    tdmConfig.slot_cfg =
        slotConfig;

    tdmConfig.gpio_cfg.mclk =
        PIN_I2S_MCLK;

    tdmConfig.gpio_cfg.bclk =
        PIN_I2S_BCLK;

    tdmConfig.gpio_cfg.ws =
        PIN_I2S_WS;

    tdmConfig.gpio_cfg.dout =
        I2S_GPIO_UNUSED;

    tdmConfig.gpio_cfg.din =
        PIN_I2S_DIN;

    tdmConfig.gpio_cfg.invert_flags.mclk_inv =
        false;

    tdmConfig.gpio_cfg.invert_flags.bclk_inv =
        false;

    tdmConfig.gpio_cfg.invert_flags.ws_inv =
        false;

    result =
        i2s_channel_init_tdm_mode(
            rxChannel,
            &tdmConfig
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR configurando TDM: %d\n",
            static_cast<int>(result)
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    result =
        i2s_channel_enable(
            rxChannel
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR activando I2S: %d\n",
            static_cast<int>(result)
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    Serial.println(
        "[AsterAudio] RX TDM 24 kHz activo."
    );

    // -----------------------------------------------------
    // Control I2C compartido con el táctil
    // -----------------------------------------------------

    const audio_codec_ctrl_if_t *control =
        asterAudioWireControlCreate();

    if (control == nullptr)
    {
        Serial.println(
            "[AsterAudio] ERROR creando control I2C."
        );

        i2s_channel_disable(
            rxChannel
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    // -----------------------------------------------------
    // ES7210
    //
    // Slot 0 = MIC1 frontal
    // Slot 1 = referencia acústica
    // Slot 2 = MIC2 frontal
    // Slot 3 = no conectado
    // -----------------------------------------------------

    es7210_codec_cfg_t codecConfig{};

    codecConfig.ctrl_if =
        control;

    codecConfig.master_mode =
        false;

    codecConfig.mic_selected =
        ES7210_SEL_MIC1 |
        ES7210_SEL_MIC2 |
        ES7210_SEL_MIC3;

    codecConfig.mclk_src =
        ES7210_MCLK_FROM_PAD;

    codecConfig.mclk_div =
        256;

    const audio_codec_if_t *codec =
        es7210_codec_new(
            &codecConfig
        );

    if (codec == nullptr)
    {
        Serial.println(
            "[AsterAudio] ERROR inicializando ES7210."
        );

        i2s_channel_disable(
            rxChannel
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    esp_codec_dev_sample_info_t sampleInfo{};

    sampleInfo.bits_per_sample =
        BITS_PER_SAMPLE;

    sampleInfo.channel =
        TDM_SLOT_COUNT;

    sampleInfo.channel_mask =
        0;

    sampleInfo.sample_rate =
        SAMPLE_RATE;

    sampleInfo.mclk_multiple =
        256;

    if (
        codec->set_fs(
            codec,
            &sampleInfo
        ) != 0
    )
    {
        Serial.println(
            "[AsterAudio] ERROR configurando ES7210."
        );

        i2s_channel_disable(
            rxChannel
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    if (
        codec->enable(
            codec,
            true
        ) != 0
    )
    {
        Serial.println(
            "[AsterAudio] ERROR activando ES7210."
        );

        i2s_channel_disable(
            rxChannel
        );

        i2s_del_channel(
            rxChannel
        );

        return false;
    }

    _rxChannel =
        reinterpret_cast<void *>(
            rxChannel
        );

    _microphoneCodec =
        reinterpret_cast<const void *>(
            codec
        );

    _microphoneReady =
        true;

    Serial.println(
        "[AsterAudio] ES7210 activo."
    );

    Serial.println(
        "[AsterAudio] Micrófonos preparados."
    );

    return true;
}

size_t AsterAudioClass::readMonoPcm(
    int16_t *destination,
    size_t maxSamples,
    uint32_t timeoutMs
)
{
    if (
        !_microphoneReady ||
        _rxChannel == nullptr ||
        destination == nullptr ||
        maxSamples == 0
    )
    {
        return 0;
    }

    constexpr size_t RAW_FRAMES = 128;

    int16_t rawSamples[
        RAW_FRAMES *
        TDM_SLOT_COUNT
    ];

    size_t totalSamples = 0;

    while (totalSamples < maxSamples)
    {
        size_t bytesRead = 0;

        const esp_err_t result =
            i2s_channel_read(
                reinterpret_cast<i2s_chan_handle_t>(
                    _rxChannel
                ),
                rawSamples,
                sizeof(rawSamples),
                &bytesRead,
                timeoutMs
            );

        if (
            result != ESP_OK ||
            bytesRead == 0
        )
        {
            break;
        }

        const size_t receivedSamples =
            bytesRead /
            sizeof(int16_t);

        const size_t receivedFrames =
            receivedSamples /
            TDM_SLOT_COUNT;

        for (
            size_t frame = 0;
            frame < receivedFrames &&
            totalSamples < maxSamples;
            ++frame
        )
        {
            const size_t base =
                frame *
                TDM_SLOT_COUNT;

            const int32_t left =
                rawSamples[
                    base +
                    SLOT_MIC_LEFT
                ];

            const int32_t right =
                rawSamples[
                    base +
                    SLOT_MIC_RIGHT
                ];

            destination[totalSamples++] =
                static_cast<int16_t>(
                    (left + right) / 2
                );
        }
    }

    return totalSamples;
}


bool AsterAudioClass::readMicrophoneStats(
    AsterAudioStats &stats,
    uint32_t windowMs
)
{
    stats = AsterAudioStats{};

    if (
        !_microphoneReady ||
        _rxChannel == nullptr
    )
    {
        return false;
    }

    stats.micLeft.minimum =
        std::numeric_limits<int16_t>::max();

    stats.micLeft.maximum =
        std::numeric_limits<int16_t>::min();

    stats.micRight.minimum =
        std::numeric_limits<int16_t>::max();

    stats.micRight.maximum =
        std::numeric_limits<int16_t>::min();

    stats.reference.minimum =
        std::numeric_limits<int16_t>::max();

    stats.reference.maximum =
        std::numeric_limits<int16_t>::min();

    uint64_t leftSquares = 0;
    uint64_t rightSquares = 0;
    uint64_t referenceSquares = 0;

    size_t totalFrames = 0;

    const uint32_t startMs =
        millis();

    do
    {
        int16_t samples[
            AUDIO_FRAME_COUNT *
            TDM_SLOT_COUNT
        ];

        size_t bytesRead = 0;

        const esp_err_t result =
            i2s_channel_read(
                reinterpret_cast<i2s_chan_handle_t>(
                    _rxChannel
                ),
                samples,
                sizeof(samples),
                &bytesRead,
                20
            );

        if (
            result != ESP_OK ||
            bytesRead == 0
        )
        {
            continue;
        }

        const size_t sampleCount =
            bytesRead /
            sizeof(int16_t);

        const size_t frameCount =
            sampleCount /
            TDM_SLOT_COUNT;

        for (
            size_t frame = 0;
            frame < frameCount;
            ++frame
        )
        {
            const size_t base =
                frame *
                TDM_SLOT_COUNT;

            const int16_t left =
                samples[
                    base +
                    SLOT_MIC_LEFT
                ];

            const int16_t reference =
                samples[
                    base +
                    SLOT_REFERENCE
                ];

            const int16_t right =
                samples[
                    base +
                    SLOT_MIC_RIGHT
                ];

            if (left < stats.micLeft.minimum)
                stats.micLeft.minimum = left;

            if (left > stats.micLeft.maximum)
                stats.micLeft.maximum = left;

            if (right < stats.micRight.minimum)
                stats.micRight.minimum = right;

            if (right > stats.micRight.maximum)
                stats.micRight.maximum = right;

            if (reference < stats.reference.minimum)
                stats.reference.minimum = reference;

            if (reference > stats.reference.maximum)
                stats.reference.maximum = reference;

            const int32_t left32 =
                static_cast<int32_t>(left);

            const int32_t right32 =
                static_cast<int32_t>(right);

            const int32_t reference32 =
                static_cast<int32_t>(reference);

            const uint32_t leftAbs =
                static_cast<uint32_t>(
                    left32 < 0 ? -left32 : left32
                );

            const uint32_t rightAbs =
                static_cast<uint32_t>(
                    right32 < 0 ? -right32 : right32
                );

            const uint32_t referenceAbs =
                static_cast<uint32_t>(
                    reference32 < 0
                        ? -reference32
                        : reference32
                );

            if (leftAbs > stats.micLeft.peak)
                stats.micLeft.peak = leftAbs;

            if (rightAbs > stats.micRight.peak)
                stats.micRight.peak = rightAbs;

            if (referenceAbs > stats.reference.peak)
                stats.reference.peak = referenceAbs;

            leftSquares +=
                static_cast<uint64_t>(
                    left32 * left32
                );

            rightSquares +=
                static_cast<uint64_t>(
                    right32 * right32
                );

            referenceSquares +=
                static_cast<uint64_t>(
                    reference32 * reference32
                );
        }

        totalFrames +=
            frameCount;
    }
    while (
        millis() - startMs <
        windowMs
    );

    if (totalFrames == 0)
    {
        return false;
    }

    stats.frames =
        totalFrames;

    stats.micLeft.rms =
        static_cast<uint32_t>(
            std::sqrt(
                static_cast<double>(
                    leftSquares
                ) /
                totalFrames
            )
        );

    stats.micRight.rms =
        static_cast<uint32_t>(
            std::sqrt(
                static_cast<double>(
                    rightSquares
                ) /
                totalFrames
            )
        );

    stats.reference.rms =
        static_cast<uint32_t>(
            std::sqrt(
                static_cast<double>(
                    referenceSquares
                ) /
                totalFrames
            )
        );

    return true;
}

bool AsterAudioClass::readMicrophoneLevels(
    uint32_t &micLeft,
    uint32_t &micRight,
    uint32_t &reference,
    uint32_t timeoutMs
)
{
    AsterAudioStats stats;

    if (
        !readMicrophoneStats(
            stats,
            timeoutMs
        )
    )
    {
        micLeft = 0;
        micRight = 0;
        reference = 0;

        return false;
    }

    micLeft =
        stats.micLeft.rms;

    micRight =
        stats.micRight.rms;

    reference =
        stats.reference.rms;

    return true;
}

bool AsterAudioClass::isMicrophoneReady() const
{
    return _microphoneReady;
}
