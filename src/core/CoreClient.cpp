#include "CoreClient.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "secrets.h"


static constexpr uint32_t CORE_HTTP_TIMEOUT_MS =
    60000;


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
    HTTPClient http;

    const String url =
        String(ASTER_CORE_URL) +
        "/health";


    Serial.print(
        "[CoreClient] GET "
    );

    Serial.println(
        url
    );


    if (!http.begin(url))
    {
        Serial.println(
            "[CoreClient] ERROR inicializando HTTP."
        );

        return false;
    }


    http.setTimeout(
        10000
    );


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
        "[CoreClient] Health: "
    );

    Serial.println(
        response
    );


    return status == 200;
}


// ---------------------------------------------------------
// Crear conversación
// ---------------------------------------------------------

bool CoreClientClass::createConversation(
    String &conversationId
)
{
    conversationId = "";


    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/conversations";


    Serial.print(
        "[CoreClient] POST "
    );

    Serial.println(
        url
    );


    if (!http.begin(url))
    {
        Serial.println(
            "[CoreClient] ERROR inicializando conversación."
        );

        return false;
    }


    http.setTimeout(
        CORE_HTTP_TIMEOUT_MS
    );


    http.addHeader(
        "Content-Type",
        "application/json"
    );


    addAuthenticationHeader(
        http
    );


    JsonDocument request;


    request["title"] =
        "A.S.T.E.R. Pocket - primera conversación real";


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


    Serial.print(
        "[CoreClient] Respuesta: "
    );

    Serial.println(
        response
    );


    if (status != 201)
    {
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
            "[CoreClient] ERROR: Core no devolvió conversation id."
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


    HTTPClient http;


    const String url =
        String(ASTER_CORE_URL) +
        "/conversations/" +
        conversationId +
        "/messages";


    Serial.print(
        "[CoreClient] POST "
    );

    Serial.println(
        url
    );


    if (!http.begin(url))
    {
        Serial.println(
            "[CoreClient] ERROR inicializando mensaje."
        );

        return false;
    }


    http.setTimeout(
        CORE_HTTP_TIMEOUT_MS
    );


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
        "[CoreClient] Enviando mensaje a Asty..."
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


    Serial.print(
        "[CoreClient] Respuesta completa: "
    );

    Serial.println(
        response
    );


    if (status != 201)
    {
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
