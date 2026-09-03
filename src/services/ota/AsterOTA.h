#pragma once

#include <Arduino.h>


class AsterOTAClass
{
public:
    bool begin();

    void handle();

    bool ready() const;


private:
    bool _ready = false;

    uint8_t _lastProgress = 255;
};


extern AsterOTAClass AsterOTA;
