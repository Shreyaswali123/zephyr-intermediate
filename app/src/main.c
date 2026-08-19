#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE        1024
#define BURST_EVENTS      5
#define BURST_INTERVAL_MS 5
#define DEBOUNCE_MS       30
#define BURST_COUNT       3

static int total_events;
static int total_processed;

/* ---------------------------------------------------------------
 * Debounce work handler
 * --------------------------------------------------------------- */

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    LOG_INF("[HANDLER] processed burst %d  tick=%u",
            total_processed,
            k_uptime_get_32());
}

/*
 * Delayable work allows us to postpone execution.
 *
 * Every new sensor event reschedules the handler 30 ms into
 * the future. Therefore, a burst of events collapses into one
 * handler execution.
 */
K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);

/* ---------------------------------------------------------------
 * Sensor simulator
 * --------------------------------------------------------------- */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int burst = 0; burst < BURST_COUNT; burst++) {

        LOG_INF("[BURST] starting burst %d  tick=%u",
                burst + 1,
                k_uptime_get_32());

        for (int i = 0; i < BURST_EVENTS; i++) {

            total_events++;

            LOG_INF("[SENSOR] event %d.%d  tick=%u",
                    burst + 1,
                    i + 1,
                    k_uptime_get_32());

            int ret = k_work_reschedule(
                &debounce_work,
                K_MSEC(DEBOUNCE_MS));

            if (ret < 0) {
                LOG_ERR("[SENSOR] reschedule failed: %d", ret);
            }

            /*
             * Rapid events within the debounce window.
             */
            k_msleep(BURST_INTERVAL_MS);
        }

        LOG_INF("[BURST] finished burst %d  tick=%u",
                burst + 1,
                k_uptime_get_32());

        /*
         * Wait long enough for the debounced handler to execute
         * before starting the next burst.
         */
        k_msleep(100);
    }

    LOG_INF("[SENSOR] all bursts produced");
    LOG_INF("[SUMMARY] sensor_events=%d  handler_runs=%d",
            total_events,
            total_processed);
}

/* ---------------------------------------------------------------
 * Sensor thread
 * --------------------------------------------------------------- */

K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_sim_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void)
{
    LOG_INF("=== L3 Homework: Workqueue Debounce ===");
    LOG_INF("Burst: %d events, %dms apart",
            BURST_EVENTS,
            BURST_INTERVAL_MS);
    LOG_INF("Debounce delay: %dms", DEBOUNCE_MS);

    /*
     * 3 bursts × roughly 125 ms + startup margin.
     */
    k_msleep(1000);

    return 0;
}