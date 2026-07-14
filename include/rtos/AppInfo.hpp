#pragma once

#include <cstddef>
#include <cstdint>

namespace rtos
{
    // Read-only information about the running application and the device it
    // runs on: firmware build description, chip details, and the factory MAC
    // address.
    //
    // The interface is defined here; each backend implements it in its
    // src/rtos_*.cpp file (ESP-IDF fills it from esp_app_get_description(),
    // esp_chip_info() and the eFuse MAC; the Linux host backend returns
    // compile-time fallbacks).
    class AppInfo
    {
    public:
        // Firmware build description
        struct Description
        {
            char projectName[32]; // e.g. "uru-device"
            char version[32];     // e.g. "0.10.57"
            char buildDate[16];   // e.g. "Jul 14 2026"
            char buildTime[16];   // e.g. "12:34:56"
            char sdkVersion[32];  // e.g. "v5.2.1" (ESP-IDF version)
        };

        // Chip / SoC details
        struct Chip
        {
            char model[24];    // e.g. "esp32s3"
            uint16_t revision; // silicon revision
            uint8_t cores;     // number of CPU cores
            bool wifi;
            bool bluetoothLe;
            bool bluetoothClassic;
            bool ieee802154;
            bool embeddedFlash;
            bool embeddedPsram;
        };

        static constexpr std::size_t MacSize = 6;

        // Firmware build description. Read once and cached.
        static const Description &description();

        // Chip details. Read once and cached.
        static const Chip &chip();

        // Factory-default MAC address. Returns false if the backend cannot
        // provide one (mac is zero-filled in that case).
        static bool macAddress(uint8_t (&mac)[MacSize]);
    };
} // namespace rtos
