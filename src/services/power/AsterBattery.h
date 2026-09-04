#pragma once

#include <Arduino.h>


struct AsterBatteryStatus
{
    bool valid = false;

    bool batteryConnected = false;

    bool charging = false;

    bool vbusConnected = false;

    uint16_t batteryVoltageMv = 0;

    uint16_t vbusVoltageMv = 0;

    uint8_t percent = 0;
};


class AsterBatteryClass
{
public:

    bool begin();

    bool read(
        AsterBatteryStatus &status
    );

    bool ready() const
    {
        return initialized;
    }

    void printStatus();

private:

    bool initialized = false;
};


extern AsterBatteryClass AsterBattery;
