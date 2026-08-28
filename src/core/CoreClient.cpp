#include "CoreClient.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "secrets.h"
#include "TlsRoots.h"


static constexpr uint32_t CORE_HTTP_TIMEOUT_MS = 60000;


// ---------------------------------------------------------
// Preparar conexión HTTPS verificada
// ---------------------------------------------------------

static bool beginSecureRequest(
    HTTPClient &http,
    WiFiClientSecure &tlsClient,
    const String &url
)
{
    if (!url.startsWith("https://"))
    {
        Serial.println(
            "[CoreClient] ERROR: ASTER_CORE_URL no usa HTTPS."
        );

        return false;
    }


    // Verificación TLS mediante CA de confianza.
    // No usamos setInsecure().

    tlsClient.setCACert(
        ASTER_TLS_ROOT_CA
    );


    // Tiempo máximo para completar handshake TLS.

    tlsClient.setHandshakeTimeout(
        20
    );


    http.setTimeout(
        CORE_HTTP_TIMEOUT_MS
    );


    if (
        !http.begin(
            tlsClient,
            url
        )
    )
    {
        Serial.println(
            "[CoreClient] ERROR inicializando HTTPS."
        );

        return false;
    }


    return true;
}


// ---------------------------------------------------------
// Authorization
// ---------------------------------------------------------

void CoreClientClass::addAuthenticationHeader(
    HTTPClient &http
)
{
    http.addHeader(
        "Authorization",
        String("Bearer ") +
        ASTER_POCKET_API_KEY
    );
}


// ---------------------------------------------------------
// Health
// ---------------------------------------------------------

bool CoreClientClass::checkHealth()
{
    WiFiClientSecure tlsClient;

    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/health";


    Serial.println();
    Serial.print(
        "[CoreClient] GET "
    );

    Serial.println(
        url
    );


    Serial.println(
        "[CoreClient] TLS: verificación activada."
    );


    if (
        !beginSecureRequest(
            http,
            tlsClient,
            url
        )
    )
    {
        return false;
    }


    const int status =
        http.GET();


    const String response =
        http.getString();


    http.end();


    Serial.print(
        "[CoreClient] Health HTTP: "
    );

    Serial.println(
        status
    );


    Serial.print(
        "[CoreClient] Health response: "
    );

    Serial.println(
        response
    );


    if (status != 200)
    {
        Serial.println(
            "[CoreClient] ERROR: Core remoto no disponible."
        );

        return false;
    }


    return true;
}


// ---------------------------------------------------------
// Crear conversación
// ---------------------------------------------------------

bool CoreClientClass::createConversation(
    String &conversationId
)
{
    conversationId = "";


    WiFiClientSecure tlsClient;

    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/conversations";


    Serial.println();
    Serial.print(
        "[CoreClient] POST "
    );

    Serial.println(
        url
    );


    if (
        !beginSecureRequest(
            http,
            tlsClient,
            url
        )
    )
    {
        return false;
    }


    http.addHeader(
        "Content-Type",
        "application/json"
    );


    addAuthenticationHeader(
        http
    );


    JsonDocument request;


    request["title"] =
        "A.S.T.E.R. Pocket remoto v0.6";


    String body;


    serializeJson(
        request,
        body
    );


    const int status =
        http.POST(
            body
        );


    const String response =
        http.getString();


    http.end();


    Serial.print(
        "[CoreClient] Crear conversación HTTP: "
    );

    Serial.println(
        status
    );


    if (status != 201)
    {
        Serial.print(
            "[CoreClient] ERROR response: "
        );

        Serial.println(
            response
        );

        return false;
    }


    JsonDocument document;


    const DeserializationError error =
        deserializeJson(
            document,
            response
        );


    if (error)
    {
        Serial.print(
            "[CoreClient] ERROR JSON conversación: "
        );

        Serial.println(
            error.c_str()
        );

        return false;
    }


    const char *id =
        document["id"];


    if (id == nullptr)
    {
        Serial.println(
            "[CoreClient] ERROR: Core no devolvió conversation ID."
        );

        return false;
    }


    conversationId =
        String(id);


    Serial.print(
        "[CoreClient] Conversation ID: "
    );

    Serial.println(
        conversationId
    );


    return true;
}


// ---------------------------------------------------------
// Enviar mensaje a Asty
// ---------------------------------------------------------

bool CoreClientClass::sendMessage(
    const String &conversationId,
    const String &message,
    String &answer,
    String &provider,
    String &model
)
{
    answer = "";
    provider = "";
    model = "";


    WiFiClientSecure tlsClient;

    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/conversations/" +
        conversationId +
        "/messages";


    Serial.println();
    Serial.print(
        "[CoreClient] POST "
    );

    Serial.println(
        url
    );


    if (
        !beginSecureRequest(
            http,
            tlsClient,
            url
        )
    )
    {
        return false;
    }


    http.addHeader(
        "Content-Type",
        "application/json"
    );


    addAuthenticationHeader(
        http
    );


    JsonDocument request;


    request["content"] =
        message;


    String body;


    serializeJson(
        request,
        body
    );


    Serial.println(
        "[CoreClient] Enviando mensaje remoto a Asty..."
    );


    const int status =
        http.POST(
            body
        );


    const String response =
        http.getString();


    http.end();


    Serial.print(
        "[CoreClient] Mensaje HTTP: "
    );

    Serial.println(
        status
    );


    if (status != 201)
    {
        Serial.print(
            "[CoreClient] ERROR response: "
        );

        Serial.println(
            response
        );

        return false;
    }


    JsonDocument document;


    const DeserializationError error =
        deserializeJson(
            document,
            response
        );


    if (error)
    {
        Serial.print(
            "[CoreClient] ERROR JSON respuesta: "
        );

        Serial.println(
            error.c_str()
        );

        return false;
    }


    const char *content =
        document
        ["assistant_message"]
        ["content"];


    const char *responseProvider =
        document
        ["assistant_message"]
        ["provider"];


    const char *responseModel =
        document
        ["assistant_message"]
        ["model"];


    if (content == nullptr)
    {
        Serial.println(
            "[CoreClient] ERROR: Asty no devolvió contenido."
        );

        return false;
    }


    answer =
        String(content);


    if (responseProvider != nullptr)
    {
        provider =
            String(responseProvider);
    }


    if (responseModel != nullptr)
    {
        model =
            String(responseModel);
    }


    Serial.println();
    Serial.println(
        "========== ASTY =========="
    );

    Serial.println(
        answer
    );

    Serial.println(
        "=========================="
    );


    Serial.print(
        "[CoreClient] Provider: "
    );

    Serial.println(
        provider
    );


    Serial.print(
        "[CoreClient] Model: "
    );

    Serial.println(
        model
    );


    return true;
}


// ---------------------------------------------------------
// Enviar audio PCM de Pocket a Core
// ---------------------------------------------------------

bool CoreClientClass::sendAudio(
    const String &conversationId,
    const uint8_t *audioData,
    size_t audioBytes,
    uint32_t sampleRate,
    CoreAudioTurnResult &result
)
{
    result.transcription = "";
    result.answer = "";

    result.assistantMessageId = "";
    result.provider = "";
    result.model = "";
    result.rawResponse = "";


    if (conversationId.length() == 0)
    {
        Serial.println(
            "[CoreClient] ERROR: conversación no disponible."
        );

        return false;
    }


    if (
        audioData == nullptr ||
        audioBytes == 0
    )
    {
        Serial.println(
            "[CoreClient] ERROR: audio vacío."
        );

        return false;
    }


    if (
        audioBytes %
        sizeof(int16_t) != 0
    )
    {
        Serial.println(
            "[CoreClient] ERROR: PCM no alineado a 16 bits."
        );

        return false;
    }


    if (sampleRate == 0)
    {
        Serial.println(
            "[CoreClient] ERROR: sample rate inválido."
        );

        return false;
    }


    WiFiClientSecure tlsClient;

    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/conversations/" +
        conversationId +
        "/audio";


    Serial.println();

    Serial.print(
        "[CoreClient] POST "
    );

    Serial.println(
        url
    );


    Serial.println(
        "[CoreClient] Preparando envío PCM + STT..."
    );


    Serial.printf(
        "[CoreClient] Audio: %lu bytes, %lu Hz, 16-bit mono\n",
        static_cast<unsigned long>(
            audioBytes
        ),
        static_cast<unsigned long>(
            sampleRate
        )
    );


    if (
        !beginSecureRequest(
            http,
            tlsClient,
            url
        )
    )
    {
        return false;
    }


    http.addHeader(
        "Content-Type",
        "audio/x-pcm"
    );


    http.addHeader(
        "X-Audio-Sample-Rate",
        String(sampleRate)
    );


    http.addHeader(
        "X-Audio-Bits-Per-Sample",
        "16"
    );


    http.addHeader(
        "X-Audio-Channels",
        "1"
    );


    http.addHeader(
        "X-Audio-Encoding",
        "pcm_s16le"
    );


    addAuthenticationHeader(
        http
    );


    Serial.println(
        "[CoreClient] Enviando voz a Core..."
    );


    const uint32_t startedAt =
        millis();


    const int status =
        http.sendRequest(
            "POST",
            const_cast<uint8_t *>(
                audioData
            ),
            audioBytes
        );


    const uint32_t elapsed =
        millis() -
        startedAt;


    const String response =
        http.getString();


    http.end();


    Serial.print(
        "[CoreClient] Audio/STT HTTP: "
    );

    Serial.println(
        status
    );


    Serial.printf(
        "[CoreClient] Tiempo total Core: %lu ms\n",
        static_cast<unsigned long>(
            elapsed
        )
    );


    if (status != 200)
    {
        Serial.print(
            "[CoreClient] ERROR response: "
        );

        Serial.println(
            response
        );

        return false;
    }


    JsonDocument document;


    const DeserializationError error =
        deserializeJson(
            document,
            response
        );


    if (error)
    {
        Serial.print(
            "[CoreClient] ERROR JSON audio/STT: "
        );

        Serial.println(
            error.c_str()
        );

        return false;
    }


    // -----------------------------------------------------
    // Validación del receipt PCM
    // -----------------------------------------------------

    const char *receiptStatus =
        document["status"];


    const uint32_t bytesReceived =
        document["bytes_received"] | 0U;


    if (
        receiptStatus == nullptr ||
        String(receiptStatus) != "accepted"
    )
    {
        Serial.println(
            "[CoreClient] ERROR: Core no aceptó el audio."
        );

        return false;
    }


    if (
        bytesReceived !=
        static_cast<uint32_t>(
            audioBytes
        )
    )
    {
        Serial.printf(
            "[CoreClient] ERROR: Core recibió %lu de %lu bytes.\n",
            static_cast<unsigned long>(
                bytesReceived
            ),
            static_cast<unsigned long>(
                audioBytes
            )
        );

        return false;
    }


    // -----------------------------------------------------
    // Pocket v0.11
    // Transcripción + turno real de Asty
    // -----------------------------------------------------

    const char *transcription =
        document["transcription"];


    const char *answer =
        document
        ["turn"]
        ["assistant_message"]
        ["content"];


    const char *assistantMessageId =

        document

        ["turn"]

        ["assistant_message"]

        ["id"];


    const char *provider =
        document
        ["turn"]
        ["assistant_message"]
        ["provider"];


    const char *model =
        document
        ["turn"]
        ["assistant_message"]
        ["model"];


    if (
        transcription == nullptr ||
        String(transcription).length() == 0
    )
    {
        Serial.println(
            "[CoreClient] ERROR: Core no devolvió transcription."
        );

        Serial.print(
            "[CoreClient] Response: "
        );

        Serial.println(
            response
        );

        return false;
    }


    if (
        answer == nullptr ||
        String(answer).length() == 0
    )
    {
        Serial.println(
            "[CoreClient] ERROR: Asty no devolvió respuesta."
        );

        Serial.print(
            "[CoreClient] Response: "
        );

        Serial.println(
            response
        );

        return false;
    }


    result.transcription =
        String(transcription);


    result.answer =
        String(answer);


    if (assistantMessageId != nullptr)

    {

        result.assistantMessageId =

            String(assistantMessageId);

    }


    if (provider != nullptr)
    {
        result.provider =
            String(provider);
    }


    if (model != nullptr)
    {
        result.model =
            String(model);
    }


    result.rawResponse =
        response;


    Serial.println();

    Serial.println(
        "========== VOZ / STT =========="
    );


    Serial.print(
        "[Pocket] TÚ: "
    );

    Serial.println(
        result.transcription
    );


    Serial.println();

    Serial.print(
        "[Pocket] ASTY: "
    );

    Serial.println(
        result.answer
    );


    Serial.println();

    Serial.print(
        "[Pocket] Provider: "
    );

    Serial.println(
        result.provider
    );


    Serial.print(
        "[Pocket] Model: "
    );

    Serial.println(
        result.model
    );


    Serial.println(
        "================================"
    );


    return true;
}




// ---------------------------------------------------------
// Adaptador de stream HTTPS -> PCM 16-bit little-endian
// ---------------------------------------------------------

namespace
{

class CoreSpeechPcmSink final : public Stream

{

public:

    CoreSpeechPcmSink(

        CoreSpeechPcmCallback callback,

        void *context

    )

        : _callback(callback),

          _context(context)

    {

    }


    size_t write(

        uint8_t value

    ) override

    {

        return write(

            &value,

            1

        );

    }


    size_t write(

        const uint8_t *buffer,

        size_t size

    ) override

    {

        if (

            _failed ||

            buffer == nullptr ||

            size == 0

        )

        {

            return 0;

        }


        size_t offset = 0;


        // Una muestra int16 puede quedar dividida entre
        // dos bloques recibidos por HTTPS.

        if (_hasPendingByte)

        {

            const uint16_t raw =

                static_cast<uint16_t>(

                    _pendingByte

                ) |

                (

                    static_cast<uint16_t>(

                        buffer[0]

                    ) << 8

                );


            const int16_t sample =

                static_cast<int16_t>(

                    raw

                );


            if (

                !_callback(

                    &sample,

                    1,

                    _context

                )

            )

            {

                _failed = true;

                return 0;

            }


            _hasPendingByte = false;

            offset = 1;

        }


        constexpr size_t CHUNK_SAMPLES = 256;

        int16_t samples[CHUNK_SAMPLES];


        while (

            offset + 1 < size

        )

        {

            const size_t availableSamples =

                (

                    size - offset

                ) /

                2;


            const size_t sampleCount =

                availableSamples < CHUNK_SAMPLES

                    ? availableSamples

                    : CHUNK_SAMPLES;


            for (

                size_t i = 0;

                i < sampleCount;

                ++i

            )

            {

                const size_t byteOffset =

                    offset +

                    i * 2;


                const uint16_t raw =

                    static_cast<uint16_t>(

                        buffer[byteOffset]

                    ) |

                    (

                        static_cast<uint16_t>(

                            buffer[byteOffset + 1]

                        ) << 8

                    );


                samples[i] =

                    static_cast<int16_t>(

                        raw

                    );

            }


            if (

                !_callback(

                    samples,

                    sampleCount,

                    _context

                )

            )

            {

                _failed = true;

                return offset;

            }


            offset +=

                sampleCount *

                2;

        }


        if (offset < size)

        {

            _pendingByte = buffer[offset];

            _hasPendingByte = true;

            ++offset;

        }


        _bytesReceived += offset;

        return offset;

    }


    int available() override

    {

        return 0;

    }


    int read() override

    {

        return -1;

    }


    int peek() override

    {

        return -1;

    }


    void flush() override

    {

    }


    bool finish() const

    {

        return

            !_failed &&

            !_hasPendingByte &&

            _bytesReceived > 0;

    }


    size_t bytesReceived() const

    {

        return _bytesReceived;

    }


private:

    CoreSpeechPcmCallback _callback = nullptr;

    void *_context = nullptr;

    bool _failed = false;

    bool _hasPendingByte = false;

    uint8_t _pendingByte = 0;

    size_t _bytesReceived = 0;

};

}


// ---------------------------------------------------------
// Descargar TTS PCM de Asty
// ---------------------------------------------------------

bool CoreClientClass::streamSpeech(

    const String &conversationId,

    const String &messageId,

    CoreSpeechPcmCallback callback,

    void *context

)

{

    if (

        conversationId.length() == 0 ||

        messageId.length() == 0 ||

        callback == nullptr

    )

    {

        Serial.println(

            "[CoreClient] ERROR: parámetros TTS inválidos."

        );

        return false;

    }


    WiFiClientSecure tlsClient;

    HTTPClient http;


    const String url =

        String(ASTER_CORE_URL) +

        "/conversations/" +

        conversationId +

        "/messages/" +

        messageId +

        "/speech";


    Serial.println();

    Serial.print(

        "[CoreClient] GET "

    );

    Serial.println(

        url

    );


    Serial.println(

        "[CoreClient] Solicitando TTS PCM de Asty..."

    );


    if (

        !beginSecureRequest(

            http,

            tlsClient,

            url

        )

    )

    {

        return false;

    }


    http.addHeader(

        "Accept",

        "audio/x-pcm"

    );


    addAuthenticationHeader(

        http

    );


    const uint32_t startedAt = millis();


    const int status =

        http.GET();


    Serial.print(

        "[CoreClient] Speech HTTP: "

    );

    Serial.println(

        status

    );


    if (status != 200)

    {

        const String response =

            http.getString();


        Serial.print(

            "[CoreClient] ERROR speech: "

        );

        Serial.println(

            response

        );


        http.end();

        return false;

    }


    CoreSpeechPcmSink sink(

        callback,

        context

    );


    const int streamedBytes =

        http.writeToStream(

            &sink

        );


    const uint32_t elapsed =

        millis() -

        startedAt;


    const bool validPcm =

        streamedBytes >= 0 &&

        sink.finish();


    Serial.printf(

        "[CoreClient] Speech recibido: %lu bytes en %lu ms\n",

        static_cast<unsigned long>(

            sink.bytesReceived()

        ),

        static_cast<unsigned long>(

            elapsed

        )

    );


    http.end();


    if (!validPcm)

    {

        Serial.println(

            "[CoreClient] ERROR: stream PCM TTS incompleto."

        );

        return false;

    }


    Serial.println(

        "[CoreClient] TTS PCM completado."

    );


    return true;

}


CoreClientClass CoreClient;
