// Linux stub for rtos::Nvs / rtos::nvs.
// No persistent storage backend on host builds; all operations return false.

#include "rtos/Nvs.hpp"

using namespace rtos;

bool rtos::nvs::init() { return false; }
void rtos::nvs::deinit() {}
bool rtos::nvs::erase() { return false; }

bool Nvs::open(const char*, Mode) { return false; }
void Nvs::close() {}
bool Nvs::get_str(const char*, char*, size_t&) { return false; }
bool Nvs::get_blob(const char*, void*, size_t&) { return false; }
bool Nvs::get_u32(const char*, uint32_t&) { return false; }
bool Nvs::get_i32(const char*, int32_t&) { return false; }
bool Nvs::set_str(const char*, const char*) { return false; }
bool Nvs::set_blob(const char*, const void*, size_t) { return false; }
bool Nvs::set_u32(const char*, uint32_t) { return false; }
bool Nvs::set_i32(const char*, int32_t) { return false; }
bool Nvs::erase_key(const char*) { return false; }
bool Nvs::commit() { return false; }
