#pragma once

#include <cstdint>
#include <functional>
#include <chrono>

namespace rtos
{
    namespace gpio
    {

        // Direction / Mode
        enum class Mode : uint8_t
        {
            Input,
            Output,
            Alternate, // platform-defined alternate function
            Analog     // no digital buff / high-Z analog
        };

        // Pull resistors
        enum class Pull : uint8_t
        {
            None,
            Up,
            Down
        };

        // Drive strength — coarse, mapped per-platform
        enum class Drive : uint8_t
        {
            Default,
            Low,
            Medium,
            High
        };

        // Active level semantics for convenience APIs
        enum class Active : uint8_t
        {
            High,
            Low
        };

        // Interrupt trigger types (portable superset)
        enum class Trigger : uint8_t
        {
            None,
            Rising,
            Falling,
            Both,
            LevelHigh,
            LevelLow
        };

        // Open-drain / open-source hints
        struct Electrical
        {
            bool open_drain = false;
            bool open_source = false; // rarely used; map to platform if supported
        };

        // Initial configuration for a GPIO
        struct Config
        {
            Mode mode = Mode::Input;
            Pull pull = Pull::None;
            Drive drive = Drive::Default;
            Electrical electrical = {};
            Active active = Active::High; // used by write_active/read_active helpers
            uint8_t alt_function = 0;     // optional mux index if supported
        };

        // Event delivered from ISR->task via callback or queue
        struct Event
        {
            int pin_id;            // logical pin id in your board map
            Trigger trigger;       // which edge/level
            bool level;            // sampled digital level
            uint32_t isr_count;    // monotonically increasing for this pin
            uint64_t timestamp_us; // best-effort time (monotonic)
        };

        // Forward declaration to avoid dragging queue headers here
        template <typename T>
        class RtosQueue;

        // Callback type (called in TASK CONTEXT — backends will de-bounce and defer)
        using Callback = std::function<void(const Event &)>;

        class Pin
        {
        public:
            // Implementation interface is public so backends can derive from it
            struct Impl
            {
                virtual ~Impl() = default;
                virtual void reconfigure(const Config &) = 0;
                virtual bool read() const = 0;
                virtual void write(bool) = 0;
                virtual void toggle() = 0;
                virtual void enable_interrupt(Trigger) = 0;
                virtual void disable_interrupt() = 0;
                virtual void set_callback(Callback) = 0;
                virtual void attach_queue(RtosQueue<Event> *) = 0;
                virtual void set_debounce_us(uint32_t) = 0;
            };

            Pin() = default;
            // Non-copyable; movable
            Pin(Pin &&) noexcept;
            Pin &operator=(Pin &&) noexcept;
            Pin(const Pin &) = delete;
            Pin &operator=(const Pin &) = delete;
            ~Pin();

            // Factory — maps a logical pin id (board-specific) to the platform pin
            static Pin make(int pin_id, const Config &cfg = {});

            // Reconfigure at runtime (safe to call while enabled)
            void reconfigure(const Config &cfg);

            // Basic IO
            bool read() const;
            void write(bool level);
            void toggle();

            // Convenience honoring Active semantics
            bool read_active() const { return (read() ^ (cfg_.active == Active::Low)); }
            void write_active(bool asserted) { write(asserted ^ (cfg_.active == Active::Low)); }

            // Interrupts
            void enable_interrupt(Trigger trig);
            void disable_interrupt();

            // Deliver events using one of these:
            void set_callback(Callback cb);
            void attach_queue(RtosQueue<Event> *queue);

            // Optional soft debounce (in microseconds). 0 disables.
            void set_debounce_us(uint32_t us);

            // Introspection
            int id() const { return id_; }
            Config config() const { return cfg_; }

        private:
            Impl *impl_ = nullptr; // pImpl keeps header portable
            int id_ = -1;
            Config cfg_{};
        };

    } // namespace gpio
} // namespace rtos