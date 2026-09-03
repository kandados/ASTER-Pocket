#include <mbedtls/base64.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include <WiFi.h>
#include "services/ota/AsterOTA.h"
#include <time.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#include "drivers/display/AsterDisplay.h"
#include "drivers/touch/AsterTouch.h"
#include "drivers/audio/AsterAudio.h"
#include "drivers/audio/AsterVoice.h"
#include "core/CoreClient.h"
#include "secrets.h"


static constexpr uint32_t WIFI_TIMEOUT_MS =
    20000;

static constexpr uint32_t TIME_TIMEOUT_MS =
    20000;

static constexpr time_t MINIMUM_VALID_TIME =
    1700000000;


static String conversationId;

static uint32_t lastAudioMeterMs = 0;


// ---------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------

static bool connectWiFi()
{
    Serial.println();
    Serial.println(
        "[Pocket] Conectando a Wi-Fi..."
    );


    Serial.print(
        "[Pocket] SSID: "
    );

    Serial.println(
        ASTER_WIFI_SSID
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Conectando Wi-Fi..."
    );


    WiFi.mode(
        WIFI_STA
    );


    WiFi.setSleep(
        false
    );


    WiFi.begin(
        ASTER_WIFI_SSID,
        ASTER_WIFI_PASSWORD
    );


    const uint32_t start =
        millis();


    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < WIFI_TIMEOUT_MS
    )
    {
        AsterDisplay.update();

        delay(
            50
        );
    }


    if (
        WiFi.status() != WL_CONNECTED
    )
    {
        Serial.println(
            "[Pocket] ERROR conectando a Wi-Fi."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error de Wi-Fi"
        );


        return false;
    }


    Serial.println(
        "[Pocket] Wi-Fi conectado."
    );


    Serial.print(
        "[Pocket] IP: "
    );

    Serial.println(
        WiFi.localIP()
    );


    Serial.print(
        "[Pocket] RSSI: "
    );

    Serial.print(
        WiFi.RSSI()
    );

    Serial.println(
        " dBm"
    );


    return true;
}


// ---------------------------------------------------------
// NTP para TLS
// ---------------------------------------------------------

static bool synchronizeClock()
{
    Serial.println();
    Serial.println(
        "[Pocket] Sincronizando hora..."
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Sincronizando hora..."
    );


    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.cloudflare.com",
        "time.google.com"
    );


    const uint32_t start =
        millis();


    while (
        time(nullptr) < MINIMUM_VALID_TIME &&
        millis() - start < TIME_TIMEOUT_MS
    )
    {
        AsterDisplay.update();

        delay(
            100
        );
    }


    if (
        time(nullptr) < MINIMUM_VALID_TIME
    )
    {
        Serial.println(
            "[Pocket] ERROR sincronizando hora."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error de hora"
        );


        return false;
    }


    Serial.println(
        "[Pocket] Hora válida para TLS."
    );


    return true;
}


// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------


// =========================================================
// TTS anticipado durante el streaming de respuesta
// =========================================================

struct VoiceEarlyTtsState
{
    String conversationId;
    String pendingText;
    String firstPhrase;

    size_t spokenCharacters = 0;

    volatile bool launched = false;
    volatile bool running = false;
    volatile bool finished = false;
    volatile bool succeeded = false;
};


static size_t findFirstSpeakableSentenceEnd(
    const String &text
)
{
    if (text.length() < 12)
    {
        return 0;
    }

    for (size_t i = 0; i < text.length(); ++i)
    {
        const char c = text[i];

        if (
            c != "."[0] &&
            c != "!"[0] &&
            c != "?"[0]
        )
        {
            continue;
        }

        if (
            i + 1 >= text.length() ||
            isspace(
                static_cast<unsigned char>(
                    text[i + 1]
                )
            )
        )
        {
            return i + 1;
        }
    }

    return 0;
}


static bool playSpeechPcm(
    const int16_t *samples,
    size_t sampleCount,
    void *context
)
{
    bool *speakerStarted =
        static_cast<bool *>(context);

    if (speakerStarted == nullptr)
    {
        return false;
    }

    if (!*speakerStarted)
    {
        if (!AsterAudio.startSpeakerPlayback())
        {
            return false;
        }

        *speakerStarted = true;
    }

    return AsterAudio.playMonoPcm(
        samples,
        sampleCount,
        2000
    );
}


static void earlyVoiceTtsTask(
    void *parameter
)
{
    VoiceEarlyTtsState *state =
        static_cast<VoiceEarlyTtsState *>(
            parameter
        );

    if (state == nullptr)
    {
        vTaskDelete(nullptr);
        return;
    }

    Serial.println(
        "[Pocket] TTS anticipado iniciado."
    );

    bool speakerStarted = false;

    const bool ok =
        CoreClient.streamSpeechText(
            state->conversationId,
            state->firstPhrase,
            playSpeechPcm,
            &speakerStarted
        );

    if (speakerStarted)
    {
        AsterAudio.stopSpeakerPlayback();
    }

    state->succeeded = ok;
    state->running = false;
    state->finished = true;

    Serial.print(
        "[Pocket] TTS anticipado terminado: "
    );

    Serial.println(
        ok ? "OK" : "ERROR"
    );

    vTaskDelete(nullptr);
}


static bool launchEarlyVoiceTts(
    VoiceEarlyTtsState &state
)
{
    if (
        state.launched ||
        state.firstPhrase.length() == 0
    )
    {
        return false;
    }

    state.launched = true;
    state.running = true;
    state.finished = false;
    state.succeeded = false;

    const BaseType_t created =
        xTaskCreate(
            earlyVoiceTtsTask,
            "asty-tts-early",
            12288,
            &state,
            1,
            nullptr
        );

    if (created != pdPASS)
    {
        state.launched = false;
        state.running = false;

        Serial.println(
            "[Pocket] ERROR creando tarea TTS anticipada."
        );

        return false;
    }

    Serial.println(
        "[Pocket] TTS anticipado lanzado antes de done."
    );

    return true;
}



// =========================================================
// Voz multiplexada: texto + PCM en /audio/stream
// =========================================================

struct VoiceStreamPlaybackState
{
    StreamBufferHandle_t pcmBuffer = nullptr;
    TaskHandle_t playbackTask = nullptr;

    volatile bool sawAudio = false;
    volatile bool audioFailed = false;
    volatile bool audioEnded = false;
    volatile bool playbackFinished = false;
    volatile bool textFinished = false;

    // El LLM puede generar el texto por delante de la voz.
    // Lo conservamos aquí y solo lo mostramos según avanza PCM.
    String bufferedText;
    size_t displayedTextBytes = 0;
    float textRevealCredit = 0.0f;
    size_t lastDecodedBytes = 0;
    bool textRevealStarted = false;
};


static constexpr size_t VOICE_PCM_BUFFER_BYTES =
    32768;

static constexpr size_t VOICE_PCM_DECODE_BYTES =
    6144;


static void voicePlaybackTask(
    void *parameter
)
{
    VoiceStreamPlaybackState *state =
        static_cast<
            VoiceStreamPlaybackState *
        >(
            parameter
        );

    if (
        state == nullptr ||
        state->pcmBuffer == nullptr
    )
    {
        vTaskDelete(
            nullptr
        );

        return;
    }

    alignas(int16_t)
    uint8_t pcmBytes[4096];

    bool speakerStarted =
        false;

    while (true)
    {
        const size_t received =
            xStreamBufferReceive(
                state->pcmBuffer,
                pcmBytes,
                sizeof(pcmBytes),
                pdMS_TO_TICKS(20)
            );

        if (received > 0)
        {
            if (
                received %
                sizeof(int16_t) != 0
            )
            {
                Serial.println(
                    "[Pocket] ERROR: bloque PCM desalineado."
                );

                state->audioFailed = true;
                break;
            }

            if (!speakerStarted)
            {
                if (
                    !AsterAudio.startSpeakerPlayback()
                )
                {
                    Serial.println(
                        "[Pocket] ERROR iniciando altavoz "
                        "desde tarea PCM."
                    );

                    state->audioFailed = true;
                    break;
                }

                speakerStarted = true;

                Serial.println(
                    "[Pocket] Reproducción PCM independiente iniciada."
                );
            }

            if (
                !AsterAudio.playMonoPcm(
                    reinterpret_cast<
                        const int16_t *
                    >(
                        pcmBytes
                    ),
                    received /
                    sizeof(int16_t),
                    1000
                )
            )
            {
                Serial.println(
                    "[Pocket] ERROR reproduciendo PCM "
                    "desde tarea dedicada."
                );

                state->audioFailed = true;
                break;
            }
        }

        if (
            state->audioEnded &&
            xStreamBufferBytesAvailable(
                state->pcmBuffer
            ) == 0
        )
        {
            break;
        }
    }

    if (speakerStarted)
    {
        AsterAudio.stopSpeakerPlayback();
    }

    state->playbackFinished =
        true;

    state->playbackTask =
        nullptr;

    Serial.println(
        "[Pocket] Tarea PCM finalizada."
    );

    vTaskDelete(
        nullptr
    );
}


static bool beginVoicePlayback(
    VoiceStreamPlaybackState &state
)
{
    state.pcmBuffer =
        xStreamBufferCreate(
            VOICE_PCM_BUFFER_BYTES,
            1
        );

    if (state.pcmBuffer == nullptr)
    {
        Serial.println(
            "[Pocket] ERROR creando buffer PCM."
        );

        state.audioFailed = true;
        return false;
    }

    const BaseType_t created =
        xTaskCreate(
            voicePlaybackTask,
            "asty-pcm",
            8192,
            &state,
            3,
            &state.playbackTask
        );

    if (created != pdPASS)
    {
        Serial.println(
            "[Pocket] ERROR creando tarea PCM."
        );

        vStreamBufferDelete(
            state.pcmBuffer
        );

        state.pcmBuffer =
            nullptr;

        state.audioFailed = true;

        return false;
    }

    Serial.println(
        "[Pocket] Buffer y tarea PCM preparados."
    );

    return true;
}


static bool queueMultiplexedPcm(
    const String &encoded,
    VoiceStreamPlaybackState &state
)
{
    if (
        state.pcmBuffer == nullptr ||
        state.audioFailed
    )
    {
        return false;
    }

    alignas(int16_t)
    static uint8_t decoded[
        VOICE_PCM_DECODE_BYTES
    ];

    size_t decodedBytes =
        0;

    const int decodeResult =
        mbedtls_base64_decode(
            decoded,
            sizeof(decoded),
            &decodedBytes,
            reinterpret_cast<
                const unsigned char *
            >(
                encoded.c_str()
            ),
            encoded.length()
        );

    if (
        decodeResult != 0 ||
        decodedBytes == 0 ||
        decodedBytes %
        sizeof(int16_t) != 0
    )
    {
        Serial.printf(
            "[Pocket] ERROR PCM Base64: %d\n",
            decodeResult
        );

        state.audioFailed = true;
        return false;
    }

    state.lastDecodedBytes =
        decodedBytes;

    size_t offset =
        0;

    const uint32_t startedAt =
        millis();

    while (offset < decodedBytes)
    {
        const size_t sent =
            xStreamBufferSend(
                state.pcmBuffer,
                decoded + offset,
                decodedBytes - offset,
                pdMS_TO_TICKS(20)
            );

        if (sent > 0)
        {
            offset += sent;
        }

        // La UI sigue viva, pero ya no alimenta directamente
        // al I2S. La tarea de audio tiene prioridad propia.
        AsterDisplay.update();

        if (state.audioFailed)
        {
            return false;
        }

        if (
            millis() -
            startedAt > 5000
        )
        {
            Serial.println(
                "[Pocket] ERROR: timeout llenando buffer PCM."
            );

            state.audioFailed = true;
            return false;
        }
    }

    if (!state.sawAudio)
    {
        state.sawAudio = true;

        Serial.println(
            "[Pocket] Primer PCM depositado en buffer."
        );
    }

    return true;
}



// =========================================================
// Texto acompasado con la reproducción de voz
// =========================================================

// Aproximación inicial para castellano hablado.
// El flujo PCM real actúa como reloj, no el LLM.
static constexpr float ASTY_TEXT_CHARS_PER_SECOND =
    15.0f;


static bool isTextSpacing(
    char value
)
{
    return (
        value == ' ' ||
        value == '\n' ||
        value == '\t'
    );
}


static size_t nextTextUnitEnd(
    const String &text,
    size_t start
)
{
    const size_t length =
        text.length();

    if (start >= length)
    {
        return length;
    }

    size_t end =
        start;

    // Llegar hasta el final de la palabra.
    while (
        end < length &&
        !isTextSpacing(
            text[end]
        )
    )
    {
        ++end;
    }

    // Incluir los espacios posteriores para conservar formato.
    while (
        end < length &&
        isTextSpacing(
            text[end]
        )
    )
    {
        ++end;
    }

    return end;
}


static void revealTextForPcm(
    VoiceStreamPlaybackState &state,
    size_t pcmBytes
)
{
    if (
        pcmBytes == 0 ||
        state.bufferedText.length() == 0
    )
    {
        return;
    }

    constexpr float PCM_BYTES_PER_SECOND =
        static_cast<float>(
            AsterAudioClass::SAMPLE_RATE *
            sizeof(int16_t)
        );

    state.textRevealCredit +=
        (
            static_cast<float>(
                pcmBytes
            ) /
            PCM_BYTES_PER_SECOND
        ) *
        ASTY_TEXT_CHARS_PER_SECOND;

    // La primera palabra aparece prácticamente al comenzar
    // a sonar Asty.
    const bool firstReveal =
        !state.textRevealStarted;

    if (
        firstReveal &&
        state.textRevealCredit >= 1.0f
    )
    {
        const size_t end =
            nextTextUnitEnd(
                state.bufferedText,
                state.displayedTextBytes
            );

        if (
            end >
            state.displayedTextBytes
        )
        {
            const String piece =
                state.bufferedText.substring(
                    state.displayedTextBytes,
                    end
                );

            AsterDisplay.appendReplyStream(
                piece.c_str()
            );

            state.displayedTextBytes =
                end;

            state.textRevealCredit =
                0.0f;

            state.textRevealStarted =
                true;

            Serial.println(
                "[Pocket] Texto sincronizado iniciado con PCM."
            );
        }
    }

    // Después avanzamos palabra a palabra según la duración
    // de audio realmente recibida.
    while (
        state.displayedTextBytes <
        state.bufferedText.length()
    )
    {
        const size_t end =
            nextTextUnitEnd(
                state.bufferedText,
                state.displayedTextBytes
            );

        if (
            end <=
            state.displayedTextBytes
        )
        {
            break;
        }

        const size_t unitBytes =
            end -
            state.displayedTextBytes;

        if (
            state.textRevealCredit <
            static_cast<float>(
                unitBytes
            )
        )
        {
            break;
        }

        const String piece =
            state.bufferedText.substring(
                state.displayedTextBytes,
                end
            );

        AsterDisplay.appendReplyStream(
            piece.c_str()
        );

        state.displayedTextBytes =
            end;

        state.textRevealCredit -=
            static_cast<float>(
                unitBytes
            );
    }
}


static void finishSynchronizedText(
    VoiceStreamPlaybackState &state
)
{
    if (
        state.displayedTextBytes <
        state.bufferedText.length()
    )
    {
        const String remaining =
            state.bufferedText.substring(
                state.displayedTextBytes
            );

        AsterDisplay.appendReplyStream(
            remaining.c_str()
        );

        state.displayedTextBytes =
            state.bufferedText.length();
    }

    AsterDisplay.endReplyStream();
}


static void finishVoicePlaybackInput(
    VoiceStreamPlaybackState &state
)
{
    state.audioEnded =
        true;
}


static void waitVoicePlayback(
    VoiceStreamPlaybackState &state
)
{
    if (state.pcmBuffer == nullptr)
    {
        return;
    }

    while (!state.playbackFinished)
    {
        AsterDisplay.update();

        delay(
            5
        );
    }

    vStreamBufferDelete(
        state.pcmBuffer
    );

    state.pcmBuffer =
        nullptr;
}


void setup()
{

    Serial.begin(
        115200
    );


    delay(
        1000
    );


    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "A.S.T.E.R. Pocket"
    );

    Serial.println(
        "Touch Chat v0.8"
    );

    Serial.println(
        "================================"
    );


    // -----------------------------------------------------
    // AMOLED + LVGL
    // -----------------------------------------------------

    AsterDisplay.begin();


    AsterDisplay.showStatus(
        "ASTY",
        "Iniciando..."
    );


    // -----------------------------------------------------
    // Táctil
    // -----------------------------------------------------

    if (!AsterTouch.begin())
    {
        Serial.println(
            "[Pocket] ERROR inicializando táctil."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error táctil"
        );


        return;
    }


    // -----------------------------------------------------
    // Micrófonos / ES7210
    // -----------------------------------------------------

    Serial.println();
    Serial.println(
        "[Pocket] Inicializando micrófonos..."
    );

    if (!AsterAudio.beginMicrophone())
    {
        Serial.println(
            "[Pocket] ERROR inicializando micrófonos."
        );
    }
    else
    {
        Serial.println(
            "[Pocket] Micrófonos preparados."
        );
    }


    // -----------------------------------------------------
    // Altavoz / ES8311 - validación física v0.12
    // -----------------------------------------------------

    Serial.println();
    Serial.println(
        "[Pocket] Inicializando altavoz..."
    );

    if (!AsterAudio.beginSpeaker())
    {
        Serial.println(
            "[Pocket] ERROR inicializando altavoz."
        );
    }
    else
    {
        Serial.println(
            "[Pocket] Altavoz preparado."
        );

        AsterDisplay.setVolumeLevel(
            AsterAudio.speakerVolume()
        );

        Serial.printf(
            "[Pocket] Volumen inicial: %u%%\n",
            static_cast<unsigned>(
                AsterAudio.speakerVolume()
            )
        );
    }


    // -----------------------------------------------------
    // Wi-Fi
    // -----------------------------------------------------

    if (!connectWiFi())
    {
        return;
    }


    // -----------------------------------------------------
    // OTA local
    // -----------------------------------------------------

    if (!AsterOTA.begin())
    {
        Serial.println(
            "[Pocket] AVISO: OTA local no disponible."
        );
    }


    // -----------------------------------------------------
    // Hora TLS
    // -----------------------------------------------------

    if (!synchronizeClock())
    {
        return;
    }


    // -----------------------------------------------------
    // Core remoto
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Conectando con Core..."
    );


    // No hacemos un /health previo: crear la conversación
    // ya valida directamente la disponibilidad de Core.
    Serial.println(
        "[Pocket] Conectando directamente con Core..."
    );


    // -----------------------------------------------------
    // Conversación persistente
    // -----------------------------------------------------

    if (
        !CoreClient.createConversation(
            conversationId
        )
    )
    {
        Serial.println(
            "[Pocket] ERROR creando conversación."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error creando conversación"
        );


        return;
    }


    Serial.print(
        "[Pocket] Conversación activa: "
    );

    Serial.println(
        conversationId
    );


    // -----------------------------------------------------
    // Interfaz de chat
    // -----------------------------------------------------

    AsterDisplay.showChatInput();


    Serial.println();
    Serial.println(
        "[Pocket] Escribe una pregunta desde la pantalla."
    );
}


// ---------------------------------------------------------
// Loop
// ---------------------------------------------------------

void loop()
{
    AsterOTA.handle();

    AsterDisplay.update();

    // -----------------------------------------------------
    // Volumen táctil
    // -----------------------------------------------------

    uint8_t requestedVolume = 0;

    if (
        AsterDisplay.consumeVolumeChange(
            requestedVolume
        )
    )
    {
        if (
            AsterAudio.setSpeakerVolume(
                requestedVolume
            )
        )
        {
            AsterDisplay.setVolumeLevel(
                AsterAudio.speakerVolume()
            );

            Serial.printf(
                "[Pocket] Volumen aplicado: %u%%\n",
                static_cast<unsigned>(
                    AsterAudio.speakerVolume()
                )
            );
        }
        else
        {
            Serial.println(
                "[Pocket] ERROR aplicando volumen."
            );
        }
    }



    // -----------------------------------------------------
    // Pocket v0.9.3 - prueba de grabación de voz
    //
    // En el monitor serie:
    //   r + Enter
    //
    // espera 2 segundos y graba 5 segundos en PSRAM.
    // -----------------------------------------------------

    if (Serial.available() > 0)
    {
        const char command =
            static_cast<char>(
                Serial.read()
            );

        while (Serial.available() > 0)
        {
            Serial.read();
        }

        if (
            command == 'r' ||
            command == 'R'
        )
        {
            Serial.println();
            Serial.println(
                "[AsterVoice] Grabación solicitada."
            );

            Serial.println(
                "[AsterVoice] Preparando detección automática..."
            );

            const bool recorded =
                AsterVoice.recordUntilSilence(
                    10000,
                    1200,
                    3000
                );


            if (!recorded)
            {
                Serial.println(
                    "[Pocket] ERROR grabando audio."
                );

                AsterDisplay.showStatus(
                    "ASTY",
                    "Error grabando voz"
                );
            }
            else
            {
                Serial.println();
                Serial.println(
                    "[Pocket] Enviando grabación a Core..."
                );


                AsterDisplay.showStatus(
                    "ASTY",
                    "Enviando voz..."
                );


                CoreAudioTurnResult voiceResult;
                VoiceStreamPlaybackState voicePlayback;

                const bool playbackReady =
                    beginVoicePlayback(
                        voicePlayback
                    );

                const bool sent =
                    CoreClient.sendAudioStream(
                        conversationId,
                        reinterpret_cast<
                            const uint8_t *
                        >(
                            AsterVoice.data()
                        ),
                        AsterVoice.byteCount(),
                        AsterAudio.SAMPLE_RATE,
                        voiceResult,
                        [](
                            CoreAudioStreamEventType eventType,
                            const String &content,
                            void *context
                        ) -> bool
                        {
                            VoiceStreamPlaybackState *playback =
                                static_cast<
                                    VoiceStreamPlaybackState *
                                >(
                                    context
                                );

                            if (playback == nullptr)
                            {
                                return false;
                            }

                            switch (eventType)
                            {
                                case CoreAudioStreamEventType::Transcription:
                                    Serial.print(
                                        "[Pocket] STT: "
                                    );

                                    Serial.println(
                                        content
                                    );
                                    break;

                                case CoreAudioStreamEventType::Start:
                                    Serial.println(
                                        "[Pocket] Asty empieza a responder..."
                                    );

                                    AsterDisplay.beginReplyStream();
                                    break;
                                case CoreAudioStreamEventType::Delta:
                                    playback->bufferedText +=
                                        content;
                                    break;
                                case CoreAudioStreamEventType::TextDone:
                                    playback->textFinished =
                                        true;

                                    Serial.println(
                                        "[Pocket] Texto generado; "
                                        "presentación acompasada continúa."
                                    );
                                    break;


                                case CoreAudioStreamEventType::AudioStart:
                                    Serial.println(
                                        "[Pocket] Core inicia voz multiplexada."
                                    );
                                    break;
                                case CoreAudioStreamEventType::AudioPcm:
                                    if (!playback->audioFailed)
                                    {
                                        if (
                                            queueMultiplexedPcm(
                                                content,
                                                *playback
                                            )
                                        )
                                        {
                                            revealTextForPcm(
                                                *playback,
                                                playback->lastDecodedBytes
                                            );
                                        }
                                    }
                                    break;

                                case CoreAudioStreamEventType::AudioEnd:
                                    finishVoicePlaybackInput(
                                        *playback
                                    );

                                    Serial.println(
                                        "[Pocket] Fin PCM recibido desde Core."
                                    );
                                    break;
                                case CoreAudioStreamEventType::AudioError:
                                    playback->audioFailed = true;

                                    finishVoicePlaybackInput(
                                        *playback
                                    );

                                    Serial.print(
                                        "[Pocket] ERROR voz multiplexada: "
                                    );

                                    Serial.println(
                                        content
                                    );
                                    break;
                                case CoreAudioStreamEventType::Done:
                                    Serial.println(
                                        "[Pocket] Stream de Core completado."
                                    );
                                    break;



                                case CoreAudioStreamEventType::Error:
                                    Serial.print(
                                        "[Pocket] ERROR streaming Asty: "
                                    );

                                    Serial.println(
                                        content
                                    );
                                    break;
                            }

                            return true;
                        },
                        &voicePlayback
                    );



                finishVoicePlaybackInput(
                    voicePlayback
                );

                if (playbackReady)
                {
                    waitVoicePlayback(
                        voicePlayback
                    );
                }

                finishSynchronizedText(
                    voicePlayback
                );

                if (!sent)
                {


                    Serial.println(
                        "[Pocket] ERROR procesando voz con Core."
                    );

                    AsterDisplay.showStatus(
                        "ASTY",
                        "Error procesando voz"
                    );
                }
                else
                {
                    Serial.println();
                    Serial.println(
                        "================================"
                    );

                    Serial.println(
                        "[Pocket] VOZ PROCESADA POR ASTY"
                    );

                    Serial.print(
                        "[Pocket] TÚ: "
                    );

                    Serial.println(
                        voiceResult.transcription
                    );

                    Serial.println();

                    Serial.print(
                        "[Pocket] ASTY: "
                    );

                    Serial.println(
                        voiceResult.answer
                    );

                    Serial.println(
                        "================================"
                    );

                    if (voicePlayback.audioFailed)
                    {
                        Serial.println(
                            "[Pocket] AVISO: hubo un error "
                            "en la voz multiplexada."
                        );
                    }
                    else if (voicePlayback.sawAudio)
                    {
                        Serial.println(
                            "[Pocket] Texto y voz recibidos "
                            "por una sola conexión HTTPS."
                        );
                    }
                    else
                    {
                        Serial.println(
                            "[Pocket] AVISO: Core no envió "
                            "audio multiplexado."
                        );
                    }

                }
            }


            Serial.println(
                "[AsterVoice] Fin de prueba v0.10b."
            );

            Serial.println(
                "[AsterVoice] Pulsa r para repetir."
            );
        }
        else if (
            command == 'd' ||
            command == 'D'
        )
        {
            AsterVoice.dumpBase64();
        }
    }


    String message;


    if (
        AsterDisplay.consumeSendRequest(
            message
        )
    )
    {
        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.print(
            "[Pocket] Usuario: "
        );

        Serial.println(
            message
        );

        Serial.println(
            "================================"
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Pensando..."
        );


        String answer;

        String provider;

        String model;


        const bool success =
            CoreClient.sendMessage(
                conversationId,
                message,
                answer,
                provider,
                model
            );


        if (!success)
        {
            Serial.println(
                "[Pocket] ERROR enviando mensaje."
            );


            AsterDisplay.showStatus(
                "ASTY",
                "Error hablando con Core"
            );


            delay(
                1500
            );


            AsterDisplay.showChatInput();


            return;
        }


        Serial.println();
        Serial.println(
            "================================"
        );

        Serial.print(
            "[Pocket] Asty: "
        );

        Serial.println(
            answer
        );


        Serial.print(
            "[Pocket] Ruta: "
        );

        Serial.print(
            provider
        );

        Serial.print(
            " / "
        );

        Serial.println(
            model
        );

        Serial.println(
            "================================"
        );


        AsterDisplay.showReply(
            answer.c_str()
        );
    }


    delay(
        5
    );
}
