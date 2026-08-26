#include "AsterAudioWireControl.h"

#include <Arduino.h>
#include <Wire.h>

#include <cstring>

namespace
{
    constexpr uint8_t ES7210_I2C_ADDRESS = 0x40;

    struct AsterWireControl
    {
        audio_codec_ctrl_if_t base;
        bool opened;
    };

    AsterWireControl control{};

    int controlOpen(
        const audio_codec_ctrl_if_t *ctrl,
        void *cfg,
        int cfgSize
    )
    {
        (void)cfg;
        (void)cfgSize;

        auto *instance =
            reinterpret_cast<AsterWireControl *>(
                const_cast<audio_codec_ctrl_if_t *>(ctrl)
            );

        instance->opened = true;

        return 0;
    }

    bool controlIsOpen(
        const audio_codec_ctrl_if_t *ctrl
    )
    {
        const auto *instance =
            reinterpret_cast<const AsterWireControl *>(ctrl);

        return instance->opened;
    }

    int controlReadRegister(
        const audio_codec_ctrl_if_t *ctrl,
        int address,
        int addressLength,
        void *data,
        int dataLength
    )
    {
        (void)ctrl;

        if (
            addressLength != 1 ||
            data == nullptr ||
            dataLength <= 0
        )
        {
            return -1;
        }

        Wire.beginTransmission(
            ES7210_I2C_ADDRESS
        );

        Wire.write(
            static_cast<uint8_t>(address)
        );

        if (
            Wire.endTransmission(false) != 0
        )
        {
            return -1;
        }

        const uint8_t requested =
            static_cast<uint8_t>(dataLength);

        const uint8_t received =
            Wire.requestFrom(
                ES7210_I2C_ADDRESS,
                requested,
                static_cast<uint8_t>(true)
            );

        if (received != requested)
        {
            return -1;
        }

        auto *output =
            static_cast<uint8_t *>(data);

        for (
            int index = 0;
            index < dataLength;
            ++index
        )
        {
            if (!Wire.available())
            {
                return -1;
            }

            output[index] =
                static_cast<uint8_t>(
                    Wire.read()
                );
        }

        return 0;
    }

    int controlWriteRegister(
        const audio_codec_ctrl_if_t *ctrl,
        int address,
        int addressLength,
        void *data,
        int dataLength
    )
    {
        (void)ctrl;

        if (
            addressLength != 1 ||
            data == nullptr ||
            dataLength <= 0
        )
        {
            return -1;
        }

        Wire.beginTransmission(
            ES7210_I2C_ADDRESS
        );

        Wire.write(
            static_cast<uint8_t>(address)
        );

        const auto *input =
            static_cast<const uint8_t *>(data);

        for (
            int index = 0;
            index < dataLength;
            ++index
        )
        {
            Wire.write(
                input[index]
            );
        }

        return
            Wire.endTransmission() == 0
                ? 0
                : -1;
    }

    int controlClose(
        const audio_codec_ctrl_if_t *ctrl
    )
    {
        auto *instance =
            reinterpret_cast<AsterWireControl *>(
                const_cast<audio_codec_ctrl_if_t *>(ctrl)
            );

        instance->opened = false;

        return 0;
    }
}

const audio_codec_ctrl_if_t *
asterAudioWireControlCreate()
{
    std::memset(
        &control,
        0,
        sizeof(control)
    );

    control.base.open =
        controlOpen;

    control.base.is_open =
        controlIsOpen;

    control.base.read_reg =
        controlReadRegister;

    control.base.write_reg =
        controlWriteRegister;

    control.base.close =
        controlClose;

    if (
        control.base.open(
            &control.base,
            nullptr,
            0
        ) != 0
    )
    {
        return nullptr;
    }

    return &control.base;
}
