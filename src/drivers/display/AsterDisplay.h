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

    void showReply(
        const char *reply
    );

    bool consumeSendRequest(
        String &message
    );

    void update();
};


extern AsterDisplayClass AsterDisplay;
