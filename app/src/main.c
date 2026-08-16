#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024
#define PRIO_A 5
#define PRIO_B 5

#define ITERATIONS 1000

volatile int shared_counter = 0;

K_MUTEX_DEFINE(counter_mutex);

void thread_a_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < ITERATIONS; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        int temp = shared_counter;
        k_yield();
        shared_counter = temp + 1;

        k_mutex_unlock(&counter_mutex);
    }

    LOG_INF("Thread A finished");
}

void thread_b_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < ITERATIONS; i++) {
        k_mutex_lock(&counter_mutex, K_FOREVER);

        int temp = shared_counter;
        k_yield();
        shared_counter = temp + 1;

        k_mutex_unlock(&counter_mutex);
    }

    LOG_INF("Thread B finished");
}

K_THREAD_DEFINE(thread_a, STACK_SIZE, thread_a_fn,
                NULL, NULL, NULL, PRIO_A, 0, 0);

K_THREAD_DEFINE(thread_b, STACK_SIZE, thread_b_fn,
                NULL, NULL, NULL, PRIO_B, 0, 0);

int main(void)
{
    LOG_INF("Starting mutex-protected counter demo");

    k_msleep(1000);

    LOG_INF("Final counter = %d, expected = %d",
            shared_counter, ITERATIONS * 2);

    return 0;
}