#include <atomic>
#include <mutex>
#include "Gpio.hpp"

namespace rtos { namespace gpio {

Pin::Pin(Pin&& o) noexcept { impl_ = o.impl_; id_ = o.id_; cfg_ = o.cfg_; o.impl_ = nullptr; }
Pin& Pin::operator=(Pin&& o) noexcept { if (this!=&o){ delete impl_; impl_=o.impl_; id_=o.id_; cfg_=o.cfg_; o.impl_=nullptr;} return *this; }
Pin::~Pin(){ delete impl_; }

} }
