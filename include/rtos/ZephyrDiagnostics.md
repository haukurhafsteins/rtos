# Zephyr allocation and stack diagnostics

`CONFIG_RTOS_DIAGNOSTICS` is opt-in and requires the existing stack-fill/name
support on a single application CPU. Call `rtos::zephyr_diagnostics::begin()`
from main before constructing application tasks and queues. It enables
saturating counters without clearing history; it creates no task, queue, heap
allocation, or kernel hook.

`failures()` returns separate counts for unavailable task slots, failed task
creation/join, failed queue allocations, failed message-buffer allocations,
and failed pipeline-heap malloc/calloc/realloc. Counters cover these RTOS-owned
boundaries after begin only. They exclude invalid task/queue arguments,
intentional zero-size allocation/free results, and third-party allocator
callers. A calloc failure is counted once, including arithmetic overflow.
These are observed operation failures, not a complete heap-failure census.

`task_slot_count()` and `task_stack_snapshot()` read existing preallocated task
slots. Call from task context after startup; inactive or not-yet-initialized
slots return `active=false`. Stack watermarks come from Zephyr's initialized
stack fill, not sampled current stack pointers. The snapshot pins scheduling
on the single CPU while copying the name and scanning; interrupts remain
enabled during the scan. No callback or logging runs under its locks.
The ready flag is published only after creation/name assignment and cleared
on slot release. A slot's watermark resets when its stack is reused for a new
task; this is not cumulative history across task lifetimes. Main and native
workqueues are outside the RTOS slot table and must be inspected separately.

On the Nordic B241 profile the counters occupy 21 bytes before linker
alignment. Slot readiness fits existing TaskSlot padding. Applications must
measure their own final map and stack headroom, and include scan latency in
hardware validation. No libc-heap statistics API is provided here.
