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
// Elementos prueba táctil
// ---------------------------------------------------------

static lv_obj_t *touchButton =
    nullptr;

static lv_obj_t *touchButtonLabel =
    nullptr;


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
// Diagnóstico del evento táctil
// ---------------------------------------------------------

static void touchButtonEvent(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event)
        != LV_EVENT_CLICKED
    )
    {
        return;
    }


    lv_obj_t *target =
        lv_event_get_target(event);


    lv_area_t area;

    lv_obj_get_coords(
        target,
        &area
    );


    lv_point_t point = {
        0,
        0
    };


    lv_indev_t *input =
        lv_indev_get_act();


    if (input != nullptr)
    {
        lv_indev_get_point(
            input,
            &point
        );
    }


    Serial.println();
    Serial.println(
        "----- LVGL CLICK -----"
    );


    Serial.print(
        "[LVGL] Punto evento x="
    );

    Serial.print(
        point.x
    );

    Serial.print(
        " y="
    );

    Serial.println(
        point.y
    );


    Serial.print(
        "[LVGL] Boton x1="
    );

    Serial.print(
        area.x1
    );

    Serial.print(
        " x2="
    );

    Serial.print(
        area.x2
    );

    Serial.print(
        " y1="
    );

    Serial.print(
        area.y1
    );

    Serial.print(
        " y2="
    );

    Serial.println(
        area.y2
    );


    const bool inside =
        point.x >= area.x1 &&
        point.x <= area.x2 &&
        point.y >= area.y1 &&
        point.y <= area.y2;


    Serial.print(
        "[LVGL] Punto dentro del boton: "
    );

    Serial.println(
        inside ? "SI" : "NO"
    );


    Serial.println(
        "----------------------"
    );


    if (touchButtonLabel != nullptr)
    {
        lv_label_set_text(
            touchButtonLabel,
            "TACTO\nDETECTADO"
        );
    }
}


// ---------------------------------------------------------
// Preparar pantalla
// ---------------------------------------------------------

static lv_obj_t *prepareScreen()
{
    lv_obj_t *screen =
        lv_scr_act();


    lv_obj_clean(
        screen
    );


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


    return screen;
}


// ---------------------------------------------------------
// Inicialización
// ---------------------------------------------------------

void AsterDisplayClass::begin()
{
    Serial.println(
        "[AsterDisplay] Inicializando AMOLED..."
    );


    gfx->begin();


    gfx->setBrightness(
        200
    );


    lv_init();


    lv_disp_draw_buf_init(
        &drawBuffer,
        displayBuffer,
        nullptr,
        LCD_WIDTH * LCD_HEIGHT / 10
    );


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


    const esp_timer_create_args_t timerArgs = {
        .callback = &asterLvglTick,
        .name = "aster_lvgl_tick"
    };


    esp_timer_handle_t timer =
        nullptr;


    const esp_err_t timerResult =
        esp_timer_create(
            &timerArgs,
            &timer
        );


    if (timerResult == ESP_OK)
    {
        const esp_err_t startResult =
            esp_timer_start_periodic(
                timer,
                ASTER_LVGL_TICK_PERIOD_MS
                    * 1000
            );


        if (startResult != ESP_OK)
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
// Pantalla básica
// ---------------------------------------------------------

void AsterDisplayClass::showStatus(
    const char *titleText,
    const char *messageText
)
{
    touchButton =
        nullptr;

    touchButtonLabel =
        nullptr;


    lv_obj_t *screen =
        prepareScreen();


    lv_obj_t *title =
        lv_label_create(
            screen
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


    lv_timer_handler();
}


// ---------------------------------------------------------
// Prueba táctil v0.7
// ---------------------------------------------------------

void AsterDisplayClass::showTouchTest()
{
    lv_obj_t *screen =
        prepareScreen();


    lv_obj_t *title =
        lv_label_create(
            screen
        );


    lv_label_set_text(
        title,
        "ASTY"
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
        55
    );


    lv_obj_t *instruction =
        lv_label_create(
            screen
        );


    lv_label_set_text(
        instruction,
        "Prueba tactil"
    );


    lv_obj_set_width(
        instruction,
        400
    );


    lv_obj_set_style_text_color(
        instruction,
        lv_color_white(),
        0
    );


    lv_obj_set_style_text_font(
        instruction,
        &lv_font_montserrat_24,
        0
    );


    lv_obj_set_style_text_align(
        instruction,
        LV_TEXT_ALIGN_CENTER,
        0
    );


    lv_obj_align(
        instruction,
        LV_ALIGN_TOP_MID,
        0,
        115
    );


    // -----------------------------------------------------
    // Botón
    // -----------------------------------------------------

    touchButton =
        lv_btn_create(
            screen
        );


    lv_obj_set_size(
        touchButton,
        330,
        160
    );


    lv_obj_align(
        touchButton,
        LV_ALIGN_CENTER,
        0,
        45
    );


    lv_obj_set_style_radius(
        touchButton,
        35,
        0
    );


    lv_obj_set_style_bg_color(
        touchButton,
        lv_color_make(
            30,
            30,
            30
        ),
        0
    );


    lv_obj_set_style_border_width(
        touchButton,
        2,
        0
    );


    lv_obj_set_style_border_color(
        touchButton,
        lv_color_white(),
        0
    );


    lv_obj_add_event_cb(
        touchButton,
        touchButtonEvent,
        LV_EVENT_CLICKED,
        nullptr
    );


    touchButtonLabel =
        lv_label_create(
            touchButton
        );


    lv_label_set_text(
        touchButtonLabel,
        "TOCA AQUI"
    );


    lv_obj_set_width(
        touchButtonLabel,
        290
    );


    lv_obj_set_style_text_color(
        touchButtonLabel,
        lv_color_white(),
        0
    );


    lv_obj_set_style_text_font(
        touchButtonLabel,
        &lv_font_montserrat_36,
        0
    );


    lv_obj_set_style_text_align(
        touchButtonLabel,
        LV_TEXT_ALIGN_CENTER,
        0
    );


    lv_obj_center(
        touchButtonLabel
    );


    // -----------------------------------------------------
    // Mostrar límites reales calculados por LVGL
    // -----------------------------------------------------

    lv_obj_update_layout(
        screen
    );


    lv_area_t area;


    lv_obj_get_coords(
        touchButton,
        &area
    );


    Serial.println();
    Serial.println(
        "===== BOTON LVGL ====="
    );


    Serial.print(
        "x1="
    );

    Serial.print(
        area.x1
    );

    Serial.print(
        " x2="
    );

    Serial.print(
        area.x2
    );

    Serial.print(
        " y1="
    );

    Serial.print(
        area.y1
    );

    Serial.print(
        " y2="
    );

    Serial.println(
        area.y2
    );


    Serial.println(
        "======================"
    );


    lv_timer_handler();
}


// ---------------------------------------------------------
// Update
// ---------------------------------------------------------

void AsterDisplayClass::update()
{
    lv_timer_handler();
}


// ---------------------------------------------------------
// Instancia
// ---------------------------------------------------------

AsterDisplayClass AsterDisplay;
