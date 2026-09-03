#include "AsterOTA.h"

#include <ArduinoOTA.h>
#include <WiFi.h>

#include "local/AsterOTASecret.h"


AsterOTAClass AsterOTA;


bool AsterOTAClass::begin()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "[AsterOTA] ERROR: Wi-Fi no conectado."
        );

        return false;
    }

    if (
        strlen(ASTER_OTA_PASSWORD) <
        8
    )
    {
        Serial.println(
            "[AsterOTA] ERROR: contraseña OTA inválida."
        );

        return false;
    }

    ArduinoOTA.setHostname(
        "aster-pocket"
    );

    ArduinoOTA.setPort(
        3232
    );

    ArduinoOTA.setPassword(
        ASTER_OTA_PASSWORD
    );

    ArduinoOTA.setRebootOnSuccess(
        true
    );

    ArduinoOTA.onStart(
        [this]()
        {
            _lastProgress = 255;

            Serial.println();
            Serial.println(
                "[AsterOTA] Actualización iniciada."
            );

            if (
                ArduinoOTA.getCommand() ==
                U_FLASH
            )
            {
                Serial.println(
                    "[AsterOTA] Destino: firmware."
                );
            }
            else
            {
                Serial.println(
                    "[AsterOTA] Destino: filesystem."
                );
            }
        }
    );

    ArduinoOTA.onProgress(
        [this](
            unsigned int progress,
            unsigned int total
        )
        {
            if (total == 0)
            {
                return;
            }

            const uint8_t percent =
                static_cast<uint8_t>(
                    (
                        static_cast<uint64_t>(
                            progress
                        ) *
                        100ULL
                    ) /
                    total
                );

            if (
                percent == _lastProgress
            )
            {
                return;
            }

            if (
                percent == 100 ||
                percent % 10 == 0
            )
            {
                Serial.printf(
                    "[AsterOTA] Progreso: %u%%\n",
                    static_cast<unsigned>(
                        percent
                    )
                );
            }

            _lastProgress =
                percent;
        }
    );

    ArduinoOTA.onEnd(
        []()
        {
            Serial.println(
                "[AsterOTA] Actualización completada."
            );

            Serial.println(
                "[AsterOTA] Reiniciando Pocket..."
            );
        }
    );

    ArduinoOTA.onError(
        [](
            ota_error_t error
        )
        {
            Serial.printf(
                "[AsterOTA] ERROR OTA: %u\n",
                static_cast<unsigned>(
                    error
                )
            );
        }
    );

    ArduinoOTA.begin();

    _ready = true;

    Serial.println();
    Serial.println(
        "[AsterOTA] Servicio OTA local activo."
    );

    Serial.print(
        "[AsterOTA] Host: aster-pocket.local"
    );

    Serial.println();

    Serial.print(
        "[AsterOTA] IP: "
    );

    Serial.println(
        WiFi.localIP()
    );

    Serial.println(
        "[AsterOTA] Puerto: 3232"
    );

    Serial.println(
        "[AsterOTA] Autenticación: activa"
    );

    return true;
}


void AsterOTAClass::handle()
{
    if (!_ready)
    {
        return;
    }

    ArduinoOTA.handle();
}


bool AsterOTAClass::ready() const
{
    return _ready;
}
