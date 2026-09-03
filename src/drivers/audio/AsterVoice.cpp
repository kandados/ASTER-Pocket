#include "AsterVoice.h"
#include "AsterAudio.h"

#include <cmath>
#include <cstring>
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


bool AsterVoiceClass::recordUntilSilence(
    uint32_t maxDurationMs,
    uint32_t finalSilenceMs,
    uint32_t waitForVoiceMs
)
{
    clear();

    if (
        !AsterAudio.isMicrophoneReady() ||
        maxDurationMs == 0 ||
        finalSilenceMs == 0 ||
        waitForVoiceMs == 0
    )
    {
        Serial.println(
            "[AsterVoice] ERROR: parámetros VAD inválidos."
        );

        return false;
    }

    // 1024 muestras @ 24 kHz = ~42,7 ms.
    // Es múltiplo exacto de los 128 frames utilizados
    // internamente por readMonoPcm().
    constexpr size_t CHUNK_SAMPLES = 1024;

    constexpr size_t CALIBRATION_BLOCKS = 7;

    constexpr uint8_t VOICE_CONFIRM_BLOCKS = 2;

    constexpr uint32_t PRE_ROLL_MS = 200;
    constexpr uint32_t POST_ROLL_MS = 200;

    const size_t maxSamples =
        static_cast<size_t>(
            (
                static_cast<uint64_t>(
                    AsterAudio.SAMPLE_RATE
                ) *
                maxDurationMs
            ) /
            1000ULL
        );

    const size_t maxBytes =
        maxSamples *
        sizeof(int16_t);

    Serial.println();
    Serial.println(
        "[AsterVoice] Grabación con detección automática."
    );

    Serial.printf(
        "[AsterVoice] Máximo absoluto: %lu ms\n",
        static_cast<unsigned long>(
            maxDurationMs
        )
    );

    Serial.printf(
        "[AsterVoice] Silencio para finalizar: %lu ms\n",
        static_cast<unsigned long>(
            finalSilenceMs
        )
    );

    Serial.printf(
        "[AsterVoice] Espera máxima de voz: %lu ms\n",
        static_cast<unsigned long>(
            waitForVoiceMs
        )
    );

    _buffer =
        static_cast<int16_t *>(
            heap_caps_malloc(
                maxBytes,
                MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT
            )
        );

    if (_buffer == nullptr)
    {
        Serial.println(
            "[AsterVoice] ERROR reservando PSRAM para VAD."
        );

        return false;
    }

    // -----------------------------------------------------
    // Medidor de energía de un bloque
    // -----------------------------------------------------

    auto measureBlock =
        [](
            const int16_t *samples,
            size_t count,
            uint32_t &rms,
            uint32_t &peak
        )
        {
            uint64_t squareSum = 0;
            uint32_t blockPeak = 0;

            for (size_t i = 0; i < count; ++i)
            {
                const int32_t value =
                    static_cast<int32_t>(
                        samples[i]
                    );

                const uint32_t absolute =
                    static_cast<uint32_t>(
                        value < 0
                            ? -value
                            : value
                    );

                if (absolute > blockPeak)
                {
                    blockPeak = absolute;
                }

                squareSum +=
                    static_cast<uint64_t>(
                        value * value
                    );
            }

            rms =
                count > 0
                ? static_cast<uint32_t>(
                    std::sqrt(
                        static_cast<double>(
                            squareSum
                        ) /
                        count
                    )
                )
                : 0;

            peak = blockPeak;
        };

    // -----------------------------------------------------
    // Calibrar ruido ambiente
    // -----------------------------------------------------

    Serial.println(
        "[AsterVoice] Calibrando ruido ambiente..."
    );

    int16_t calibration[
        CHUNK_SAMPLES
    ];

    uint64_t calibrationRmsSum = 0;
    uint64_t calibrationPeakSum = 0;
    size_t calibrationValid = 0;

    for (
        size_t block = 0;
        block < CALIBRATION_BLOCKS;
        ++block
    )
    {
        const size_t captured =
            AsterAudio.readMonoPcm(
                calibration,
                CHUNK_SAMPLES,
                250
            );

        if (captured == 0)
        {
            continue;
        }

        uint32_t blockRms = 0;
        uint32_t blockPeak = 0;

        measureBlock(
            calibration,
            captured,
            blockRms,
            blockPeak
        );

        calibrationRmsSum += blockRms;
        calibrationPeakSum += blockPeak;

        ++calibrationValid;
    }

    if (calibrationValid == 0)
    {
        Serial.println(
            "[AsterVoice] ERROR calibrando micrófono."
        );

        clear();

        return false;
    }

    const uint32_t noiseRms =
        static_cast<uint32_t>(
            calibrationRmsSum /
            calibrationValid
        );

    const uint32_t noisePeak =
        static_cast<uint32_t>(
            calibrationPeakSum /
            calibrationValid
        );

    uint32_t rmsThreshold =
        noiseRms * 2U + 1U;

    if (rmsThreshold < 4U)
    {
        rmsThreshold = 4U;
    }

    uint32_t peakThreshold =
        noisePeak * 2U + 4U;

    if (peakThreshold < 16U)
    {
        peakThreshold = 16U;
    }

    Serial.printf(
        "[AsterVoice] Ruido: rms=%lu peak=%lu\n",
        static_cast<unsigned long>(
            noiseRms
        ),
        static_cast<unsigned long>(
            noisePeak
        )
    );

    Serial.printf(
        "[AsterVoice] Umbral voz: rms=%lu o peak=%lu\n",
        static_cast<unsigned long>(
            rmsThreshold
        ),
        static_cast<unsigned long>(
            peakThreshold
        )
    );

    Serial.println(
        "[AsterVoice] HABLA AHORA."
    );

    // -----------------------------------------------------
    // Captura dinámica
    // -----------------------------------------------------

    const uint32_t captureStart =
        millis();

    bool voiceDetected = false;
    bool silenceDetected = false;

    uint8_t consecutiveVoiceBlocks = 0;

    size_t candidateVoiceStart = 0;
    size_t voiceStartSample = 0;
    size_t lastVoiceEndSample = 0;

    uint32_t maxObservedRms = 0;
    uint32_t maxObservedPeak = 0;

    _sampleCount = 0;

    while (_sampleCount < maxSamples)
    {
        const size_t remaining =
            maxSamples -
            _sampleCount;

        const size_t requested =
            remaining < CHUNK_SAMPLES
                ? remaining
                : CHUNK_SAMPLES;

        const size_t blockStart =
            _sampleCount;

        const size_t captured =
            AsterAudio.readMonoPcm(
                _buffer +
                _sampleCount,
                requested,
                250
            );

        if (captured == 0)
        {
            Serial.println(
                "[AsterVoice] ERROR leyendo bloque PCM."
            );

            clear();

            return false;
        }

        _sampleCount += captured;

        uint32_t blockRms = 0;
        uint32_t blockPeak = 0;

        measureBlock(
            _buffer + blockStart,
            captured,
            blockRms,
            blockPeak
        );

        if (blockRms > maxObservedRms)
        {
            maxObservedRms = blockRms;
        }

        if (blockPeak > maxObservedPeak)
        {
            maxObservedPeak = blockPeak;
        }

        const bool activeVoice =
            (
                blockRms >= rmsThreshold ||
                blockPeak >= peakThreshold
            );

        if (!voiceDetected)
        {
            if (activeVoice)
            {
                if (consecutiveVoiceBlocks == 0)
                {
                    candidateVoiceStart =
                        blockStart;
                }

                ++consecutiveVoiceBlocks;

                if (
                    consecutiveVoiceBlocks >=
                    VOICE_CONFIRM_BLOCKS
                )
                {
                    voiceDetected = true;

                    voiceStartSample =
                        candidateVoiceStart;

                    lastVoiceEndSample =
                        _sampleCount;

                    Serial.printf(
                        "[AsterVoice] Voz detectada tras %lu ms.\n",
                        static_cast<unsigned long>(
                            millis() -
                            captureStart
                        )
                    );
                }
            }
            else
            {
                consecutiveVoiceBlocks = 0;
            }

            if (
                !voiceDetected &&
                millis() -
                captureStart >=
                waitForVoiceMs
            )
            {
                Serial.printf(
                    "[AsterVoice] No se detectó voz. "
                    "Máximo observado rms=%lu peak=%lu\n",
                    static_cast<unsigned long>(
                        maxObservedRms
                    ),
                    static_cast<unsigned long>(
                        maxObservedPeak
                    )
                );

                clear();

                return false;
            }

            continue;
        }

        if (activeVoice)
        {
            lastVoiceEndSample =
                _sampleCount;
        }

        const size_t silentSamples =
            _sampleCount -
            lastVoiceEndSample;

        const uint32_t silentMs =
            static_cast<uint32_t>(
                (
                    static_cast<uint64_t>(
                        silentSamples
                    ) *
                    1000ULL
                ) /
                AsterAudio.SAMPLE_RATE
            );

        if (silentMs >= finalSilenceMs)
        {
            silenceDetected = true;

            Serial.printf(
                "[AsterVoice] Silencio final detectado: %lu ms.\n",
                static_cast<unsigned long>(
                    silentMs
                )
            );

            break;
        }
    }

    if (!voiceDetected)
    {
        Serial.println(
            "[AsterVoice] ERROR: terminó sin detectar voz."
        );

        clear();

        return false;
    }

    if (!silenceDetected)
    {
        Serial.println(
            "[AsterVoice] Alcanzado máximo de seguridad."
        );
    }

    // -----------------------------------------------------
    // Recortar silencio inicial y final
    // -----------------------------------------------------

    const size_t preRollSamples =
        static_cast<size_t>(
            (
                static_cast<uint64_t>(
                    AsterAudio.SAMPLE_RATE
                ) *
                PRE_ROLL_MS
            ) /
            1000ULL
        );

    const size_t postRollSamples =
        static_cast<size_t>(
            (
                static_cast<uint64_t>(
                    AsterAudio.SAMPLE_RATE
                ) *
                POST_ROLL_MS
            ) /
            1000ULL
        );

    const size_t trimStart =
        voiceStartSample > preRollSamples
            ? voiceStartSample -
              preRollSamples
            : 0;

    size_t trimEnd =
        lastVoiceEndSample +
        postRollSamples;

    if (trimEnd > _sampleCount)
    {
        trimEnd = _sampleCount;
    }

    if (trimEnd <= trimStart)
    {
        Serial.println(
            "[AsterVoice] ERROR calculando recorte VAD."
        );

        clear();

        return false;
    }

    const size_t trimmedSamples =
        trimEnd -
        trimStart;

    if (trimStart > 0)
    {
        memmove(
            _buffer,
            _buffer + trimStart,
            trimmedSamples *
            sizeof(int16_t)
        );
    }

    _sampleCount =
        trimmedSamples;

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

    // -----------------------------------------------------
    // Estadísticas finales
    // -----------------------------------------------------

    _stats.minimum =
        std::numeric_limits<int16_t>::max();

    _stats.maximum =
        std::numeric_limits<int16_t>::min();

    _stats.peak = 0;

    uint64_t squareSum = 0;

    for (
        size_t i = 0;
        i < _sampleCount;
        ++i
    )
    {
        const int16_t sample =
            _buffer[i];

        if (sample < _stats.minimum)
        {
            _stats.minimum = sample;
        }

        if (sample > _stats.maximum)
        {
            _stats.maximum = sample;
        }

        const int32_t value =
            static_cast<int32_t>(
                sample
            );

        const uint32_t absolute =
            static_cast<uint32_t>(
                value < 0
                    ? -value
                    : value
            );

        if (absolute > _stats.peak)
        {
            _stats.peak = absolute;
        }

        squareSum +=
            static_cast<uint64_t>(
                value * value
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

    const uint32_t captureElapsed =
        millis() -
        captureStart;

    Serial.println(
        "[AsterVoice] Grabación automática terminada."
    );

    Serial.printf(
        "[AsterVoice] Duración PCM útil: %lu ms\n",
        static_cast<unsigned long>(
            _durationMs
        )
    );

    Serial.printf(
        "[AsterVoice] Tiempo hasta fin de habla: %lu ms\n",
        static_cast<unsigned long>(
            captureElapsed
        )
    );

    Serial.printf(
        "[AsterVoice] Bytes almacenados: %lu\n",
        static_cast<unsigned long>(
            byteCount()
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

    Serial.println(
        "[AsterVoice] Grabación VAD OK."
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
