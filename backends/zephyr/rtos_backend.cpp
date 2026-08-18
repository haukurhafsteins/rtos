#include "rtos/Gpio.hpp"
#include "rtos/Queue.hpp"

#include <cstdint>
#include <new>
#include <utility>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

namespace rtos::gpio
{
namespace
{
constexpr int logicalPinId(int port, int pin)
{
    return (port << 5) | pin;
}

struct MapEntry
{
    int logical_id;
    const gpio_dt_spec* spec;
};

const MapEntry* findPin(int logicalId)
{
    static const gpio_dt_spec touch1 =
        GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
    static const gpio_dt_spec touch2 =
        GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
    static const gpio_dt_spec imu_int1 =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), imu_int1_gpios);
    static const gpio_dt_spec imu_int2 =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), imu_int2_gpios);
    static const gpio_dt_spec pmic_int =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pmic_int_gpios);
    static const gpio_dt_spec battery_alert =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), bat_alrt_gpios);
    static const gpio_dt_spec motor_control =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), haptic_en_gpios);
    static const gpio_dt_spec led_control = {
        DEVICE_DT_GET(DT_NODELABEL(gpio0)), 25, GPIO_ACTIVE_HIGH};
    static const MapEntry table[] = {
        {logicalPinId(0, 7), &touch1},          // TBTN1
        {logicalPinId(0, 4), &touch2},          // TBTN2
        {logicalPinId(0, 26), &imu_int1},       // A_INT1
        {logicalPinId(0, 27), &imu_int2},       // A_INT2
        {logicalPinId(0, 28), &pmic_int},       // PMIC-INT
        {logicalPinId(0, 24), &battery_alert},  // BAT_ALRT
        {logicalPinId(1, 15), &motor_control},  // MCTRL
        {logicalPinId(0, 25), &led_control},    // LEDCTL
    };
    for (const auto& entry : table)
    {
        if (entry.logical_id == logicalId)
            return &entry;
    }
    return nullptr;
}

uint64_t nowUs()
{
    return k_ticks_to_us_near64(k_uptime_ticks());
}
} // namespace

class ZephyrImpl final : public Pin::Impl
{
public:
    explicit ZephyrImpl(const gpio_dt_spec* spec, const Config& cfg)
        : spec_(*spec)
    {
        async_.owner = this;
        configure(cfg);
        k_work_init(&async_.work, &ZephyrImpl::workHandler);
    }

    ~ZephyrImpl() override
    {
        disable_interrupt();
    }

    void reconfigure(const Config& cfg) override
    {
        configure(cfg);
    }

    bool read() const override
    {
        return gpio_pin_get_dt(&spec_) > 0;
    }

    void write(bool level) override
    {
        (void)gpio_pin_set_dt(&spec_, level ? 1 : 0);
    }

    void toggle() override
    {
        (void)gpio_pin_toggle_dt(&spec_);
    }

    void enable_interrupt(Trigger trigger) override
    {
        disable_interrupt();
        gpio_flags_t flags = GPIO_INT_DISABLE;
        switch (trigger)
        {
        case Trigger::Rising:
            flags = GPIO_INT_EDGE_RISING;
            break;
        case Trigger::Falling:
            flags = GPIO_INT_EDGE_FALLING;
            break;
        case Trigger::Both:
            flags = GPIO_INT_EDGE_BOTH;
            break;
        case Trigger::LevelHigh:
            flags = GPIO_INT_LEVEL_HIGH;
            break;
        case Trigger::LevelLow:
            flags = GPIO_INT_LEVEL_LOW;
            break;
        case Trigger::None:
            return;
        }
        if (gpio_pin_interrupt_configure_dt(&spec_, flags) != 0)
            return;
        trigger_ = trigger;
        gpio_init_callback(
            &async_.callback, &ZephyrImpl::gpioCallback, BIT(spec_.pin));
        callbackAttached_ =
            gpio_add_callback(spec_.port, &async_.callback) == 0;
    }

    void disable_interrupt() override
    {
        (void)gpio_pin_interrupt_configure_dt(&spec_, GPIO_INT_DISABLE);
        if (callbackAttached_)
        {
            (void)gpio_remove_callback(spec_.port, &async_.callback);
            callbackAttached_ = false;
        }
        trigger_ = Trigger::None;
    }

    void set_callback(Callback callback) override
    {
        callback_ = std::move(callback);
    }

    void attach_queue(rtos::Queue<Event>* queue) override
    {
        queue_ = queue;
    }

    void set_debounce_us(uint32_t microseconds) override
    {
        debounceUs_ = microseconds;
    }

    int logicalId = -1;

private:
    struct AsyncContext
    {
        gpio_callback callback{};
        k_work work{};
        ZephyrImpl* owner = nullptr;
    };

    void configure(const Config& cfg)
    {
        gpio_flags_t flags = 0;
        if (cfg.mode == Mode::Input)
            flags |= GPIO_INPUT;
        if (cfg.mode == Mode::Output)
            flags |= GPIO_OUTPUT;
        if (cfg.electrical.open_drain)
            flags |= GPIO_OPEN_DRAIN;
        if (cfg.pull == Pull::Up)
            flags |= GPIO_PULL_UP;
        if (cfg.pull == Pull::Down)
            flags |= GPIO_PULL_DOWN;
        (void)gpio_pin_configure_dt(&spec_, flags);
    }

    static void gpioCallback(
        const device*, gpio_callback* callback, gpio_port_pins_t pins)
    {
        (void)pins;
        auto* context = CONTAINER_OF(callback, AsyncContext, callback);
        auto* self = context->owner;
        const uint64_t timestamp = nowUs();
        const bool level = self->read();
        if (self->debounceUs_ != 0)
        {
            const uint64_t since = timestamp - self->lastInterruptUs_;
            if (since < self->debounceUs_)
                return;
            self->lastInterruptUs_ = timestamp;
        }
        ++self->interruptCount_;
        const Trigger trigger = self->trigger_ == Trigger::Both
            ? (level ? Trigger::Rising : Trigger::Falling)
            : self->trigger_;
        self->last_event_ = Event{
            self->logicalId,
            trigger,
            level,
            self->interruptCount_,
            timestamp,
        };
        if (self->queue_)
            (void)self->queue_->send_isr(self->last_event_);
        (void)k_work_submit(&self->async_.work);
    }

    static void workHandler(k_work* work)
    {
        auto* context = CONTAINER_OF(work, AsyncContext, work);
        if (context->owner->callback_)
            context->owner->callback_(context->owner->last_event_);
    }

    gpio_dt_spec spec_{};
    Callback callback_{};
    rtos::Queue<Event>* queue_ = nullptr;
    AsyncContext async_{};
    uint32_t debounceUs_ = 0;
    uint64_t lastInterruptUs_ = 0;
    uint32_t interruptCount_ = 0;
    Trigger trigger_ = Trigger::None;
    bool callbackAttached_ = false;
    Event last_event_{};
};

Pin Pin::make(int pinId, const Config& cfg)
{
    Pin pin;
    const auto* entry = findPin(pinId);
    if (entry == nullptr || !gpio_is_ready_dt(entry->spec))
        return pin;

    auto* impl = new (std::nothrow) ZephyrImpl(entry->spec, cfg);
    if (impl == nullptr)
        return pin;
    pin.impl_ = impl;
    pin.id_ = pinId;
    pin.cfg_ = cfg;
    impl->logicalId = pinId;
    return pin;
}

void Pin::reconfigure(const Config& cfg)
{
    cfg_ = cfg;
    static_cast<ZephyrImpl*>(impl_)->reconfigure(cfg);
}

bool Pin::read() const
{
    return static_cast<ZephyrImpl*>(impl_)->read();
}

void Pin::write(bool level)
{
    static_cast<ZephyrImpl*>(impl_)->write(level);
}

void Pin::toggle()
{
    static_cast<ZephyrImpl*>(impl_)->toggle();
}

void Pin::enable_interrupt(Trigger trigger)
{
    static_cast<ZephyrImpl*>(impl_)->enable_interrupt(trigger);
}

void Pin::disable_interrupt()
{
    static_cast<ZephyrImpl*>(impl_)->disable_interrupt();
}

void Pin::set_callback(Callback callback)
{
    static_cast<ZephyrImpl*>(impl_)->set_callback(std::move(callback));
}

void Pin::attach_queue(rtos::Queue<Event>* queue)
{
    static_cast<ZephyrImpl*>(impl_)->attach_queue(queue);
}

void Pin::set_debounce_us(uint32_t microseconds)
{
    static_cast<ZephyrImpl*>(impl_)->set_debounce_us(microseconds);
}

} // namespace rtos::gpio

#include "rtos/backend.hpp"

// Platform bring-up: Zephyr's kernel and devicetree-driven drivers are
// initialized before main() runs, so there is nothing to install here.
// Present so portable app code can unconditionally call rtos::backend::init().
bool rtos::backend::init() noexcept { return true; }
