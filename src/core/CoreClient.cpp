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


CoreClientClass CoreClient;
