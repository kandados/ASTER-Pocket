#include "AsterDisplay.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>


// ---------------------------------------------------------
// Waveshare ESP32-S3 Touch AMOLED 1.75"
// ---------------------------------------------------------

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7

#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39

#define LCD_WIDTH 466
#define LCD_HEIGHT 466

#define ASTER_LVGL_TICK_PERIOD_MS 2


// ---------------------------------------------------------
// LVGL
// ---------------------------------------------------------

static lv_disp_draw_buf_t drawBuffer;

static lv_color_t displayBuffer[
    LCD_WIDTH * LCD_HEIGHT / 10
];


// ---------------------------------------------------------
// AMOLED
// ---------------------------------------------------------

static Arduino_DataBus *bus =
    new Arduino_ESP32QSPI(
        LCD_CS,
        LCD_SCLK,
        LCD_SDIO0,
        LCD_SDIO1,
        LCD_SDIO2,
        LCD_SDIO3
    );


static Arduino_CO5300 *gfx =
    new Arduino_CO5300(
        bus,
        LCD_RESET,
        0,
        LCD_WIDTH,
        LCD_HEIGHT,
        6,
        0,
        0,
        0
    );


// ---------------------------------------------------------
// Flush LVGL → AMOLED
// ---------------------------------------------------------

static void asterDisplayFlush(
    lv_disp_drv_t *display,
    const lv_area_t *area,
    lv_color_t *color
)
{
    const uint32_t width =
        area->x2 - area->x1 + 1;

    const uint32_t height =
        area->y2 - area->y1 + 1;


#if (LV_COLOR_16_SWAP != 0)

    gfx->draw16bitBeRGBBitmap(
        area->x1,
        area->y1,
        reinterpret_cast<uint16_t *>(
            &color->full
        ),
        width,
        height
    );

#else

    gfx->draw16bitRGBBitmap(
        area->x1,
        area->y1,
        reinterpret_cast<uint16_t *>(
            &color->full
        ),
        width,
        height
    );

#endif


    lv_disp_flush_ready(
        display
    );
}


// ---------------------------------------------------------
// Tick LVGL
// ---------------------------------------------------------

static void asterLvglTick(
    void *argument
)
{
    (void)argument;

    lv_tick_inc(
        ASTER_LVGL_TICK_PERIOD_MS
    );
}


// ---------------------------------------------------------
// Inicialización de pantalla
// ---------------------------------------------------------

void AsterDisplayClass::begin()
{
    Serial.println(
        "[AsterDisplay] Inicializando AMOLED..."
    );


    // Inicializar controlador físico

    gfx->begin();


    // Brillo AMOLED

    gfx->setBrightness(
        200
    );


    // Inicializar LVGL

    lv_init();


    // Buffer de dibujo

    lv_disp_draw_buf_init(
        &drawBuffer,
        displayBuffer,
        nullptr,
        LCD_WIDTH * LCD_HEIGHT / 10
    );


    // Driver de pantalla LVGL

    static lv_disp_drv_t displayDriver;

    lv_disp_drv_init(
        &displayDriver
    );

    displayDriver.hor_res =
        LCD_WIDTH;

    displayDriver.ver_res =
        LCD_HEIGHT;

    displayDriver.flush_cb =
        asterDisplayFlush;

    displayDriver.draw_buf =
        &drawBuffer;

    lv_disp_drv_register(
        &displayDriver
    );


    // Temporizador necesario para LVGL

    const esp_timer_create_args_t timerArgs = {
        .callback = &asterLvglTick,
        .name = "aster_lvgl_tick"
    };


    esp_timer_handle_t timer =
        nullptr;


    esp_err_t timerResult =
        esp_timer_create(
            &timerArgs,
            &timer
        );


    if (
        timerResult == ESP_OK
    )
    {
        esp_err_t startResult =
            esp_timer_start_periodic(
                timer,
                ASTER_LVGL_TICK_PERIOD_MS
                    * 1000
            );


        if (
            startResult != ESP_OK
        )
        {
            Serial.println(
                "[AsterDisplay] ERROR iniciando temporizador LVGL"
            );
        }
    }
    else
    {
        Serial.println(
            "[AsterDisplay] ERROR creando temporizador LVGL"
        );
    }


    Serial.println(
        "[AsterDisplay] AMOLED preparada."
    );
}


// ---------------------------------------------------------
// Pantalla básica de estado
// ---------------------------------------------------------

void AsterDisplayClass::showStatus(
    const char *titleText,
    const char *messageText
)
{
    lv_obj_t *screen =
        lv_scr_act();


    // Limpiar pantalla anterior

    lv_obj_clean(
        screen
    );


    // -----------------------------------------------------
    // Fondo AMOLED negro
    // -----------------------------------------------------

    lv_obj_set_style_bg_color(
        screen,
        lv_color_black(),
        0
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        0
    );


    // -----------------------------------------------------
    // Título
    // -----------------------------------------------------

    lv_obj_t *title =
        lv_label_create(
            screen
        );


    lv_label_set_long_mode(
        title,
        LV_LABEL_LONG_WRAP
    );


    lv_label_set_text(
        title,
        titleText
    );


    lv_obj_set_width(
        title,
        420
    );


    lv_obj_set_style_text_color(
        title,
        lv_color_white(),
        0
    );


    // Fuente grande

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_36,
        0
    );


    lv_obj_set_style_text_align(
        title,
        LV_TEXT_ALIGN_CENTER,
        0
    );


    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        65
    );


    // -----------------------------------------------------
    // Mensaje
    // -----------------------------------------------------

    lv_obj_t *message =
        lv_label_create(
            screen
        );


    lv_label_set_long_mode(
        message,
        LV_LABEL_LONG_WRAP
    );


    lv_label_set_text(
        message,
        messageText
    );


    lv_obj_set_width(
        message,
        400
    );


    lv_obj_set_style_text_color(
        message,
        lv_color_white(),
        0
    );


    // Fuente grande

    lv_obj_set_style_text_font(
        message,
        &lv_font_montserrat_36,
        0
    );


    lv_obj_set_style_text_align(
        message,
        LV_TEXT_ALIGN_CENTER,
        0
    );


    lv_obj_align(
        message,
        LV_ALIGN_CENTER,
        0,
        45
    );


    // Forzar primer refresco

    lv_timer_handler();
}


// ---------------------------------------------------------
// Actualización LVGL
// ---------------------------------------------------------

void AsterDisplayClass::update()
{
    lv_timer_handler();
}


// ---------------------------------------------------------
// Instancia global
// ---------------------------------------------------------

AsterDisplayClass AsterDisplay;