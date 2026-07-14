// Zephyr stub for RtosSpiBus / RtosSpiDevice.
// Real Zephyr SPI support is not yet implemented; all operations return false.

#include "RtosSpi.hpp"

bool RtosSpiBus::init(const Config&) { return false; }
void RtosSpiBus::deinit() {}

bool RtosSpiDevice::init(RtosSpiBus&, const Config&) { return false; }
void RtosSpiDevice::deinit() {}
bool RtosSpiDevice::write(const uint8_t*, size_t) { return false; }
bool RtosSpiDevice::read(uint8_t*, size_t) { return false; }
bool RtosSpiDevice::transfer(const uint8_t*, uint8_t*, size_t) { return false; }
bool RtosSpiDevice::transfer_cmd(uint16_t, const uint8_t*, uint8_t*, size_t) { return false; }
