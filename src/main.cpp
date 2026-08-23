#include <Arduino.h>
#include <WiFi.h>

#include "drivers/display/AsterDisplay.h"
#include "secrets.h"


static void scanWiFi()
{
    Serial.println();
    Serial.println(
        "[Pocket] Escaneando redes Wi-Fi..."
    );

    AsterDisplay.showStatus(
        "ASTY",
        "Buscando\nWi-Fi..."
    );


    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);

    delay(500);


    const int networkCount =
        WiFi.scanNetworks();


    Serial.print(
        "[Pocket] Redes encontradas: "
    );

    Serial.println(
        networkCount
    );


    if (networkCount <= 0)
    {
        AsterDisplay.showStatus(
            "ASTY",
            "No encuentro\nredes Wi-Fi"
        );

        Serial.println(
            "[Pocket] No se encontró ninguna red."
        );

        return;
    }


    bool targetFound =
        false;


    int targetRSSI =
        0;


    Serial.println();
    Serial.println(
        "----------------------------------------"
    );


    for (
        int i = 0;
        i < networkCount;
        i++
    )
    {
        const String ssid =
            WiFi.SSID(i);

        const int rssi =
            WiFi.RSSI(i);

        const int channel =
            WiFi.channel(i);


        Serial.print(
            i + 1
        );

        Serial.print(
            ". SSID: "
        );

        Serial.print(
            ssid
        );

        Serial.print(
            " | RSSI: "
        );

        Serial.print(
            rssi
        );

        Serial.print(
            " dBm | Canal: "
        );

        Serial.println(
            channel
        );


        if (
            ssid == ASTER_WIFI_SSID
        )
        {
            targetFound =
                true;

            targetRSSI =
                rssi;
        }
    }


    Serial.println(
        "----------------------------------------"
    );

    Serial.println();


    if (targetFound)
    {
        Serial.println(
            "[Pocket] RED DEL HOMELAB ENCONTRADA."
        );

        Serial.print(
            "[Pocket] RSSI: "
        );

        Serial.print(
            targetRSSI
        );

        Serial.println(
            " dBm"
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Wi-Fi del\nhomelab visible"
        );
    }
    else
    {
        Serial.println(
            "[Pocket] ERROR: el SSID configurado "
            "no aparece en el escaneo."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Wi-Fi del\nhomelab NO visible"
        );
    }


    WiFi.scanDelete();
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
        "Wi-Fi Scan Test v0.3"
    );

    Serial.println(
        "================================"
    );


    AsterDisplay.begin();


    AsterDisplay.showStatus(
        "A.S.T.E.R.",
        "Pocket\nWi-Fi test"
    );


    delay(
        1000
    );


    scanWiFi();
}


void loop()
{
    AsterDisplay.update();

    delay(
        5
    );
}