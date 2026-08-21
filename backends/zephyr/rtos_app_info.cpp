#include "rtos/AppInfo.hpp"

#include <cstdio>
#include <cstring>

#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/version.h>

namespace rtos
{
const AppInfo::Description& AppInfo::description()
{
    static const Description description = [] {
        Description value{};
        std::snprintf(
            value.projectName, sizeof(value.projectName), "%s", CONFIG_BOARD);
        std::snprintf(
            value.version,
            sizeof(value.version),
            "%s",
            APP_VERSION_EXTENDED_STRING);

        mcuboot_img_header header{};
        if (boot_read_bank_header(
                FIXED_PARTITION_ID(slot0_partition),
                &header,
                sizeof(header)) == 0 &&
            header.mcuboot_version == 1)
        {
            const auto& version = header.h.v1.sem_ver;
            std::snprintf(
                value.version,
                sizeof(value.version),
                "%u.%u.%u+%u",
                static_cast<unsigned int>(version.major),
                static_cast<unsigned int>(version.minor),
                static_cast<unsigned int>(version.revision),
                static_cast<unsigned int>(version.build_num));
        }

        std::snprintf(
            value.buildDate, sizeof(value.buildDate), "%s", __DATE__);
        std::snprintf(
            value.buildTime, sizeof(value.buildTime), "%s", __TIME__);
        std::snprintf(
            value.sdkVersion,
            sizeof(value.sdkVersion),
            "%s",
            KERNEL_VERSION_STRING);
        return value;
    }();
    return description;
}

const AppInfo::Chip& AppInfo::chip()
{
    static const Chip chip = [] {
        Chip value{};
        std::snprintf(value.model, sizeof(value.model), "nRF5340");
        value.revision = 0;
        value.cores = 2;
        value.wifi = false;
        value.bluetoothLe = true;
        value.bluetoothClassic = false;
        value.ieee802154 = true;
        value.embeddedFlash = true;
        value.embeddedPsram = false;
        return value;
    }();
    return chip;
}

bool AppInfo::macAddress(uint8_t (&mac)[MacSize])
{
    std::memset(mac, 0, sizeof(mac));
    if (hwinfo_get_device_id(mac, sizeof(mac)) ==
        static_cast<ssize_t>(sizeof(mac)))
    {
        return true;
    }
    std::memset(mac, 0, sizeof(mac));
    return false;
}
}
