#include "AsterBattery.h"

#include <Wire.h>


#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>


namespace
{

XPowersPMU power;


// ---------------------------------------------------------
// Lectura I2C usando el Wire YA inicializado por AsterTouch.
// No llamamos a Wire.begin() aquí.
// ---------------------------------------------------------

int readRegister(
    uint8_t deviceAddress,
    uint8_t registerAddress,
    uint8_t *data,
    uint8_t length
)
{
    if (
        data == nullptr ||
        length == 0
    )
    {
        return -1;
    }


    Wire.beginTransmission(
        deviceAddress
    );

    Wire.write(
        registerAddress
    );


    if (
        Wire.endTransmission() != 0
    )
    {
        return -1;
    }


    const size_t received =
        Wire.requestFrom(
            deviceAddress,
            length
        );


    if (received != length)
    {
        return -1;
    }


    for (
        uint8_t i = 0;
        i < length;
        ++i
    )
    {
        if (!Wire.available())
        {
            return -1;
        }

        data[i] =
            static_cast<uint8_t>(
                Wire.read()
            );
    }


    return 0;
}


// ---------------------------------------------------------
// Escritura I2C usando el Wire compartido.
// ---------------------------------------------------------

int writeRegister(
    uint8_t deviceAddress,
    uint8_t registerAddress,
    uint8_t *data,
    uint8_t length
)
{
    if (
        data == nullptr ||
        length == 0
    )
    {
        return -1;
    }


    Wire.beginTransmission(
        deviceAddress
    );

    Wire.write(
        registerAddress
    );

    Wire.write(
        data,
        length
    );


    return
        Wire.endTransmission() == 0
            ? 0
            : -1;
}

} // namespace


AsterBatteryClass AsterBattery;


// ---------------------------------------------------------
// Inicialización
// ---------------------------------------------------------

bool AsterBatteryClass::begin()
{
    if (initialized)
    {
        return true;
    }


    Serial.println();
    Serial.println(
        "[AsterBattery] Inicializando AXP2101..."
    );


    const bool ok =
        power.begin(
            AXP2101_SLAVE_ADDRESS,
            readRegister,
            writeRegister
        );


    if (!ok)
    {
        Serial.println(
            "[AsterBattery] ERROR: AXP2101 no detectado."
        );

        return false;
    }


    // Solo activamos telemetría.
    // No modificamos raíles, tensiones ni corriente de carga.

    power.enableBattDetection();

    power.enableBattVoltageMeasure();

    power.enableVbusVoltageMeasure();

    power.enableSystemVoltageMeasure();


    initialized = true;


    Serial.printf(
        "[AsterBattery] AXP2101 detectado. Chip ID: 0x%02X\n",
        power.getChipID()
    );


    printStatus();


    return true;
}


// ---------------------------------------------------------
// Lectura del estado actual
// ---------------------------------------------------------

bool AsterBatteryClass::read(
    AsterBatteryStatus &status
)
{
    status =
        AsterBatteryStatus{};


    if (!initialized)
    {
        return false;
    }


    status.batteryConnected =
        power.isBatteryConnect();

    status.charging =
        power.isCharging();

    status.vbusConnected =
        power.isVbusIn();

    status.batteryVoltageMv =
        power.getBattVoltage();

    status.vbusVoltageMv =
        power.getVbusVoltage();


    if (status.batteryConnected)
    {
        status.percent =
            power.getBatteryPercent();
    }


    status.valid = true;


    return true;
}


// ---------------------------------------------------------
// Diagnóstico Serial
// ---------------------------------------------------------

void AsterBatteryClass::printStatus()
{
    AsterBatteryStatus status;


    if (!read(status))
    {
        Serial.println(
            "[AsterBattery] Lectura no disponible."
        );

        return;
    }


    Serial.println(
        "[AsterBattery] ========================"
    );

    Serial.printf(
        "[AsterBattery] Batería conectada: %s\n",
        status.batteryConnected
            ? "SI"
            : "NO"
    );

    Serial.printf(
        "[AsterBattery] Voltaje batería: %u mV\n",
        static_cast<unsigned>(
            status.batteryVoltageMv
        )
    );


    if (status.batteryConnected)
    {
        Serial.printf(
            "[AsterBattery] Nivel: %u %%\n",
            static_cast<unsigned>(
                status.percent
            )
        );
    }


    Serial.printf(
        "[AsterBattery] Cargando: %s\n",
        status.charging
            ? "SI"
            : "NO"
    );

    Serial.printf(
        "[AsterBattery] USB/VBUS: %s\n",
        status.vbusConnected
            ? "SI"
            : "NO"
    );

    Serial.printf(
        "[AsterBattery] VBUS: %u mV\n",
        static_cast<unsigned>(
            status.vbusVoltageMv
        )
    );

    Serial.println(
        "[AsterBattery] ========================"
    );
}
