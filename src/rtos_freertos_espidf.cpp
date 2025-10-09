#include <cstdio>
#include <cstring>

#include "backend.hpp"
#include "time.hpp"
#include "rtos_assert.hpp"
#include "RtosLog.hpp"
#include "RtosLogSinks.hpp"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"
}

#include "esp_timer.h"
#include <esp_log.h>

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

using rtos::time::Millis;

namespace
{
    //-----------------------------------------------------------------------------
    // Tasks
    //-----------------------------------------------------------------------------
    // FreeRTOS expects stack depth in words (StackType_t), not bytes.
    constexpr uint32_t bytes_to_stack_depth(uint32_t bytes)
    {
        return (bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    }

} // namespace

namespace rtos::backend
{
    bool task_create(TaskHandle &out_handle,
                     const char *name,
                     uint32_t stack_size_bytes,
                     uint32_t priority,
                     TaskFunction func,
                     void *arg) noexcept
    {
        TaskHandle_t native = nullptr;
        const uint32_t depth = bytes_to_stack_depth(stack_size_bytes);
        BaseType_t res = xTaskCreate(func, name, depth, arg, priority, &native);
        if (res != pdPASS)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<TaskHandle>(native);
        return true;
    }

    void task_delete(TaskHandle handle) noexcept
    {
        if (handle)
        {
            vTaskDelete(static_cast<TaskHandle_t>(handle));
        }
    }

    void delay_ms(Millis duration) noexcept { vTaskDelay(pdMS_TO_TICKS(duration.count())); }
    void yield() noexcept { taskYIELD(); }

    TaskHandle current_task() noexcept
    {
        return static_cast<TaskHandle>(xTaskGetCurrentTaskHandle());
    }

    //-----------------------------------------------------------------------------
    // Time
    //-----------------------------------------------------------------------------
    inline TickType_t to_ticks(Millis timeout_ms)
    {
        if (timeout_ms == Millis::max())
        {
            return portMAX_DELAY;
        }
        return pdMS_TO_TICKS(timeout_ms.count());
    }

    //-----------------------------------------------------------------------------
    // Queues
    //-----------------------------------------------------------------------------
    bool queue_create(QueueHandle &out_handle, std::size_t length, std::size_t item_size) noexcept
    {
        QueueHandle_t native = xQueueCreate(static_cast<UBaseType_t>(length),
                                            static_cast<UBaseType_t>(item_size));
        if (!native)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<QueueHandle>(native);
        return true;
    }

    void queue_delete(QueueHandle handle) noexcept
    {
        if (handle)
        {
            vQueueDelete(static_cast<QueueHandle_t>(handle));
        }
    }

    bool queue_send(QueueHandle handle, const void *item, Millis timeout_ms) noexcept
    {
        return xQueueSend(static_cast<QueueHandle_t>(handle),
                          item,
                          to_ticks(timeout_ms)) == pdTRUE;
    }

    bool queue_receive(QueueHandle handle, void *out_item, Millis timeout_ms) noexcept
    {
        return xQueueReceive(static_cast<QueueHandle_t>(handle),
                             out_item,
                             to_ticks(timeout_ms)) == pdTRUE;
    }

    bool queue_send_isr(QueueHandle handle, const void *item, bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        BaseType_t ok = xQueueSendFromISR(static_cast<QueueHandle_t>(handle),
                                          item,
                                          &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return ok == pdTRUE;
    }

    bool queue_receive_isr(QueueHandle handle, void *out_item, bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        BaseType_t ok = xQueueReceiveFromISR(static_cast<QueueHandle_t>(handle),
                                             out_item,
                                             &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return ok == pdTRUE;
    }

    std::size_t queue_count(QueueHandle handle) noexcept
    {
        return static_cast<std::size_t>(uxQueueMessagesWaiting(static_cast<QueueHandle_t>(handle)));
    }

    std::size_t queue_spaces(QueueHandle handle) noexcept
    {
        return static_cast<std::size_t>(uxQueueSpacesAvailable(static_cast<QueueHandle_t>(handle)));
    }

    bool queue_reset(QueueHandle handle) noexcept
    {
        return xQueueReset(static_cast<QueueHandle_t>(handle)) == pdPASS;
    }

    //-----------------------------------------------------------------------------
    // Message Buffer
    //-----------------------------------------------------------------------------
    bool msgbuf_create(MsgBufferHandle &out_handle, std::size_t capacity_bytes) noexcept
    {
        MessageBufferHandle_t h = xMessageBufferCreate(capacity_bytes);
        if (!h)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<MsgBufferHandle>(h);
        return true;
    }

    void msgbuf_delete(MsgBufferHandle handle) noexcept
    {
        if (handle)
            vMessageBufferDelete(static_cast<MessageBufferHandle_t>(handle));
    }

    std::size_t msgbuf_send(MsgBufferHandle handle,
                            const void *data, std::size_t bytes,
                            Millis timeout_ms) noexcept
    {
        return xMessageBufferSend(static_cast<MessageBufferHandle_t>(handle),
                                  data, bytes, to_ticks(timeout_ms));
    }

    std::size_t msgbuf_receive(MsgBufferHandle handle,
                               void *out, std::size_t max_bytes,
                               Millis timeout_ms) noexcept
    {
        return xMessageBufferReceive(static_cast<MessageBufferHandle_t>(handle),
                                     out, max_bytes, to_ticks(timeout_ms));
    }

    std::size_t msgbuf_send_isr(MsgBufferHandle handle,
                                const void *data, std::size_t bytes,
                                bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        std::size_t sent = xMessageBufferSendFromISR(static_cast<MessageBufferHandle_t>(handle),
                                                     data, bytes, &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return sent;
    }

    std::size_t msgbuf_receive_isr(MsgBufferHandle handle,
                                   void *out, std::size_t max_bytes,
                                   bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        std::size_t recvd = xMessageBufferReceiveFromISR(static_cast<MessageBufferHandle_t>(handle),
                                                         out, max_bytes, &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return recvd;
    }

    std::size_t msgbuf_next_len(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferNextLengthBytes(static_cast<MessageBufferHandle_t>(handle));
    }

    std::size_t msgbuf_space_available(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferSpaceAvailable(static_cast<MessageBufferHandle_t>(handle));
    }

    bool msgbuf_reset(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferReset(static_cast<MessageBufferHandle_t>(handle)) == pdPASS;
    }

    //-----------------------------------------------------------------------------
    // Asserts
    //-----------------------------------------------------------------------------
    [[noreturn]] void assert_fail(const char * /*expr*/,
                                  const char * /*file*/,
                                  int /*line*/,
                                  const char * /*func*/) noexcept
    {
        // If you want file/line logging, add your logger/ITM/SEGGER print here.
        // Then route to FreeRTOS's assert path:
        configASSERT(0);
        // configASSERT is noreturn in practice; just in case:
        while (true)
        {
            taskYIELD();
        }
    }
} // namespace rtos::backend

namespace rtos::time
{
    //-----------------------------------------------------------------------------
    // Time
    //-----------------------------------------------------------------------------
    HighResClock::time_point HighResClock::now() noexcept
    {
        return HighResClock::time_point(Micros(esp_timer_get_time()));
    }

    Millis now_us() noexcept
    {
        return Millis(esp_timer_get_time());
    }

    Millis now_ms() noexcept
    {
        return Millis(esp_timer_get_time() / 1000);
    }

    Seconds now_s() noexcept
    {
        return Seconds(esp_timer_get_time() / 1000000);
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

using namespace rtos;

namespace
{
    struct TagRule
    {
        const char *tag;
        LogLevel level;
    };

    IRtosLogSink *s_sinks[RTOS_LOG_MAX_SINKS] = {};
    size_t s_sinkCount = 0;

    LogLevel s_globalLevel = LogLevel::Info;
    TagRule s_rules[RTOS_LOG_MAX_TAG_RULES] = {};
    size_t s_ruleCount = 0;

    RtosLog::TimestampFn s_ts = nullptr;

#if RTOS_LOG_SHOW_FILELINE
    // Extract basename from __FILE__ strings stored in flash/ROM
    static const char *baseName(const char *path)
    {
        if (!path)
            return "";
        const char *s1 = std::strrchr(path, '/');
        const char *s2 = std::strrchr(path, '\\');
        const char *b = s1 > s2 ? s1 : s2; // whichever is last
        return b ? (b + 1) : path;
    }
#endif
}

// ---- RtosLog impl ----
void RtosLog::addSink(IRtosLogSink &sink)
{
    lock();
    if (s_sinkCount < RTOS_LOG_MAX_SINKS)
        s_sinks[s_sinkCount++] = &sink;
    unlock();
}

void RtosLog::clearSinks()
{
    lock();
    s_sinkCount = 0;
    std::memset(s_sinks, 0, sizeof(s_sinks));
    unlock();
}

void RtosLog::setGlobalLevel(LogLevel lvl) { s_globalLevel = lvl; }
LogLevel RtosLog::getGlobalLevel() { return s_globalLevel; }

void RtosLog::setTagLevel(const char *tag, LogLevel lvl)
{
    if (!tag)
        return;
    lock();
    // Replace existing rule if present
    for (size_t i = 0; i < s_ruleCount; ++i)
    {
        if (s_rules[i].tag == tag || (s_rules[i].tag && tag && std::strcmp(s_rules[i].tag, tag) == 0))
        {
            s_rules[i].level = lvl;
            unlock();
            return;
        }
    }
    if (s_ruleCount < RTOS_LOG_MAX_TAG_RULES)
    {
        s_rules[s_ruleCount++] = TagRule{tag, lvl};
    }
    unlock();
}

LogLevel RtosLog::getTagLevel(const char *tag)
{
    if (!tag)
        return LogLevel::None;
    for (size_t i = 0; i < s_ruleCount; ++i)
    {
        if (s_rules[i].tag == tag || (s_rules[i].tag && tag && std::strcmp(s_rules[i].tag, tag) == 0))
            return s_rules[i].level;
    }
    return LogLevel::None;
}

void RtosLog::setTimestampProvider(TimestampFn fn) { s_ts = fn; }

char RtosLog::levelChar(LogLevel lvl)
{
    switch (lvl)
    {
    case LogLevel::Error:
        return 'E';
    case LogLevel::Warn:
        return 'W';
    case LogLevel::Info:
        return 'I';
    case LogLevel::Debug:
        return 'D';
    case LogLevel::Verbose:
        return 'V';
    default:
        return '-';
    }
}

bool RtosLog::shouldEmit(LogLevel level, const char *tag)
{
    // Runtime gates
    LogLevel tagLvl = getTagLevel(tag);
    LogLevel gate = (tagLvl != LogLevel::None) ? tagLvl : s_globalLevel;
    return static_cast<int>(level) <= static_cast<int>(gate);
}

void RtosLog::lock()
{
    if (xPortInIsrContext()) {
        portENTER_CRITICAL_ISR(&s_mux);
    } else {
        portENTER_CRITICAL(&s_mux);
    }
}

void RtosLog::unlock()
{
    if (xPortInIsrContext()) {
        portEXIT_CRITICAL_ISR(&s_mux);
    } else {
        portEXIT_CRITICAL(&s_mux);
    }
}

void RtosLog::vlog(LogLevel level, const char *tag, const char *msg, va_list ap)
{
    switch (level)
    {
    case rtos::LogLevel::Error:
        ESP_LOGE(tag, "%s", msg);
        break;
    case rtos::LogLevel::Warn:
        ESP_LOGW(tag, "%s", msg);
        break;
    case rtos::LogLevel::Info:
        ESP_LOGI(tag, "%s", msg);
        break;
    case rtos::LogLevel::Debug:
        ESP_LOGD(tag, "%s", msg);
        break;
    case rtos::LogLevel::Verbose:
        ESP_LOGV(tag, "%s", msg);
        break;
    default:
        break;
    }

//     if (!shouldEmit(level, tag))
//         return;

//     // Format the message body
//     char body[RTOS_LOG_LINE_MAX];
//     int n = std::vsnprintf(body, sizeof(body), msg ? msg : "", ap);
//     if (n < 0)
//         return; // formatting error

//     // Compose final line with prefix
//     char line[RTOS_LOG_LINE_MAX];

// #if RTOS_LOG_SHOW_FILELINE
//     // Detect presence of FILE and LINE via GNU extensions: not available here; caller may bake in if desired
// #endif

//     const char *t = tag ? tag : "rtos";

// #if RTOS_LOG_SHOW_TIME
//     int m = std::snprintf(line, sizeof(line), "[%llu] %c/%s: %s", rtos::time::now_ms().count(), levelChar(level), t, body);
// #else
//     int m = std::snprintf(line, sizeof(line), "%c/%s: %s", levelChar(level), t, body);
// #endif
//     if (m < 0)
//         return;
//     size_t lineLen = (m < (int)sizeof(line)) ? (size_t)m : sizeof(line) - 1;

//     // Write to sinks
//     lock();
//     for (size_t i = 0; i < s_sinkCount; ++i)
//     {
//         auto *s = s_sinks[i];
//         if (s && s->enabled(level))
//             s->write(level, t, line, lineLen);
//     }
//     unlock();
}

void RtosLog::log(LogLevel level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog(level, tag, fmt, ap);
    va_end(ap);
}

using namespace rtos;

void StdoutLogSink::write(LogLevel, const char *, const char *line, size_t)
{
    // We keep FILE* out of header to reduce includes
    std::fputs(line, (FILE *)(_stream ? _stream : stdout));
    std::fputc('\n', (FILE *)(_stream ? _stream : stdout));
}

void EspIdfLogSink::write(LogLevel level, const char *tag, const char *msg, size_t)
{
    switch (level)
    {
    case rtos::LogLevel::Error:
        ESP_LOGE(tag, "%s", msg);
        break;
    case rtos::LogLevel::Warn:
        ESP_LOGW(tag, "%s", msg);
        break;
    case rtos::LogLevel::Info:
        ESP_LOGI(tag, "%s", msg);
        break;
    case rtos::LogLevel::Debug:
        ESP_LOGD(tag, "%s", msg);
        break;
    case rtos::LogLevel::Verbose:
        ESP_LOGV(tag, "%s", msg);
        break;
    default:
        break;
    }
}
