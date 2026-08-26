#include "AsterVoice.h"
#include "AsterAudio.h"

#include <cmath>
#include <limits>
#include <mbedtls/base64.h>

#include <esp_heap_caps.h>

AsterVoiceClass AsterVoice;

bool AsterVoiceClass::record(
    uint32_t durationMs
)
{
    clear();

    if (
        !AsterAudio.isMicrophoneReady() ||
        durationMs == 0
    )
    {
        Serial.println(
            "[AsterVoice] ERROR: micrófono no disponible."
        );

        return false;
    }

    const size_t targetSamples =
        static_cast<size_t>(
            (
                static_cast<uint64_t>(
                    AsterAudio.SAMPLE_RATE
                ) *
                durationMs
            ) /
            1000ULL
        );

    const size_t targetBytes =
        targetSamples *
        sizeof(int16_t);

    Serial.println();
    Serial.println(
        "[AsterVoice] Preparando grabación..."
    );

    Serial.printf(
        "[AsterVoice] Duración objetivo: %lu ms\n",
        static_cast<unsigned long>(
            durationMs
        )
    );

    Serial.printf(
        "[AsterVoice] Muestras objetivo: %lu\n",
        static_cast<unsigned long>(
            targetSamples
        )
    );

    Serial.printf(
        "[AsterVoice] Memoria necesaria: %lu bytes\n",
        static_cast<unsigned long>(
            targetBytes
        )
    );

    _buffer =
        static_cast<int16_t *>(
            heap_caps_malloc(
                targetBytes,
                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );

    if (_buffer == nullptr)
    {
        Serial.println(
            "[AsterVoice] ERROR reservando PSRAM."
        );

        return false;
    }

    Serial.println(
        "[AsterVoice] Grabando..."
    );

    const uint32_t captureStart =
        millis();

    _sampleCount =
        AsterAudio.readMonoPcm(
            _buffer,
            targetSamples,
            1000
        );

    const uint32_t captureElapsed =
        millis() -
        captureStart;

    if (_sampleCount == 0)
    {
        Serial.println(
            "[AsterVoice] ERROR: no se capturaron muestras."
        );

        clear();

        return false;
    }

    _durationMs =
        static_cast<uint32_t>(
            (
                static_cast<uint64_t>(
                    _sampleCount
                ) *
                1000ULL
            ) /
            AsterAudio.SAMPLE_RATE
        );

    _stats.minimum =
        std::numeric_limits<int16_t>::max();

    _stats.maximum =
        std::numeric_limits<int16_t>::min();

    _stats.peak =
        0;

    uint64_t squareSum =
        0;

    for (
        size_t i = 0;
        i < _sampleCount;
        ++i
    )
    {
        const int16_t sample =
            _buffer[i];

        if (
            sample <
            _stats.minimum
        )
        {
            _stats.minimum =
                sample;
        }

        if (
            sample >
            _stats.maximum
        )
        {
            _stats.maximum =
                sample;
        }

        const int32_t sample32 =
            static_cast<int32_t>(
                sample
            );

        const uint32_t absolute =
            static_cast<uint32_t>(
                sample32 < 0
                    ? -sample32
                    : sample32
            );

        if (
            absolute >
            _stats.peak
        )
        {
            _stats.peak =
                absolute;
        }

        squareSum +=
            static_cast<uint64_t>(
                sample32 *
                sample32
            );
    }

    _stats.rms =
        static_cast<uint32_t>(
            std::sqrt(
                static_cast<double>(
                    squareSum
                ) /
                _sampleCount
            )
        );

    Serial.println(
        "[AsterVoice] Grabación terminada."
    );

    Serial.printf(
        "[AsterVoice] Capturadas: %lu muestras\n",
        static_cast<unsigned long>(
            _sampleCount
        )
    );

    Serial.printf(
        "[AsterVoice] Bytes almacenados: %lu\n",
        static_cast<unsigned long>(
            byteCount()
        )
    );

    Serial.printf(
        "[AsterVoice] Duración PCM: %lu ms\n",
        static_cast<unsigned long>(
            _durationMs
        )
    );

    Serial.printf(
        "[AsterVoice] Tiempo real captura: %lu ms\n",
        static_cast<unsigned long>(
            captureElapsed
        )
    );

    Serial.printf(
        "[AsterVoice] min=%d max=%d peak=%lu rms=%lu\n",
        _stats.minimum,
        _stats.maximum,
        static_cast<unsigned long>(
            _stats.peak
        ),
        static_cast<unsigned long>(
            _stats.rms
        )
    );

    Serial.printf(
        "[AsterVoice] PSRAM libre después: %lu bytes\n",
        static_cast<unsigned long>(
            ESP.getFreePsram()
        )
    );

    Serial.println(
        "[AsterVoice] Grabación OK."
    );

    return true;
}

bool AsterVoiceClass::dumpBase64() const
{
    if (!hasRecording())
    {
        Serial.println(
            "[AsterVoiceDump] ERROR: no hay grabación."
        );

        return false;
    }

    const uint8_t *bytes =
        reinterpret_cast<const uint8_t *>(
            _buffer
        );

    const size_t totalBytes =
        byteCount();

    constexpr size_t INPUT_CHUNK =
        768;

    unsigned char encoded[1100];

    Serial.printf(
        "[AsterVoiceDump] BEGIN bytes=%lu rate=%lu bits=16 channels=1\n",
        static_cast<unsigned long>(
            totalBytes
        ),
        static_cast<unsigned long>(
            AsterAudio.SAMPLE_RATE
        )
    );

    for (
        size_t offset = 0;
        offset < totalBytes;
        offset += INPUT_CHUNK
    )
    {
        const size_t remaining =
            totalBytes - offset;

        const size_t chunk =
            remaining < INPUT_CHUNK
                ? remaining
                : INPUT_CHUNK;

        size_t encodedLength =
            0;

        const int result =
            mbedtls_base64_encode(
                encoded,
                sizeof(encoded),
                &encodedLength,
                bytes + offset,
                chunk
            );

        if (result != 0)
        {
            Serial.printf(
                "[AsterVoiceDump] ERROR base64: %d\n",
                result
            );

            return false;
        }

        Serial.write(
            encoded,
            encodedLength
        );

        Serial.println();
    }

    Serial.println(
        "[AsterVoiceDump] END"
    );

    return true;
}


void AsterVoiceClass::clear()
{
    if (_buffer != nullptr)
    {
        free(
            _buffer
        );

        _buffer =
            nullptr;
    }

    _sampleCount =
        0;

    _durationMs =
        0;

    _stats =
        AsterVoiceStats{};
}

bool AsterVoiceClass::hasRecording() const
{
    return (
        _buffer != nullptr &&
        _sampleCount > 0
    );
}

const int16_t *AsterVoiceClass::data() const
{
    return _buffer;
}

size_t AsterVoiceClass::sampleCount() const
{
    return _sampleCount;
}

size_t AsterVoiceClass::byteCount() const
{
    return (
        _sampleCount *
        sizeof(int16_t)
    );
}

uint32_t AsterVoiceClass::durationMs() const
{
    return _durationMs;
}

const AsterVoiceStats &
AsterVoiceClass::stats() const
{
    return _stats;
}
