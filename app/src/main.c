#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(l5_task1, LOG_LEVEL_INF);

/*
 * Producer -> queue -> consumer
 */
#define QUEUE_SIZE              8
#define PRODUCER_INTERVAL_MS    100
#define CONSUMER_INTERVAL_MS    150

/*
 * Deliberately longer than the watchdog timeout.
 */
#define STUCK_SLEEP_MS          3000

/*
 * Consumer must feed before this expires.
 */
#define WATCHDOG_TIMEOUT_MS     1000

/*
 * Queue warning threshold:
 * 75% of 8 entries = 6 entries.
 */
#define QUEUE_WARNING_LEVEL     ((QUEUE_SIZE * 75) / 100)

struct message {
	int sequence;
	uint32_t timestamp;
};

K_MSGQ_DEFINE(message_queue,
	      sizeof(struct message),
	      QUEUE_SIZE,
	      4);

static int consumer_wdt_channel = -1;
static bool stuck_simulated;

/*
 * Task watchdog callback.
 *
 * A non-NULL callback is used so that the demo logs the watchdog
 * event instead of requesting an immediate system reset.
 */
static void watchdog_callback(int channel_id, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_ERR("[WATCHDOG] Consumer timeout! channel=%d",
		channel_id);
}

/*
 * Producer thread.
 */
static void producer_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int sequence = 0;

	while (1) {
		struct message msg = {
			.sequence = sequence++,
			.timestamp = k_uptime_get_32(),
		};

		int ret = k_msgq_put(&message_queue, &msg, K_MSEC(50));

		if (ret == 0) {
			LOG_INF("[PRODUCER] queued seq=%d queue=%u/%u",
				msg.sequence,
				k_msgq_num_used_get(&message_queue),
				QUEUE_SIZE);
		} else {
			LOG_WRN("[PRODUCER] queue full, seq=%d dropped",
				msg.sequence);
		}

		k_msleep(PRODUCER_INTERVAL_MS);
	}
}

/*
 * Consumer thread.
 *
 * The first few messages are processed normally.
 * After that, one deliberately long k_sleep() simulates
 * a stuck consumer. During that sleep the watchdog is not fed.
 */
static void consumer_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct message msg;

	consumer_wdt_channel =
		task_wdt_add(WATCHDOG_TIMEOUT_MS,
			     watchdog_callback,
			     NULL);

	if (consumer_wdt_channel < 0) {
		LOG_ERR("[CONSUMER] failed to add watchdog: %d",
			consumer_wdt_channel);
		return;
	}

	LOG_INF("[CONSUMER] watchdog channel=%d timeout=%dms",
		consumer_wdt_channel,
		WATCHDOG_TIMEOUT_MS);

	int processed = 0;

	while (1) {
		int ret = k_msgq_get(&message_queue,
				     &msg,
				     K_FOREVER);

		if (ret != 0) {
			LOG_ERR("[CONSUMER] queue receive failed: %d", ret);
			continue;
		}

		LOG_INF("[CONSUMER] processing seq=%d queue=%u/%u",
			msg.sequence,
			k_msgq_num_used_get(&message_queue),
			QUEUE_SIZE);

		/*
		 * Simulate a stuck consumer once.
		 */
		if (!stuck_simulated && processed >= 2) {
			stuck_simulated = true;

			LOG_WRN("[CONSUMER] SIMULATING STUCK TASK");
			LOG_WRN("[CONSUMER] sleeping for %dms; watchdog will expire",
				STUCK_SLEEP_MS);

			/*
			 * Intentionally longer than WATCHDOG_TIMEOUT_MS.
			 * No watchdog feed happens during this sleep.
			 */
			k_msleep(STUCK_SLEEP_MS);

			LOG_WRN("[CONSUMER] recovered from simulated stall");
		}

		ret = task_wdt_feed(consumer_wdt_channel);

		if (ret != 0) {
			LOG_ERR("[CONSUMER] watchdog feed failed: %d", ret);
		} else {
			LOG_INF("[CONSUMER] watchdog fed");
		}

		processed++;

		k_msleep(CONSUMER_INTERVAL_MS);
	}
}

/*
 * Health-check thread.
 *
 * Checks queue occupancy periodically and warns at >= 75%.
 */
static void health_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		uint32_t used = k_msgq_num_used_get(&message_queue);

		if (used >= QUEUE_WARNING_LEVEL) {
			LOG_WRN("[HEALTH] queue at %u/%u (%u%%)",
				used,
				QUEUE_SIZE,
				(used * 100U) / QUEUE_SIZE);
		} else {
			LOG_INF("[HEALTH] queue at %u/%u (%u%%)",
				used,
				QUEUE_SIZE,
				(used * 100U) / QUEUE_SIZE);
		}

		k_msleep(50);
	}
}

K_THREAD_STACK_DEFINE(consumer_stack, 1024);
static struct k_thread consumer_thread_obj;

K_THREAD_DEFINE(producer_thread,
		1024,
		producer_thread_fn,
		NULL,
		NULL,
		NULL,
		5,
		0,
		0);

K_THREAD_DEFINE(health_thread,
		1024,
		health_thread_fn,
		NULL,
		NULL,
		NULL,
		7,
		0,
		0);

int main(void)
{
	int ret;

	LOG_INF("=== L5 Task 1: Producer/Consumer Reliability ===");
	LOG_INF("Queue capacity: %d", QUEUE_SIZE);
	LOG_INF("Health warning: %d/%d entries",
		QUEUE_WARNING_LEVEL,
		QUEUE_SIZE);

	/*
	 * Initialize the task watchdog before starting
	 * the consumer thread.
	 */
	ret = task_wdt_init(NULL);

	if (ret != 0) {
		LOG_ERR("task_wdt_init failed: %d", ret);
		return 0;
	}

	LOG_INF("Task watchdog initialized");

	/*
	 * Start the consumer only after the watchdog
	 * subsystem has been initialized.
	 */
	k_thread_create(&consumer_thread_obj,
			consumer_stack,
			K_THREAD_STACK_SIZEOF(consumer_stack),
			consumer_thread_fn,
			NULL,
			NULL,
			NULL,
			6,
			0,
			K_NO_WAIT);

	return 0;
}