#include "AsterTouch.h"

#include <Arduino.h>
#include <Wire.h>

#include "TouchDrvCSTXXX.hpp"


// ---------------------------------------------------------
// Waveshare ESP32-S3 Touch AMOLED 1.75"
// CST9217
// ---------------------------------------------------------

static constexpr uint8_t TOUCH_SDA =
    15;

static constexpr uint8_t TOUCH_SCL =
    14;

static constexpr uint8_t TOUCH_IRQ =
    11;

static constexpr uint8_t TOUCH_RST =
    40;

static constexpr uint8_t TOUCH_ADDR =
    0x5A;

static constexpr int16_t TOUCH_WIDTH =
    466;

static constexpr int16_t TOUCH_HEIGHT =
    466;


// ---------------------------------------------------------
// Driver físico
// ---------------------------------------------------------

static TouchDrvCST92xx touch;

static int16_t touchX[5];
static int16_t touchY[5];

static volatile bool touchInterrupt =
    false;


// ---------------------------------------------------------
// IRQ
// ---------------------------------------------------------

static void IRAM_ATTR onTouchInterrupt()
{
    touchInterrupt = true;
}


// ---------------------------------------------------------
// Callback LVGL
// ---------------------------------------------------------

static void asterTouchLvglRead(
    lv_indev_drv_t *driver,
    lv_indev_data_t *data
)
{
    AsterTouch.read(
        driver,
        data
    );
}


// ---------------------------------------------------------
// Inicialización
// ---------------------------------------------------------

bool AsterTouchClass::begin()
{
    Serial.println();
    Serial.println(
        "[AsterTouch] Inicializando CST9217..."
    );


    // Reset físico

    pinMode(
        TOUCH_RST,
        OUTPUT
    );

    digitalWrite(
        TOUCH_RST,
        LOW
    );

    delay(
        30
    );

    digitalWrite(
        TOUCH_RST,
        HIGH
    );

    delay(
        100
    );


    // Configuración del controlador

    touch.setPins(
        TOUCH_RST,
        TOUCH_IRQ
    );


    const bool result =
        touch.begin(
            Wire,
            TOUCH_ADDR,
            TOUCH_SDA,
            TOUCH_SCL
        );


    if (!result)
    {
        Serial.println(
            "[AsterTouch] ERROR: CST9217 no encontrado."
        );

        initialized = false;

        return false;
    }


    Serial.print(
        "[AsterTouch] Controlador detectado: "
    );

    Serial.println(
        touch.getModelName()
    );


    // Conservamos la configuración validada
    // previamente para este hardware.

    touch.sleep();

    touch.reset();


    touch.setMaxCoordinates(
        TOUCH_WIDTH,
        TOUCH_HEIGHT
    );


    touch.setMirrorXY(
        true,
        true
    );


    // IRQ

    pinMode(
        TOUCH_IRQ,
        INPUT_PULLUP
    );


    attachInterrupt(
        digitalPinToInterrupt(TOUCH_IRQ),
        onTouchInterrupt,
        FALLING
    );


    // -----------------------------------------------------
    // Registrar como dispositivo táctil LVGL
    // -----------------------------------------------------

    static lv_indev_drv_t inputDriver;


    lv_indev_drv_init(
        &inputDriver
    );


    inputDriver.type =
        LV_INDEV_TYPE_POINTER;


    inputDriver.read_cb =
        asterTouchLvglRead;


    lv_indev_drv_register(
        &inputDriver
    );


    initialized = true;


    Serial.println(
        "[AsterTouch] IRQ configurada."
    );

    Serial.println(
        "[AsterTouch] Resolución: 466 x 466."
    );

    Serial.println(
        "[AsterTouch] Registrado en LVGL."
    );

    Serial.println(
        "[AsterTouch] Táctil preparado."
    );


    return true;
}


// ---------------------------------------------------------
// Lectura para LVGL
// ---------------------------------------------------------

void AsterTouchClass::read(
    lv_indev_drv_t *driver,
    lv_indev_data_t *data
)
{
    (void)driver;


    if (!initialized)
    {
        data->state =
            LV_INDEV_STATE_REL;

        return;
    }


    if (!touchInterrupt)
    {
        data->state =
            LV_INDEV_STATE_REL;

        return;
    }


    touchInterrupt = false;


    const uint8_t touched =
        touch.getPoint(
            touchX,
            touchY,
            touch.getSupportTouchPoint()
        );


    if (touched > 0)
    {
        data->state =
            LV_INDEV_STATE_PR;


        data->point.x =
            touchX[0];

        data->point.y =
            touchY[0];


        Serial.print(
            "[AsterTouch] x="
        );

        Serial.print(
            touchX[0]
        );

        Serial.print(
            " y="
        );

        Serial.println(
            touchY[0]
        );
    }
    else
    {
        data->state =
            LV_INDEV_STATE_REL;
    }
}


// ---------------------------------------------------------
// Estado
// ---------------------------------------------------------

bool AsterTouchClass::isReady() const
{
    return initialized;
}


// ---------------------------------------------------------
// Instancia global
// ---------------------------------------------------------

AsterTouchClass AsterTouch;
