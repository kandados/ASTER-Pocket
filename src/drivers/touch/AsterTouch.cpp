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


    // -----------------------------------------------------
    // Estado persistente del puntero
    //
    // Cuando comienza un toque seguimos consultando el
    // CST9217 aunque no llegue una IRQ nueva. De esta forma
    // LVGL conserva correctamente PRESSED durante arrastres.
    // -----------------------------------------------------

    static bool pointerPressed =
        false;

    static int16_t lastX =
        0;

    static int16_t lastY =
        0;

    static int16_t lastLoggedX =
        -1000;

    static int16_t lastLoggedY =
        -1000;


    if (!initialized)

    {

        pointerPressed =
            false;

        data->state =
            LV_INDEV_STATE_REL;

        return;

    }


    // En reposo no hacemos tráfico I2C innecesario.
    // Una IRQ inicia la lectura.
    //
    // Mientras el dedo siga pulsando sí interrogamos el
    // controlador en cada ciclo LVGL para mantener PR y
    // permitir arrastrar sliders correctamente.

    if (

        !touchInterrupt &&
        !pointerPressed

    )

    {

        data->state =
            LV_INDEV_STATE_REL;

        data->point.x =
            lastX;

        data->point.y =
            lastY;

        return;

    }


    touchInterrupt =
        false;


    const uint8_t touched =

        touch.getPoint(

            touchX,

            touchY,

            touch.getSupportTouchPoint()

        );


    if (touched > 0)

    {

        int16_t x =
            touchX[0];

        int16_t y =
            touchY[0];


        if (x < 0)
        {
            x = 0;
        }
        else if (x >= TOUCH_WIDTH)
        {
            x = TOUCH_WIDTH - 1;
        }


        if (y < 0)
        {
            y = 0;
        }
        else if (y >= TOUCH_HEIGHT)
        {
            y = TOUCH_HEIGHT - 1;
        }


        const bool firstPress =
            !pointerPressed;


        pointerPressed =
            true;

        lastX =
            x;

        lastY =
            y;


        data->state =
            LV_INDEV_STATE_PR;

        data->point.x =
            lastX;

        data->point.y =
            lastY;


        // Registro suficiente para comprobar calibración,
        // sin inundar Serial durante un arrastre.

        if (

            firstPress ||

            abs(
                lastX -
                lastLoggedX
            ) >= 5 ||

            abs(
                lastY -
                lastLoggedY
            ) >= 5

        )

        {

            Serial.print(
                "[AsterTouch] x="
            );

            Serial.print(
                lastX
            );

            Serial.print(
                " y="
            );

            Serial.println(
                lastY
            );


            lastLoggedX =
                lastX;

            lastLoggedY =
                lastY;

        }

    }

    else

    {

        pointerPressed =
            false;

        data->state =
            LV_INDEV_STATE_REL;

        data->point.x =
            lastX;

        data->point.y =
            lastY;

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
