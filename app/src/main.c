#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

#define STACK_SIZE       1024
#define SENSOR_INTERVAL  100
#define SAMPLE_COUNT     10

/* ---------------------------------------------------------------
 * Sensor data message
 * --------------------------------------------------------------- */

struct sensor_data {
    int sample;
    int temperature;
    int pressure;
};

/* ---------------------------------------------------------------
 * zbus channel
 * --------------------------------------------------------------- */

ZBUS_CHAN_DEFINE(sensor_chan,
                 struct sensor_data,
                 NULL,
                 NULL,
                 ZBUS_OBSERVERS(sensor_listener, sensor_subscriber),
                 ZBUS_MSG_INIT(0));

/* ---------------------------------------------------------------
 * Fast listener
 * --------------------------------------------------------------- */

static void sensor_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_data *data;

    data = zbus_chan_const_msg(chan);

    LOG_INF("[LISTENER] sample=%d temp=%d pressure=%d tick=%u",
            data->sample,
            data->temperature,
            data->pressure,
            k_uptime_get_32());
}

ZBUS_LISTENER_DEFINE(sensor_listener, sensor_listener_cb);

/* ---------------------------------------------------------------
 * Slower subscriber
 * --------------------------------------------------------------- */

ZBUS_SUBSCRIBER_DEFINE(sensor_subscriber, 8);

/* ---------------------------------------------------------------
 * Sensor publisher
 * --------------------------------------------------------------- */

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 1; i <= SAMPLE_COUNT; i++) {

        struct sensor_data data = {
            .sample = i,
            .temperature = 20 + i,
            .pressure = 1000 + (i * 2),
        };

        LOG_INF("[SENSOR] publishing sample=%d tick=%u",
                data.sample,
                k_uptime_get_32());

        int ret = zbus_chan_pub(&sensor_chan,
                                &data,
                                K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[SENSOR] publish failed: %d", ret);
        }

        k_msleep(SENSOR_INTERVAL);
    }

    LOG_INF("[SENSOR] all samples published");
}

/* ---------------------------------------------------------------
 * Subscriber thread
 * --------------------------------------------------------------- */

static void subscriber_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct zbus_channel *chan;

    while (1) {

        int ret = zbus_sub_wait(&sensor_subscriber,
                                &chan,
                                K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[SUBSCRIBER] wait failed: %d", ret);
            continue;
        }

        struct sensor_data data;

        ret = zbus_chan_read(chan,
                             &data,
                             K_NO_WAIT);

        if (ret != 0) {
            LOG_ERR("[SUBSCRIBER] read failed: %d", ret);
            continue;
        }

        LOG_INF("[SUBSCRIBER] sample=%d temp=%d pressure=%d tick=%u",
                data.sample,
                data.temperature,
                data.pressure,
                k_uptime_get_32());

        if (data.sample >= SAMPLE_COUNT) {
            break;
        }
        /*
         * Simulate slower logging.
         */
        k_msleep(150);
    }
    LOG_INF("[SUBSCRIBER] all samples processed");
}

/* ---------------------------------------------------------------
 * Threads
 * --------------------------------------------------------------- */

K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_thread_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);

K_THREAD_DEFINE(subscriber_thread,
                STACK_SIZE,
                subscriber_thread_fn,
                NULL,
                NULL,
                NULL,
                6,
                0,
                0);

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void)
{
    LOG_INF("=== L4 Homework: zbus ===");
    LOG_INF("Sensor publishes every %dms", SENSOR_INTERVAL);
    LOG_INF("Listener: fast display updates");
    LOG_INF("Subscriber: slower logging");

    k_msleep((SAMPLE_COUNT + 3) * SENSOR_INTERVAL);

    LOG_INF("=== L4 complete ===");

    return 0;
}