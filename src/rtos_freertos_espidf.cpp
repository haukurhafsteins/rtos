// rtos_freertos_time.cpp
#include "time.hpp"
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
}

namespace rtos::time
{

    HighResClock::time_point HighResClock::now() noexcept
    {
        return HighResClock::time_point(Micros(esp_timer_get_time()));
    }

    void sleep_for(Millis ms) noexcept { vTaskDelay(pdMS_TO_TICKS(ms.count())); }

    void sleep_until(HighResClock::time_point tp) noexcept
    {
        for (;;)
        {
            auto now = HighResClock::now();
            if (now >= tp)
                break;
            auto remaining = std::chrono::duration_cast<Millis>(tp - now);
            // vTaskDelay(0) yields; delay at least 1 tick to actually sleep
            TickType_t ticks = remaining.count() <= 0 ? 0 : pdMS_TO_TICKS(remaining.count());
            if (ticks == 0)
            {
                taskYIELD();
                break;
            }
            vTaskDelay(ticks);
        }
    }

} // namespace rtos::time
