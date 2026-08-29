#include "AsterAudio.h"
#include "AsterAudioWireControl.h"

#include <Arduino.h>

#include <cmath>
#include <limits>

extern "C" {
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "es7210_adc.h"
#include "audio_codec_if.h"
#include "esp_codec_dev_types.h"
#include "es8311.h"
#include <esp_err.h>
}

namespace
{
    constexpr gpio_num_t PIN_I2S_MCLK = GPIO_NUM_42;
    constexpr gpio_num_t PIN_I2S_BCLK = GPIO_NUM_9;
    constexpr gpio_num_t PIN_I2S_WS   = GPIO_NUM_45;
    constexpr gpio_num_t PIN_I2S_DOUT = GPIO_NUM_8;
    constexpr gpio_num_t PIN_I2S_DIN  = GPIO_NUM_10;

    constexpr gpio_num_t PIN_SPEAKER_PA = GPIO_NUM_46;

    constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    constexpr int16_t TEST_TONE_AMPLITUDE = 6000;

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
            Serial.printf(

                "[AsterAudio] RX I2S ERROR: err=%d (%s), bytes=%lu, timeout=%lu ms\n",

                static_cast<int>(result),

                esp_err_to_name(result),

                static_cast<unsigned long>(
                    bytesRead
                ),

                static_cast<unsigned long>(
                    timeoutMs
                )

            );

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


bool AsterAudioClass::beginSpeaker()
{
    if (_speakerReady)
    {
        return true;
    }


    Serial.println(
        "[AsterAudio] Inicializando ES8311..."
    );

    pinMode(
        PIN_SPEAKER_PA,
        OUTPUT
    );

    // Mantener el amplificador apagado durante la
    // configuración del codec para reducir pops.
    digitalWrite(
        PIN_SPEAKER_PA,
        LOW
    );

    es8311_handle_t codec =
        es8311_create(
            0,
            ES8311_ADDRRES_0
        );

    if (codec == nullptr)
    {
        Serial.println(
            "[AsterAudio] ERROR creando ES8311."
        );

        return false;
    }

    const es8311_clock_config_t clockConfig =
    {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency =
            static_cast<int>(
                SAMPLE_RATE * 256
            ),
        .sample_frequency =
            static_cast<int>(
                SAMPLE_RATE
            )
    };

    esp_err_t result =
        es8311_init(
            codec,
            &clockConfig,
            ES8311_RESOLUTION_16,
            ES8311_RESOLUTION_16
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR inicializando ES8311: %d\n",
            static_cast<int>(result)
        );

        es8311_delete(codec);
        return false;
    }

    result =
        es8311_sample_frequency_config(
            codec,
            clockConfig.mclk_frequency,
            clockConfig.sample_frequency
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR configurando frecuencia ES8311: %d\n",
            static_cast<int>(result)
        );

        es8311_delete(codec);
        return false;
    }

    result =
        es8311_microphone_config(
            codec,
            false
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR configurando ES8311: %d\n",
            static_cast<int>(result)
        );

        es8311_delete(codec);
        return false;
    }

    result =
        es8311_voice_volume_set(
            codec,
            static_cast<int>(_speakerVolume),
            nullptr
        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR ajustando volumen ES8311: %d\n",
            static_cast<int>(result)
        );

        es8311_delete(codec);
        return false;
    }

    result =
        es8311_voice_mute(

            codec,

            true

        );

    if (result != ESP_OK)
    {
        Serial.printf(
            "[AsterAudio] ERROR silenciando ES8311: %d\n",
            static_cast<int>(result)
        );

        es8311_delete(codec);
        return false;
    }

    digitalWrite(

        PIN_SPEAKER_PA,

        LOW

    );

    delay(
        10
    );

    _speakerCodec =
        reinterpret_cast<void *>(
            codec
        );

    _speakerReady =
        true;

    Serial.printf(
        "[AsterAudio] ES8311 preparado a %lu Hz. PA GPIO46=LOW.\n",
        static_cast<unsigned long>(
            SAMPLE_RATE
        )
    );

    return true;
}



bool AsterAudioClass::startSpeakerPlayback()

{

    if (

        !_speakerReady ||

        _speakerCodec == nullptr

    )

    {

        Serial.println(

            "[AsterAudio] ERROR: altavoz no preparado."

        );

        return false;

    }


    Serial.println(

        "[AsterAudio] Cambiando I2S: RX TDM -> TX STD..."

    );


    // -----------------------------------------------------
    // Detener RX TDM del ES7210
    // -----------------------------------------------------

    if (_rxChannel != nullptr)

    {

        i2s_chan_handle_t rxChannel =

            reinterpret_cast<i2s_chan_handle_t>(

                _rxChannel

            );


        i2s_channel_disable(

            rxChannel

        );


        i2s_del_channel(

            rxChannel

        );


        _rxChannel = nullptr;

    }


    _microphoneReady = false;


    // -----------------------------------------------------
    // Crear TX STD para ES8311
    // -----------------------------------------------------

    i2s_chan_handle_t txChannel = nullptr;


    i2s_chan_config_t channelConfig =

        I2S_CHANNEL_DEFAULT_CONFIG(

            I2S_PORT,

            I2S_ROLE_MASTER

        );


    channelConfig.auto_clear =

        true;


    esp_err_t result =

        i2s_new_channel(

            &channelConfig,

            &txChannel,

            nullptr

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR creando I2S TX: %d\n",

            static_cast<int>(result)

        );


        return false;

    }


    i2s_std_config_t txConfig{};


    txConfig.clk_cfg =

        I2S_STD_CLK_DEFAULT_CONFIG(

            SAMPLE_RATE

        );


    txConfig.clk_cfg.mclk_multiple =

        I2S_MCLK_MULTIPLE_256;


    txConfig.slot_cfg =

        I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(

            I2S_DATA_BIT_WIDTH_16BIT,

            I2S_SLOT_MODE_STEREO

        );


    txConfig.gpio_cfg.mclk =

        PIN_I2S_MCLK;


    txConfig.gpio_cfg.bclk =

        PIN_I2S_BCLK;


    txConfig.gpio_cfg.ws =

        PIN_I2S_WS;


    txConfig.gpio_cfg.dout =

        PIN_I2S_DOUT;


    txConfig.gpio_cfg.din =

        I2S_GPIO_UNUSED;


    txConfig.gpio_cfg.invert_flags.mclk_inv =

        false;


    txConfig.gpio_cfg.invert_flags.bclk_inv =

        false;


    txConfig.gpio_cfg.invert_flags.ws_inv =

        false;


    result =

        i2s_channel_init_std_mode(

            txChannel,

            &txConfig

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR configurando I2S TX: %d\n",

            static_cast<int>(result)

        );


        i2s_del_channel(

            txChannel

        );


        return false;

    }


    result =

        i2s_channel_enable(

            txChannel

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR activando I2S TX: %d\n",

            static_cast<int>(result)

        );


        i2s_del_channel(

            txChannel

        );


        return false;

    }


    _txChannel =

        reinterpret_cast<void *>(

            txChannel

        );


    // -----------------------------------------------------
    // Activar ES8311 + PA
    // -----------------------------------------------------

    digitalWrite(

        PIN_SPEAKER_PA,

        HIGH

    );


    delay(

        5

    );


    result =

        es8311_voice_mute(

            reinterpret_cast<es8311_handle_t>(

                _speakerCodec

            ),

            false

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR activando ES8311: %d\n",

            static_cast<int>(result)

        );


        digitalWrite(

            PIN_SPEAKER_PA,

            LOW

        );


        i2s_channel_disable(

            txChannel

        );


        i2s_del_channel(

            txChannel

        );


        _txChannel = nullptr;


        return false;

    }


    Serial.println(

        "[AsterAudio] TX STD activo. PA GPIO46=HIGH."

    );


    return true;

}


bool AsterAudioClass::stopSpeakerPlayback()

{

    if (!_speakerReady)

    {

        return false;

    }


    bool ok = true;


    // -----------------------------------------------------
    // Silenciar salida
    // -----------------------------------------------------

    if (_txChannel != nullptr)

    {

        int16_t silence[128]{};


        playMonoPcm(

            silence,

            128,

            1000

        );

    }


    if (_speakerCodec != nullptr)

    {

        const esp_err_t muteResult =

            es8311_voice_mute(

                reinterpret_cast<es8311_handle_t>(

                    _speakerCodec

                ),

                true

            );


        if (muteResult != ESP_OK)

        {

            Serial.printf(

                "[AsterAudio] ERROR silenciando ES8311: %d\n",

                static_cast<int>(muteResult)

            );


            ok = false;

        }

    }


    digitalWrite(

        PIN_SPEAKER_PA,

        LOW

    );


    // -----------------------------------------------------
    // Destruir TX STD
    // -----------------------------------------------------

    if (_txChannel != nullptr)

    {

        i2s_chan_handle_t txChannel =

            reinterpret_cast<i2s_chan_handle_t>(

                _txChannel

            );


        i2s_channel_disable(

            txChannel

        );


        i2s_del_channel(

            txChannel

        );


        _txChannel = nullptr;

    }


    Serial.println(

        "[AsterAudio] TX detenido. PA GPIO46=LOW."

    );


    // -----------------------------------------------------
    // Restaurar RX TDM del ES7210
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

            "[AsterAudio] ERROR restaurando I2S RX: %d\n",

            static_cast<int>(result)

        );


        _microphoneReady = false;

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

            "[AsterAudio] ERROR reconfigurando RX TDM: %d\n",

            static_cast<int>(result)

        );


        i2s_del_channel(

            rxChannel

        );


        _microphoneReady = false;

        return false;

    }


    result =

        i2s_channel_enable(

            rxChannel

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR reactivando RX TDM: %d\n",

            static_cast<int>(result)

        );


        i2s_del_channel(

            rxChannel

        );


        _microphoneReady = false;

        return false;

    }


    _rxChannel =

        reinterpret_cast<void *>(

            rxChannel

        );


    _microphoneReady =

        true;


    Serial.println(

        "[AsterAudio] RX TDM restaurado. Micrófonos listos."

    );


    return ok;

}


bool AsterAudioClass::playMonoPcm(
    const int16_t *samples,
    size_t sampleCount,
    uint32_t timeoutMs
)
{
    if (
        !_speakerReady ||
        _txChannel == nullptr ||
        samples == nullptr ||
        sampleCount == 0
    )
    {
        return false;
    }

    constexpr size_t CHUNK_FRAMES =
        256;

    int16_t stereo[
        CHUNK_FRAMES * 2
    ];

    size_t offset =
        0;

    while (offset < sampleCount)
    {
        const size_t remaining =
            sampleCount - offset;

        const size_t frames =
            remaining < CHUNK_FRAMES
                ? remaining
                : CHUNK_FRAMES;

        for (
            size_t i = 0;
            i < frames;
            ++i
        )
        {
            const int16_t sample =
                samples[
                    offset + i
                ];

            stereo[
                i * 2
            ] = sample;

            stereo[
                i * 2 + 1
            ] = sample;
        }

        const size_t bytesToWrite =
            frames *
            2 *
            sizeof(int16_t);

        size_t bytesWritten =
            0;

        const esp_err_t result =
            i2s_channel_write(
                reinterpret_cast<i2s_chan_handle_t>(
                    _txChannel
                ),
                stereo,
                bytesToWrite,
                &bytesWritten,
                timeoutMs
            );

        if (
            result != ESP_OK ||
            bytesWritten != bytesToWrite
        )
        {
            Serial.printf(
                "[AsterAudio] ERROR escribiendo I2S TX: err=%d bytes=%lu/%lu\n",
                static_cast<int>(result),
                static_cast<unsigned long>(
                    bytesWritten
                ),
                static_cast<unsigned long>(
                    bytesToWrite
                )
            );

            return false;
        }

        offset +=
            frames;
    }

    return true;
}


bool AsterAudioClass::playTestTone(
    uint32_t frequencyHz,
    uint32_t durationMs
)
{
    if (
        !_speakerReady ||
        frequencyHz == 0 ||
        durationMs == 0
    )
    {
        return false;
    }

    if (!startSpeakerPlayback())

    {

        return false;

    }


    Serial.printf(
        "[AsterAudio] Tono de prueba: %lu Hz / %lu ms\n",
        static_cast<unsigned long>(
            frequencyHz
        ),
        static_cast<unsigned long>(
            durationMs
        )
    );

    constexpr size_t CHUNK_FRAMES =
        256;

    constexpr double ASTER_TWO_PI =
        6.28318530717958647692;

    int16_t mono[
        CHUNK_FRAMES
    ];

    const size_t totalFrames =
        static_cast<size_t>(
            (
                static_cast<uint64_t>(
                    SAMPLE_RATE
                ) *
                durationMs
            ) /
            1000
        );

    const double phaseStep =
        ASTER_TWO_PI *
        static_cast<double>(
            frequencyHz
        ) /
        static_cast<double>(
            SAMPLE_RATE
        );

    double phase =
        0.0;

    size_t generated =
        0;

    while (generated < totalFrames)
    {
        const size_t remaining =
            totalFrames - generated;

        const size_t frames =
            remaining < CHUNK_FRAMES
                ? remaining
                : CHUNK_FRAMES;

        for (
            size_t i = 0;
            i < frames;
            ++i
        )
        {
            mono[i] =
                static_cast<int16_t>(
                    std::sin(
                        phase
                    ) *
                    TEST_TONE_AMPLITUDE
                );

            phase +=
                phaseStep;

            if (phase >= ASTER_TWO_PI)
            {
                phase -=
                    ASTER_TWO_PI;
            }
        }

        if (
            !playMonoPcm(
                mono,
                frames,
                1000
            )
        )
        {
            stopSpeakerPlayback();

            return false;
        }

        generated +=
            frames;
    }

    // Un pequeño bloque de silencio deja la salida
    // en cero después del tono.
    int16_t silence[128]{};

    playMonoPcm(
        silence,
        128,
        1000
    );

    // Finalizada la prueba: volver al estado silencioso.

    stopSpeakerPlayback();

    Serial.println(
        "[AsterAudio] Tono enviado al altavoz."
    );

    Serial.println(
        "[AsterAudio] Altavoz silenciado. PA GPIO46=LOW."
    );

    return true;
}



bool AsterAudioClass::setSpeakerVolume(

    uint8_t volume

)

{

    if (volume > 100)

    {

        volume = 100;

    }


    _speakerVolume =

        volume;


    // El valor puede configurarse incluso antes de
    // inicializar el ES8311. Se aplicará en beginSpeaker().

    if (

        !_speakerReady ||

        _speakerCodec == nullptr

    )

    {

        Serial.printf(

            "[AsterAudio] Volumen preparado: %u%%\n",

            static_cast<unsigned>(

                _speakerVolume

            )

        );


        return true;

    }


    const esp_err_t result =

        es8311_voice_volume_set(

            reinterpret_cast<es8311_handle_t>(

                _speakerCodec

            ),

            static_cast<int>(

                _speakerVolume

            ),

            nullptr

        );


    if (result != ESP_OK)

    {

        Serial.printf(

            "[AsterAudio] ERROR ajustando volumen ES8311: %d\n",

            static_cast<int>(result)

        );


        return false;

    }


    Serial.printf(

        "[AsterAudio] Volumen del altavoz: %u%%\n",

        static_cast<unsigned>(

            _speakerVolume

        )

    );


    return true;

}


uint8_t AsterAudioClass::speakerVolume() const

{

    return _speakerVolume;

}


bool AsterAudioClass::isMicrophoneReady() const
{
    return _microphoneReady;
}

bool AsterAudioClass::isSpeakerReady() const
{
    return _speakerReady;
}
