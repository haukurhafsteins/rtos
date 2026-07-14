// Linux host stub for I2CBus / I2CDevice.
// Used for host-side unit tests and utilities. No real I2C hardware access;
// all operations are no-ops that return false.

#include "rtos/I2C.hpp"

using namespace rtos;

bool I2CBus::init(const Config&) { return false; }
void I2CBus::deinit() {}

bool I2CDevice::init(I2CBus&, const Config&) { return false; }
void I2CDevice::deinit() {}
bool I2CDevice::write(const uint8_t*, size_t) { return false; }
bool I2CDevice::read(uint8_t*, size_t) { return false; }
bool I2CDevice::write_read(const uint8_t*, size_t, uint8_t*, size_t) { return false; }
