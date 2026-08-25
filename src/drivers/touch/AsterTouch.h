#pragma once

#include <Arduino.h>
#include <lvgl.h>


class AsterTouchClass
{
public:

    bool begin();

    void read(
        lv_indev_drv_t *driver,
        lv_indev_data_t *data
    );

    bool isReady() const;


private:

    bool initialized = false;
};


extern AsterTouchClass AsterTouch;
