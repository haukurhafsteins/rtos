// Zephyr stub for I2CBus / I2CDevice.
// Real Zephyr I2C support is not yet implemented; all operations return false.

#include "rtos/I2C.hpp"

using namespace rtos;

bool I2CBus::init(const Config&) { return false; }
void I2CBus::deinit() {}

bool I2CDevice::init(I2CBus&, const Config&) { return false; }
void I2CDevice::deinit() {}
bool I2CDevice::write(const uint8_t*, size_t) { return false; }
bool I2CDevice::read(uint8_t*, size_t) { return false; }
bool I2CDevice::write_read(const uint8_t*, size_t, uint8_t*, size_t) { return false; }
