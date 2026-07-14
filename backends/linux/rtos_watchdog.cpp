// Linux host backend for rtos::watchdog.
// Functional no-op: there is no watchdog on the host, and code under test
// that subscribes and feeds should keep working, so every operation
// succeeds.

#include "rtos/watchdog.hpp"

bool rtos::watchdog::init(const Config&) { return true; }
bool rtos::watchdog::deinit() { return true; }
bool rtos::watchdog::add() { return true; }
bool rtos::watchdog::remove() { return true; }
bool rtos::watchdog::feed() { return true; }
