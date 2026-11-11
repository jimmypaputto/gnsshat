/*
 * Jimmy Paputto 2025
 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.h>


jp_gnss_gnss_config_t create_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 10;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.has_pulse_when_no_fix = false;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_geofencing = false;

    return config;
}

static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Returns false if the wait is stopped before the fix. */
static bool wait_for_fix(jp_gnss_hat_t* gnss)
{
    jp_gnss_navigation_t navigation;
    while (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
    {
        if (navigation.pvt.fix_status == JP_GNSS_FIX_STATUS_ACTIVE)
            return true;
    }
    return false;
}

/* Returns false if the wait is stopped first. */
static bool wait_for_epochs(jp_gnss_hat_t* gnss, int count)
{
    jp_gnss_navigation_t navigation;
    for (int i = 0; i < count; ++i)
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            return false;
    }
    return true;
}

static void measure_startups(jp_gnss_hat_t* gnss, int measurement_rate_hz)
{
    /* --- Cold start test --- */
    jp_gnss_hat_hard_reset_cold_start(gnss);
    int64_t start = get_time_ms();
    if (!wait_for_fix(gnss))
        return;

    int64_t time2fix = get_time_ms() - start;
    printf("Cold start took %lld ms\r\n", (long long)time2fix);

    /* --- Wait for satellite data collection --- */
    printf("Wait 40s to collect data for hot start\r\n");
    if (!wait_for_epochs(gnss, 40 * measurement_rate_hz))
        return;
    printf("Performing hot start\r\n");

    /* --- Hot start test --- */
    jp_gnss_hat_soft_reset_hot_start(gnss);
    start = get_time_ms();
    usleep(150000);
    if (!wait_for_fix(gnss))
        return;

    int64_t hot_time = get_time_ms() - start;
    printf("Hot start took %lld ms\r\n", (long long)hot_time);
}

static sigset_t signals;  /* SIGINT and SIGTERM */

static void* wait_for_signal(void* gnss)
{
    int received;
    sigwait(&signals, &received);
    jp_gnss_hat_shutdown(gnss);
    return NULL;
}

/* SIGINT and SIGTERM make the new thread call jp_gnss_hat_shutdown(), which
 * ends the waits in main. */
static bool signals_handler(jp_gnss_hat_t* gnss, pthread_t* thread)
{
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    /* Blocks the signals in the calling thread (main); the new thread
     * inherits the mask. Library threads block them on their own. */
    pthread_sigmask(SIG_BLOCK, &signals, NULL);

    return pthread_create(thread, NULL, wait_for_signal, gnss) == 0;
}

int main(void)
{
    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Error: Failed to create GNSS instance\r\n");
        return -1;
    }

    jp_gnss_gnss_config_t config = create_config();
    jp_gnss_hat_hard_reset_cold_start(gnss);

    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Startup failed, exit\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    pthread_t signals_interrupt;
    if (!signals_handler(gnss, &signals_interrupt))
    {
        printf("Failed to start the signal thread\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    measure_startups(gnss, config.measurement_rate_hz);

    /* Wakes sigwait when the measurement ends without a signal. */
    pthread_kill(signals_interrupt, SIGINT);
    pthread_join(signals_interrupt, NULL);

    jp_gnss_hat_destroy(gnss);

    return 0;
}
