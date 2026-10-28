/*
 * Jimmy Paputto 2026
 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <jimmypaputto/GnssHat.h>


void print_time_mark(const jp_gnss_time_mark_t* tm)
{
    printf("--- New TimeMark event ---\r\n");
    printf("  channel:          %u\r\n", tm->channel);
    printf("  mode:             %s\r\n",
        jp_gnss_time_mark_mode_to_string(tm->mode));
    printf("  run:              %s\r\n",
        jp_gnss_time_mark_run_to_string(tm->run));
    printf("  timeBase:         %s\r\n",
        jp_gnss_time_mark_time_base_to_string(tm->time_base));
    printf("  timeValid:        %s\r\n", tm->time_valid ? "yes" : "no");
    printf("  utcAvailable:     %s\r\n", tm->utc_available ? "yes" : "no");
    printf("  newRisingEdge:    %s\r\n", tm->new_rising_edge ? "yes" : "no");
    printf("  newFallingEdge:   %s\r\n",
        tm->new_falling_edge ? "yes" : "no");
    printf("  count:            %u\r\n", tm->count);
    printf("  rising  WN: %u  TOW: %u ms + %u ns\r\n",
        tm->week_number_rising, tm->tow_rising_ms, tm->tow_sub_rising_ns);
    printf("  falling WN: %u  TOW: %u ms + %u ns\r\n",
        tm->week_number_falling, tm->tow_falling_ms,
        tm->tow_sub_falling_ns);
    printf("  accuracy:         %u ns\r\n", tm->accuracy_estimate_ns);
    printf("\r\n");
}

jp_gnss_gnss_config_t create_config(void)
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_timing = true;
    config.timing.enable_time_mark = true;

    return config;
}

/* Toggles EXTINT every 5 navigation epochs (5 s at 1 Hz) and ends when
 * jp_gnss_hat_shutdown() stops its wait. */
void* toggle_thread_func(void* arg)
{
    jp_gnss_hat_t* gnss = (jp_gnss_hat_t*)arg;
    jp_gnss_navigation_t navigation;
    int epoch = 0;

    while (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
    {
        if (epoch++ % 5 != 0)
            continue;
        jp_gnss_hat_trigger_time_mark(gnss,
            JP_GNSS_TIME_MARK_TRIGGER_EDGE_TOGGLE);
        printf("[EXTINT] toggled\r\n");
    }

    return NULL;
}

static void termination_signals(sigset_t* signals)
{
    sigemptyset(signals);
    sigaddset(signals, SIGINT);
    sigaddset(signals, SIGTERM);
}

static void* wait_for_signal(void* gnss)
{
    sigset_t signals;
    termination_signals(&signals);
    int received;
    sigwait(&signals, &received);
    jp_gnss_hat_shutdown(gnss);
    return NULL;
}

/* SIGINT and SIGTERM make the new thread call jp_gnss_hat_shutdown(), which
 * ends the waits in main. */
static bool signals_handler(jp_gnss_hat_t* gnss, pthread_t* thread)
{
    sigset_t signals;
    termination_signals(&signals);

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

    jp_gnss_hat_soft_reset_hot_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Startup failed, exit\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }
    printf("Startup done, ublox configured\r\n");

    if (!jp_gnss_hat_enable_time_mark_trigger(gnss))
    {
        printf("Failed to enable TimeMark trigger\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }
    printf("TimeMark trigger enabled, toggling EXTINT every 5s\r\n\r\n");

    pthread_t signals_interrupt;
    if (!signals_handler(gnss, &signals_interrupt))
    {
        printf("Failed to start the signal thread\r\n");
        jp_gnss_hat_disable_time_mark_trigger(gnss);
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    /* After signals_handler(): the toggle thread inherits the blocked
     * signals. */
    pthread_t toggle_tid;
    const bool toggling =
        pthread_create(&toggle_tid, NULL, toggle_thread_func, gnss) == 0;
    if (!toggling)
        printf("Failed to start the toggle thread\r\n");

    jp_gnss_time_mark_t tm;
    while (toggling && jp_gnss_hat_wait_and_get_fresh_time_mark(gnss, &tm))
        print_time_mark(&tm);

    /* Wakes sigwait if the loop ends without a signal (e.g. a break added
     * to the loop or a future device error). */
    pthread_kill(signals_interrupt, SIGINT);
    pthread_join(signals_interrupt, NULL);
    if (toggling)
        pthread_join(toggle_tid, NULL);

    jp_gnss_hat_disable_time_mark_trigger(gnss);
    jp_gnss_hat_destroy(gnss);

    printf("Exiting...\r\n");
    return 0;
}
