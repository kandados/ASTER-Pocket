#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "drivers/display/AsterDisplay.h"
#include "secrets.h"


static constexpr unsigned long WIFI_TIMEOUT_MS = 20000;
static constexpr unsigned long CORE_TIMEOUT_MS = 10000;


// ---------------------------------------------------------
// Conectar al Wi-Fi del homelab
// ---------------------------------------------------------

static bool connectWiFi()
{
    Serial.println();
    Serial.println("[Pocket] Conectando a Wi-Fi...");
    Serial.print("[Pocket] SSID: ");
    Serial.println(ASTER_WIFI_SSID);

    AsterDisplay.showStatus(
        "ASTY",
        "Conectando\nWi-Fi..."
    );

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    WiFi.begin(
        ASTER_WIFI_SSID,
        ASTER_WIFI_PASSWORD
    );

    const unsigned long start = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - start < WIFI_TIMEOUT_MS
    )
    {
        AsterDisplay.update();
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println();
        Serial.println("[Pocket] ERROR conectando al Wi-Fi.");

        Serial.print("[Pocket] WiFi.status(): ");
        Serial.println(WiFi.status());

        AsterDisplay.showStatus(
            "ASTY",
            "Error\nWi-Fi"
        );

        return false;
    }

    Serial.println();
    Serial.println("[Pocket] Wi-Fi conectado.");

    Serial.print("[Pocket] IP Pocket: ");
    Serial.println(WiFi.localIP());

    Serial.print("[Pocket] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    Serial.print("[Pocket] Gateway: ");
    Serial.println(WiFi.gatewayIP());

    AsterDisplay.showStatus(
        "ASTY",
        "Wi-Fi\nconectado"
    );

    delay(1200);

    return true;
}


// ---------------------------------------------------------
// Comprobar Core
// ---------------------------------------------------------

static bool checkCore()
{
    Serial.println();
    Serial.println("[Pocket] Buscando A.S.T.E.R. Core...");

    AsterDisplay.showStatus(
        "ASTY",
        "Buscando\nCore..."
    );

    HTTPClient http;

    const String url =
        String(ASTER_CORE_URL) +
        "/health";

    Serial.print("[Pocket] GET ");
    Serial.println(url);

    if (!http.begin(url))
    {
        Serial.println(
            "[Pocket] ERROR inicializando HTTP."
        );

        AsterDisplay.showStatus(
            "ASTY",
            "Error\nHTTP"
        );

        return false;
    }

    http.setTimeout(
        CORE_TIMEOUT_MS
    );

    const int statusCode =
        http.GET();

    const String response =
        http.getString();

    http.end();

    Serial.print("[Pocket] HTTP: ");
    Serial.println(statusCode);

    Serial.print("[Pocket] Core: ");
    Serial.println(response);

    if (statusCode != 200)
    {
        Serial.println(
            "[Pocket] ERROR: Core no disponible."
        );

        AsterDisplay.showStatus(
            "ASTY",
            "Core no\nresponde"
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "[Pocket] A.S.T.E.R. Core disponible."
    );

    AsterDisplay.showStatus(
        "ASTY",
        "Wi-Fi OK\n\nCore disponible"
    );

    return true;
}


// ---------------------------------------------------------
// Setup
// ---------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println(
        "================================"
    );
    Serial.println(
        "A.S.T.E.R. Pocket"
    );
    Serial.println(
        "Wi-Fi + Core Test v0.4"
    );
    Serial.println(
        "================================"
    );

    AsterDisplay.begin();

    AsterDisplay.showStatus(
        "A.S.T.E.R.",
        "Pocket\niniciando..."
    );

    delay(1000);

    if (!connectWiFi())
    {
        return;
    }

    if (!checkCore())
    {
        return;
    }

    Serial.println();
    Serial.println(
        "[Pocket] TEST COMPLETADO."
    );
}


// ---------------------------------------------------------
// Loop
// ---------------------------------------------------------

void loop()
{
    AsterDisplay.update();

    delay(5);
}  
