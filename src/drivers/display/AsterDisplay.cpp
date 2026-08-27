#include "AsterDisplay.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>


LV_FONT_DECLARE(aster_montserrat_24);


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
// Estado de la interfaz
// ---------------------------------------------------------

static lv_obj_t *messageInput =
    nullptr;

static lv_obj_t *keyboard =
    nullptr;

static String pendingMessage;

static bool sendRequested =
    false;


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
// Pantalla base
// ---------------------------------------------------------

static lv_obj_t *prepareScreen()
{
    messageInput =
        nullptr;

    keyboard =
        nullptr;


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
// Solicitar envío
// ---------------------------------------------------------

static void requestSend()
{
    if (messageInput == nullptr)
    {
        return;
    }


    const char *text =
        lv_textarea_get_text(
            messageInput
        );


    if (text == nullptr)
    {
        return;
    }


    String message =
        String(text);


    message.trim();


    if (message.length() == 0)
    {
        Serial.println(
            "[AsterDisplay] Mensaje vacío. No se envía."
        );

        return;
    }


    pendingMessage =
        message;


    sendRequested =
        true;


    Serial.print(
        "[AsterDisplay] Solicitud de envío: "
    );

    Serial.println(
        pendingMessage
    );
}


// ---------------------------------------------------------
// Botón ENVIAR
// ---------------------------------------------------------

static void sendButtonEvent(
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


    requestSend();
}


// ---------------------------------------------------------
// Tecla OK del teclado
// ---------------------------------------------------------

static void keyboardReadyEvent(
    lv_event_t *event
)
{
    if (
        lv_event_get_code(event)
        != LV_EVENT_READY
    )
    {
        return;
    }


    requestSend();
}


// ---------------------------------------------------------
// Nueva pregunta
// ---------------------------------------------------------

static void newQuestionEvent(
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


    AsterDisplay.showChatInput();
}


// ---------------------------------------------------------
// Inicialización AMOLED + LVGL
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
// Pantalla de estado
// ---------------------------------------------------------

void AsterDisplayClass::showStatus(
    const char *titleText,
    const char *messageText
)
{
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
        70
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
        390
    );


    lv_obj_set_style_text_color(
        message,
        lv_color_white(),
        0
    );


    lv_obj_set_style_text_font(
        message,
        &aster_montserrat_24,
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
        35
    );


    lv_timer_handler();
}


// ---------------------------------------------------------
// Interfaz para escribir a Asty
// ---------------------------------------------------------

void AsterDisplayClass::showChatInput()
{
    sendRequested =
        false;

    pendingMessage =
        "";


    lv_obj_t *screen =
        prepareScreen();


    // -----------------------------------------------------
    // Título
    // -----------------------------------------------------

    lv_obj_t *title =
        lv_label_create(
            screen
        );


    lv_label_set_text(
        title,
        "ASTY"
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


    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        12
    );


    // -----------------------------------------------------
    // Campo de texto
    // -----------------------------------------------------

    messageInput =
        lv_textarea_create(
            screen
        );


    lv_obj_set_size(
        messageInput,
        410,
        92
    );


    lv_obj_align(
        messageInput,
        LV_ALIGN_TOP_MID,
        0,
        62
    );


    lv_textarea_set_placeholder_text(
        messageInput,
        "Escribe a Asty..."
    );


    lv_textarea_set_max_length(
        messageInput,
        240
    );


    lv_textarea_set_one_line(
        messageInput,
        false
    );


    lv_obj_set_style_text_font(
        messageInput,
        &aster_montserrat_24,
        0
    );


    // -----------------------------------------------------
    // Botón enviar
    // -----------------------------------------------------

    lv_obj_t *sendButton =
        lv_btn_create(
            screen
        );


    lv_obj_set_size(
        sendButton,
        160,
        48
    );


    lv_obj_align(
        sendButton,
        LV_ALIGN_TOP_MID,
        0,
        163
    );


    lv_obj_set_style_radius(
        sendButton,
        20,
        0
    );


    lv_obj_add_event_cb(
        sendButton,
        sendButtonEvent,
        LV_EVENT_CLICKED,
        nullptr
    );


    lv_obj_t *sendLabel =
        lv_label_create(
            sendButton
        );


    lv_label_set_text(
        sendLabel,
        "ENVIAR"
    );


    lv_obj_set_style_text_font(
        sendLabel,
        &aster_montserrat_24,
        0
    );


    lv_obj_center(
        sendLabel
    );


    // -----------------------------------------------------
    // Teclado LVGL
    // -----------------------------------------------------

    keyboard =
        lv_keyboard_create(
            screen
        );


    lv_obj_set_size(
        keyboard,
        440,
        240
    );


    lv_obj_align(
        keyboard,
        LV_ALIGN_BOTTOM_MID,
        0,
        0
    );


    lv_keyboard_set_textarea(
        keyboard,
        messageInput
    );


    lv_obj_add_event_cb(
        keyboard,
        keyboardReadyEvent,
        LV_EVENT_READY,
        nullptr
    );


    Serial.println(
        "[AsterDisplay] Interfaz de escritura preparada."
    );


    lv_timer_handler();
}


// ---------------------------------------------------------
// Mostrar respuesta de Asty
// ---------------------------------------------------------

void AsterDisplayClass::showReply(
    const char *reply
)
{
    lv_obj_t *screen =
        prepareScreen();


    // -----------------------------------------------------
    // Título
    // -----------------------------------------------------

    lv_obj_t *title =
        lv_label_create(
            screen
        );


    lv_label_set_text(
        title,
        "ASTY"
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


    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        25
    );


    // -----------------------------------------------------
    // Área desplazable de respuesta
    // -----------------------------------------------------

    lv_obj_t *responsePanel =
        lv_obj_create(
            screen
        );


    lv_obj_set_size(
        responsePanel,
        410,
        275
    );


    lv_obj_align(
        responsePanel,
        LV_ALIGN_CENTER,
        0,
        0
    );


    lv_obj_set_style_bg_color(
        responsePanel,
        lv_color_black(),
        0
    );


    lv_obj_set_style_bg_opa(
        responsePanel,
        LV_OPA_COVER,
        0
    );


    lv_obj_set_style_border_width(
        responsePanel,
        0,
        0
    );


    lv_obj_set_scroll_dir(
        responsePanel,
        LV_DIR_VER
    );


    lv_obj_t *responseLabel =
        lv_label_create(
            responsePanel
        );


    lv_obj_set_width(
        responseLabel,
        370
    );


    lv_label_set_long_mode(
        responseLabel,
        LV_LABEL_LONG_WRAP
    );


    lv_label_set_text(
        responseLabel,
        reply
    );


    lv_obj_set_style_text_color(
        responseLabel,
        lv_color_white(),
        0
    );


    lv_obj_set_style_text_font(
        responseLabel,
        &aster_montserrat_24,
        0
    );


    lv_obj_align(
        responseLabel,
        LV_ALIGN_TOP_MID,
        0,
        0
    );


    // -----------------------------------------------------
    // Nueva pregunta
    // -----------------------------------------------------

    lv_obj_t *newButton =
        lv_btn_create(
            screen
        );


    lv_obj_set_size(
        newButton,
        250,
        55
    );


    lv_obj_align(
        newButton,
        LV_ALIGN_BOTTOM_MID,
        0,
        -25
    );


    lv_obj_set_style_radius(
        newButton,
        22,
        0
    );


    lv_obj_add_event_cb(
        newButton,
        newQuestionEvent,
        LV_EVENT_CLICKED,
        nullptr
    );


    lv_obj_t *newLabel =
        lv_label_create(
            newButton
        );


    lv_label_set_text(
        newLabel,
        "NUEVA PREGUNTA"
    );


    lv_obj_set_style_text_font(
        newLabel,
        &aster_montserrat_24,
        0
    );


    lv_obj_center(
        newLabel
    );


    lv_timer_handler();
}


// ---------------------------------------------------------
// Recuperar petición pendiente
// ---------------------------------------------------------

bool AsterDisplayClass::consumeSendRequest(
    String &message
)
{
    if (!sendRequested)
    {
        return false;
    }


    message =
        pendingMessage;


    pendingMessage =
        "";


    sendRequested =
        false;


    return true;
}


// ---------------------------------------------------------
// Update
// ---------------------------------------------------------

void AsterDisplayClass::update()
{
    lv_timer_handler();
}


// ---------------------------------------------------------
// Instancia global
// ---------------------------------------------------------

AsterDisplayClass AsterDisplay;
