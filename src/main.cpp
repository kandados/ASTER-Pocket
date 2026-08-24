#include <Arduino.h>
#include <WiFi.h>

#include <time.h>

#include "drivers/display/AsterDisplay.h"
#include "core/CoreClient.h"
#include "secrets.h"


static constexpr uint32_t WIFI_TIMEOUT_MS =
    20000;

static constexpr uint32_t TIME_TIMEOUT_MS =
    20000;


// Cualquier fecha posterior a noviembre de 2023
// nos sirve para considerar que NTP ha sincronizado.

static constexpr time_t MINIMUM_VALID_TIME =
    1700000000;


// ---------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------

static bool connectWiFi()
{
    Serial.println();
    Serial.println(
        "[Pocket] Conectando a Wi-Fi externa..."
    );


    Serial.print(
        "[Pocket] SSID: "
    );

    Serial.println(
        ASTER_WIFI_SSID
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
            "[Pocket] ERROR conectando al Wi-Fi."
        );


        Serial.print(
            "[Pocket] WiFi.status(): "
        );

        Serial.println(
            WiFi.status()
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


    Serial.print(
        "[Pocket] Gateway: "
    );

    Serial.println(
        WiFi.gatewayIP()
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Wi-Fi\nconectado"
    );


    delay(
        700
    );


    return true;
}


// ---------------------------------------------------------
// Sincronizar reloj para validar certificados TLS
// ---------------------------------------------------------

static bool synchronizeClock()
{
    Serial.println();
    Serial.println(
        "[Pocket] Sincronizando hora mediante NTP..."
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Sincronizando\nhora..."
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


    const time_t now =
        time(nullptr);


    if (
        now < MINIMUM_VALID_TIME
    )
    {
        Serial.println(
            "[Pocket] ERROR sincronizando reloj."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error de\nhora"
        );


        return false;
    }


    Serial.print(
        "[Pocket] Epoch: "
    );

    Serial.println(
        static_cast<unsigned long>(now)
    );


    struct tm timeInfo;


    if (
        gmtime_r(
            &now,
            &timeInfo
        ) != nullptr
    )
    {
        char buffer[40];


        strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%d %H:%M:%S UTC",
            &timeInfo
        );


        Serial.print(
            "[Pocket] Hora: "
        );

        Serial.println(
            buffer
        );
    }


    Serial.println(
        "[Pocket] Hora válida para TLS."
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
        "Remote Asty Test v0.6"
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
        "Pocket\nremoto v0.6"
    );


    delay(
        800
    );


    // -----------------------------------------------------
    // Wi-Fi externa
    // -----------------------------------------------------

    if (
        !connectWiFi()
    )
    {
        return;
    }


    // -----------------------------------------------------
    // Hora para TLS
    // -----------------------------------------------------

    if (
        !synchronizeClock()
    )
    {
        return;
    }


    // -----------------------------------------------------
    // Comprobar Core remoto
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Conectando\nremotamente..."
    );


    if (
        !CoreClient.checkHealth()
    )
    {
        Serial.println(
            "[Pocket] ERROR: Core remoto no disponible."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Core remoto\nno disponible"
        );


        return;
    }


    Serial.println(
        "[Pocket] Core remoto disponible."
    );


    AsterDisplay.showStatus(
        "ASTY",
        "Core remoto\nconectado"
    );


    delay(
        700
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
    // Primera petición remota física
    // -----------------------------------------------------

    AsterDisplay.showStatus(
        "ASTY",
        "Pensando..."
    );


    const String pocketMessage =
        "Hola Asty. "
        "Esta peticion procede fisicamente de "
        "A.S.T.E.R. Pocket desde una red Wi-Fi externa "
        "al homelab mediante Internet y HTTPS. "
        "Responde con una sola frase muy corta confirmando "
        "que recibes correctamente a Pocket de forma remota.";


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
            "[Pocket] ERROR hablando remotamente con Asty."
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
        "================================"
    );

    Serial.println(
        "[Pocket] PRIMERA RESPUESTA REMOTA DE ASTY."
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
