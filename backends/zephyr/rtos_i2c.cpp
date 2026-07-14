// Zephyr stub for RtosI2CBus / RtosI2CDevice.
// Real Zephyr I2C support is not yet implemented; all operations return false.

#include "RtosI2C.hpp"

bool RtosI2CBus::init(const Config&) { return false; }
void RtosI2CBus::deinit() {}

bool RtosI2CDevice::init(RtosI2CBus&, const Config&) { return false; }
void RtosI2CDevice::deinit() {}
bool RtosI2CDevice::write(const uint8_t*, size_t) { return false; }
bool RtosI2CDevice::read(uint8_t*, size_t) { return false; }
bool RtosI2CDevice::write_read(const uint8_t*, size_t, uint8_t*, size_t) { return false; }
