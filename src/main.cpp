#include <Arduino.h>
#include <WiFi.h>
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
    // Wi-Fi
    // -----------------------------------------------------

    if (!connectWiFi())
    {
        return;
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


    if (!CoreClient.checkHealth())
    {
        Serial.println(
            "[Pocket] ERROR: Core remoto no disponible."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Core no disponible"
        );


        return;
    }


    Serial.println(
        "[Pocket] Core remoto disponible."
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
    AsterDisplay.update();


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
                "[AsterVoice] Empieza a hablar en 2 segundos..."
            );

            delay(
                2000
            );

            Serial.println(
                "[AsterVoice] HABLA AHORA."
            );

            AsterVoice.record(
                5000
            );

            Serial.println(
                "[AsterVoice] Fin de prueba."
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
