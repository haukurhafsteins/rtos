// Zephyr stub for SpiBus / SpiDevice.
// Real Zephyr SPI support is not yet implemented; all operations return false.

#include "rtos/Spi.hpp"

using namespace rtos;

bool SpiBus::init(const Config&) { return false; }
void SpiBus::deinit() {}

bool SpiDevice::init(SpiBus&, const Config&) { return false; }
void SpiDevice::deinit() {}
bool SpiDevice::write(const uint8_t*, size_t) { return false; }
bool SpiDevice::read(uint8_t*, size_t) { return false; }
bool SpiDevice::transfer(const uint8_t*, uint8_t*, size_t) { return false; }
bool SpiDevice::transfer_cmd(uint16_t, const uint8_t*, uint8_t*, size_t) { return false; }
