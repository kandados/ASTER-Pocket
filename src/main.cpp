#include <Arduino.h>
#include <WiFi.h>

#include "drivers/display/AsterDisplay.h"
#include "core/CoreClient.h"
#include "secrets.h"


static constexpr uint32_t WIFI_TIMEOUT_MS =
    20000;


// ---------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------

static bool connectWiFi()
{
    Serial.println(
        "[Pocket] Conectando a Cudy-Homelab..."
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Conectando\nWi-Fi..."
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
            "[Pocket] ERROR Wi-Fi."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error\nWi-Fi"
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


    AsterDisplay.showStatus(
        "ASTY",
        "Wi-Fi\nconectado"
    );


    delay(
        800
    );


    return true;
}


// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------

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
        "Asty Real Test v0.5"
    );
    Serial.println(
        "================================"
    );


    // -----------------------------------------------------
    // AMOLED
    // -----------------------------------------------------

    AsterDisplay.begin();


    AsterDisplay.showStatus(
        "A.S.T.E.R.",
        "Pocket\niniciando..."
    );


    delay(
        800
    );


    // -----------------------------------------------------
    // Wi-Fi
    // -----------------------------------------------------

    if (!connectWiFi())
    {
        return;
    }


    // -----------------------------------------------------
    // Core
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Buscando\nCore..."
    );


    if (!CoreClient.checkHealth())
    {
        AsterDisplay.showStatus(
            "ASTY",
            "Core no\ndisponible"
        );

        return;
    }


    Serial.println(
        "[Pocket] Core disponible."
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Core\nconectado"
    );


    delay(
        800
    );


    // -----------------------------------------------------
    // Crear conversación
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Creando\nconversacion..."
    );


    String conversationId;


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
            "Error de\nautenticacion"
        );


        return;
    }


    // -----------------------------------------------------
    // Primera petición real a Asty
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Pensando..."
    );


    const String pocketMessage =
        "Hola Asty. "
        "Este mensaje procede fisicamente de "
        "A.S.T.E.R. Pocket mediante la Waveshare. "
        "Responde con una sola frase muy corta "
        "confirmando que me recibes desde Pocket.";


    String answer;
    String provider;
    String model;


    if (
        !CoreClient.sendMessage(
            conversationId,
            pocketMessage,
            answer,
            provider,
            model
        )
    )
    {
        Serial.println(
            "[Pocket] ERROR hablando con Asty."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error al\nhablar con Core"
        );


        return;
    }


    // -----------------------------------------------------
    // Respuesta real en AMOLED
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        answer.c_str()
    );


    Serial.println();
    Serial.println(
        "[Pocket] PRIMERA RESPUESTA REAL DE ASTY EN POCKET."
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
}


// ---------------------------------------------------------
// Loop
// ---------------------------------------------------------

void loop()
{
    AsterDisplay.update();

    delay(
        5
    );
}
