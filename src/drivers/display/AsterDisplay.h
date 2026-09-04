#pragma once

#include <Arduino.h>


class AsterDisplayClass
{
public:

    void begin();

    void showStatus(
        const char *title,
        const char *message
    );

    void showChatInput();

    void showVolumeControl();

    void setVolumeLevel(
        uint8_t volume
    );

    bool consumeVolumeChange(
        uint8_t &volume
    );

    void setBatteryStatus(
        uint8_t percent,
        uint16_t batteryVoltageMv,
        bool batteryConnected,
        bool charging,
        bool vbusConnected
    );

    void showBatteryStatus();

    void showReply(
        const char *reply
    );

    void beginReplyStream();

    void appendReplyStream(
        const char *text
    );

    void endReplyStream();

    bool consumeSendRequest(
        String &message
    );

    void serviceUi();

    void update();
};


extern AsterDisplayClass AsterDisplay;
