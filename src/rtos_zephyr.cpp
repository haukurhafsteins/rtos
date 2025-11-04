#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace rtos { namespace gpio {

// Example: map logical id -> DT alias (configure to your board)
struct MapEntry { const gpio_dt_spec* spec; };

auto& pin_table() {
    // Create static specs using DT aliases; adjust for your project
    static const gpio_dt_spec btn0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
    static const gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    static const MapEntry table[] = {
        { &btn0 }, // id 0
        { &led0 }, // id 1
    };
    return *reinterpret_cast<const MapEntry (*)[2]>(&table);
}

static inline uint64_t now_us() { return k_ticks_to_us_near64(k_uptime_ticks()); }

class ZephyrImpl final : public Pin::Impl {
public:
    explicit ZephyrImpl(const gpio_dt_spec* spec, const Config& cfg)
    : spec_(*spec) {
        configure(cfg);
        // init work item for deferred callbacks
        k_work_init(&work_, &ZephyrImpl::work_handler_s);
        work_.user_data = this;
    }

    ~ZephyrImpl() override { disable_interrupt(); }

    void reconfigure(const Config& cfg) override { configure(cfg); }

    bool read() const override {
        int v; gpio_pin_get_dt(&spec_, &v); return v; }

    void write(bool level) override { gpio_pin_set_dt(&spec_, level); }

    void toggle() override { gpio_pin_toggle_dt(&spec_); }

    void enable_interrupt(Trigger trig) override {
        gpio_flags_t flags = GPIO_INT_DISABLE;
        switch (trig) {
            case Trigger::Rising: flags = GPIO_INT_EDGE_RISING; break;
            case Trigger::Falling: flags = GPIO_INT_EDGE_FALLING; break;
            case Trigger::Both: flags = GPIO_INT_EDGE_BOTH; break;
            case Trigger::LevelHigh: flags = GPIO_INT_LEVEL_HIGH; break;
            case Trigger::LevelLow: flags = GPIO_INT_LEVEL_LOW; break;
            default: flags = GPIO_INT_DISABLE; break;
        }
        gpio_pin_interrupt_configure_dt(&spec_, flags);
        cb_.handler = &ZephyrImpl::gpio_cb_s;
        gpio_add_callback(spec_.port, &cb_);
    }

    void disable_interrupt() override {
        gpio_pin_interrupt_configure_dt(&spec_, GPIO_INT_DISABLE);
        gpio_remove_callback(spec_.port, &cb_);
    }

    void set_callback(Callback cb) override { cb_own_ = std::move(cb); }

    void attach_queue(RtosQueue<Event>* q) override { queue_ = q; }

    void set_debounce_us(uint32_t us) override { debounce_us_ = us; }

    int logical_id_ = -1;

private:
    void configure(const Config& cfg) {
        cfg_ = cfg;
        gpio_flags_t flags = 0;
        if (cfg.mode == Mode::Input) flags |= GPIO_INPUT;
        if (cfg.mode == Mode::Output) flags |= GPIO_OUTPUT;
        if (cfg.electrical.open_drain) flags |= GPIO_OPEN_DRAIN;
        if (cfg.pull == Pull::Up) flags |= GPIO_PULL_UP;
        if (cfg.pull == Pull::Down) flags |= GPIO_PULL_DOWN;
        gpio_pin_configure_dt(&spec_, flags);
    }

    static void gpio_cb_s(const struct device*, struct gpio_callback* cb, uint32_t pins) {
        auto* self = CONTAINER_OF(cb, ZephyrImpl, cb_);
        const uint64_t t = now_us();
        const bool level = self->read();
        if (self->debounce_us_) {
            const uint64_t since = t - self->last_isr_us_;
            if (since < self->debounce_us_) return;
            self->last_isr_us_ = t;
        }
        ++self->isr_count_;
        self->last_event_ = Event{ self->logical_id_, Trigger::Both, level, self->isr_count_, t };
        // Defer to work queue so user callback executes in thread context
        k_work_submit(&self->work_);
        // Optional queue path via weak hook
        if (self->queue_) backend_queue_send_from_isr(self->queue_, self->last_event_);
    }

    static void work_handler_s(struct k_work* item) {
        auto* self = static_cast<ZephyrImpl*>(item->user_data);
        if (self->cb_own_) self->cb_own_(self->last_event_);
    }

    static void backend_queue_send_from_isr(RtosQueue<Event>*, const Event&) __attribute__((weak));
    static void backend_queue_send_from_isr(RtosQueue<Event>*, const Event&) {}

    gpio_dt_spec spec_{};
    Config cfg_{};
    Callback cb_own_{};
    RtosQueue<Event>* queue_ = nullptr;
    struct gpio_callback cb_{};
    struct k_work work_{};
    uint32_t debounce_us_ = 0;
    uint64_t last_isr_us_ = 0;
    uint32_t isr_count_ = 0;
    Event last_event_{};
};

// Factory for Zephyr
Pin Pin::make(int pin_id, const Config& cfg) {
    // Resolve logical id
    const auto& table = pin_table();
    // naive bounds check vs example table size (2)
    if (pin_id < 0 || pin_id >= 2) { Pin p; return p; }
    auto* impl = new ZephyrImpl(table[pin_id].spec, cfg);
    Pin p; p.impl_ = impl; p.id_ = pin_id; p.cfg_ = cfg; impl->logical_id_ = pin_id; return std::move(p);
}

void Pin::reconfigure(const Config& cfg) { cfg_ = cfg; static_cast<ZephyrImpl*>(impl_)->reconfigure(cfg); }
bool Pin::read() const { return static_cast<ZephyrImpl*>(impl_)->read(); }
void Pin::write(bool l) { static_cast<ZephyrImpl*>(impl_)->write(l); }
void Pin::toggle() { static_cast<ZephyrImpl*>(impl_)->toggle(); }
void Pin::enable_interrupt(Trigger t) { static_cast<ZephyrImpl*>(impl_)->enable_interrupt(t); }
void Pin::disable_interrupt() { static_cast<ZephyrImpl*>(impl_)->disable_interrupt(); }
void Pin::set_callback(Callback cb) { static_cast<ZephyrImpl*>(impl_)->set_callback(std::move(cb)); }
void Pin::attach_queue(RtosQueue<Event>* q) { static_cast<ZephyrImpl*>(impl_)->attach_queue(q); }
void Pin::set_debounce_us(uint32_t us) { static_cast<ZephyrImpl*>(impl_)->set_debounce_us(us); }

} }
