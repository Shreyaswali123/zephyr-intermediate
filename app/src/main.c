#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

#define STACK_SIZE                2048
#define CONTROL_PRIORITY            3
#define MAINTENANCE_PRIORITY        4

#define EVENT_PERIOD_MS           250
#define MAINTENANCE_LOAD_US     45000

#define CONTROL_DEADLINE_MS        10

struct control_event {
    uint32_t seq;
    uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

static uint32_t deadline_misses;

/* Timer expiry */
static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;

    struct control_event event = {
        .seq = seq++,
        .ready_ms = k_uptime_get_32(),
    };

    int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);
    if (ret != 0) {
        return;
    }

    /*
     * Trace the point at which the control event becomes ready.
     * arg0 = sequence number
     * arg1 = ready timestamp in ms
     */
    sys_trace_named_event("CONTROL_READY",
                          event.seq,
                          event.ready_ms);

    k_sem_give(&maintenance_start);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* Control thread */
static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        struct control_event event;

        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[CONTROL] receive failed: %d", ret);
            continue;
        }

        /*
         * Response time is defined as:
         *
         *   control_start_time - event_ready_time
         *
         * The requirement is that control work starts
         * within CONTROL_DEADLINE_MS after becoming ready.
         */
        uint32_t start_ms = k_uptime_get_32();
        uint32_t response_ms = start_ms - event.ready_ms;

        bool deadline_missed = response_ms > CONTROL_DEADLINE_MS;

        if (deadline_missed) {
            deadline_misses++;

            LOG_WRN_RATELIMIT(
                "[DEADLINE] seq=%u response=%u ms deadline=%u ms misses=%u",
                event.seq,
                response_ms,
                CONTROL_DEADLINE_MS,
                deadline_misses);
        }

        LOG_INF("[CONTROL] processed seq=%u response=%u ms",
                event.seq,
                response_ms);

        /*
         * Trace completion.
         * arg0 = sequence number
         * arg1 = measured response time in ms
         */
        sys_trace_named_event("CONTROL_COMPLETE",
                              event.seq,
                              response_ms);
    }
}

/* Background maintenance thread */
static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        /*
         * This work is important, but it has no short deadline.
         */
        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
                NULL, NULL, NULL, CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
                NULL, NULL, NULL, MAINTENANCE_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("=== L6 Homework: Runtime Investigation ===");
    LOG_INF("Control work must start within %u ms",
            CONTROL_DEADLINE_MS);
    LOG_INF("Inspect, measure, trace, explain, and correct the delay");

    k_timer_start(&event_timer,
                  K_MSEC(500),
                  K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
