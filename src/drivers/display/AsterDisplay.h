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
    void update();
};

extern AsterDisplayClass AsterDisplay;
