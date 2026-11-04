/*
 * Jimmy Paputto 2025
 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.h>


void print_navigation_data(const jp_gnss_navigation_t* nav)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Quality: %s\r\n",
        jp_gnss_fix_quality_to_string(nav->pvt.fix_quality));
    printf("\tFix Status: %s\r\n",
        jp_gnss_fix_status_to_string(nav->pvt.fix_status));
    printf("\tFix Type: %s\r\n",
        jp_gnss_fix_type_to_string(nav->pvt.fix_type));
    printf("\tVisible Satellites: %d\n", nav->pvt.visible_satellites);
    printf("\tLatitude: %.6f°\n", nav->pvt.latitude);
    printf("\tLongitude: %.6f°\n", nav->pvt.longitude);
    printf("\tAltitude: %.2f m\n", nav->pvt.altitude);
    printf("\tTime: %s\n", jp_gnss_utc_time_iso8601(&nav->pvt));
    printf("\tTime accuracy: %d nanoseconds\n", nav->pvt.utc.accuracy);
    printf("\tDate valid: %s\n", nav->pvt.date.valid ? "true" : "false");
}

jp_gnss_gnss_config_t create_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 1;
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
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    jp_gnss_hat_soft_reset_hot_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    pthread_t signals_interrupt;
    if (!signals_handler(gnss, &signals_interrupt))
    {
        printf("Failed to start the signal thread\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    jp_gnss_navigation_t navigation;
    while (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
        print_navigation_data(&navigation);

    pthread_join(signals_interrupt, NULL);

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
