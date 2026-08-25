#include <Arduino.h>

#include "drivers/display/AsterDisplay.h"
#include "drivers/touch/AsterTouch.h"


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
        "Touch Test v0.7"
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
        "Iniciando\ntactil..."
    );


    delay(
        500
    );


    // -----------------------------------------------------
    // CST9217
    // -----------------------------------------------------

    if (!AsterTouch.begin())
    {
        Serial.println(
            "[Pocket] ERROR inicializando táctil."
        );


        AsterDisplay.showStatus(
            "ASTY",
            "Error\ntactil"
        );


        return;
    }


    Serial.println(
        "[Pocket] Táctil preparado."
    );


    // -----------------------------------------------------
    // Prueba interactiva
    // -----------------------------------------------------

    AsterDisplay.showTouchTest();


    Serial.println();
    Serial.println(
        "[Pocket] Toca el botón de la pantalla."
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
