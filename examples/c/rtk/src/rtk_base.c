/*
 * Jimmy Paputto 2025
 *
 * RTK Base Station example (C)
 *
 * Demonstrates configuring the GNSS module as an RTK Base station
 * using the C API. Shows three configuration modes:
 *   - Survey-In: The module determines its position automatically
 *   - Fixed Position (LLA): Provide known lat/lon/height
 *   - Fixed Position (ECEF): Provide known ECEF coordinates
 *
 * Once configured, the base station produces RTCM3 correction
 * frames that can be forwarded to an RTK rover.
 */

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jimmypaputto/GnssHat.h>


static uint16_t get_rtcm3_frame_id(const uint8_t* data, uint32_t size)
{
    if (size < 6 || data[0] != 0xD3)
        return 0;

    return ((uint16_t)data[3] << 4) | (data[4] >> 4);
}

void print_rtcm3_frame(const jp_gnss_rtcm3_frame_t* frame)
{
    uint16_t msg_id = get_rtcm3_frame_id(frame->data, frame->size);
    printf("RTCM3 Frame %u: %u bytes: ", msg_id, frame->size);

    for (uint32_t j = 0; j < frame->size; ++j)
    {
        printf("%02X ", frame->data[j]);
        if ((j + 1) % 16 == 0 && j + 1 < frame->size)
            printf("\n                                             ");
    }
    printf("\r\n");
}

jp_gnss_gnss_config_t create_survey_in_config()
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

    config.has_rtk = true;
    config.rtk.mode = JP_GNSS_RTK_MODE_BASE;
    config.rtk.has_base_config = true;
    config.rtk.base.base_mode = JP_GNSS_BASE_MODE_SURVEY_IN;
    config.rtk.base.survey_in.minimum_observation_time_s = 120;
    config.rtk.base.survey_in.required_position_accuracy_m = 50.0;

    return config;
}

jp_gnss_gnss_config_t create_fixed_position_lla_config()
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

    config.has_rtk = true;
    config.rtk.mode = JP_GNSS_RTK_MODE_BASE;
    config.rtk.has_base_config = true;
    config.rtk.base.base_mode = JP_GNSS_BASE_MODE_FIXED_POSITION;
    config.rtk.base.fixed_position.position_type = JP_GNSS_FIXED_POSITION_LLA;
    config.rtk.base.fixed_position.lla.latitude_deg  = 52.232222222;
    config.rtk.base.fixed_position.lla.longitude_deg = 21.008055556;
    config.rtk.base.fixed_position.lla.height_m      = 110.0;
    config.rtk.base.fixed_position.position_accuracy_m = 0.5;

    return config;
}

jp_gnss_gnss_config_t create_fixed_position_ecef_config()
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

    config.has_rtk = true;
    config.rtk.mode = JP_GNSS_RTK_MODE_BASE;
    config.rtk.has_base_config = true;
    config.rtk.base.base_mode = JP_GNSS_BASE_MODE_FIXED_POSITION;
    config.rtk.base.fixed_position.position_type = JP_GNSS_FIXED_POSITION_ECEF;
    config.rtk.base.fixed_position.ecef.x_m = 3656215.987;
    config.rtk.base.fixed_position.ecef.y_m = 1409547.654;
    config.rtk.base.fixed_position.ecef.z_m = 5049982.321;
    config.rtk.base.fixed_position.position_accuracy_m = 0.5;

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

int main(int argc, char* argv[])
{
    uint16_t port = 2101;
    const char* mountpoint = "GNSS_HAT";

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = (uint16_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--mountpoint") == 0 && i + 1 < argc)
            mountpoint = argv[++i];
    }

    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Error: Failed to create GNSS instance\r\n");
        return -1;
    }

    jp_gnss_hat_soft_reset_hot_start(gnss);

    /* Use Survey-In mode by default.
     * Alternatively try: create_fixed_position_lla_config()
     *                or: create_fixed_position_ecef_config()  */
    jp_gnss_gnss_config_t config = create_survey_in_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("GNSS started as RTK Base (Survey-In mode).\r\n");
    printf("Waiting for corrections...\r\n\n");

    jp_gnss_ntrip_caster_t* caster =
        jp_gnss_ntrip_caster_create("0.0.0.0", port, mountpoint, 10);
    if (!caster)
    {
        printf("Failed to create NTRIP caster\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    if (!jp_gnss_ntrip_caster_start(caster))
    {
        printf("Failed to start NTRIP caster\r\n");
        jp_gnss_ntrip_caster_destroy(caster);
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    pthread_t signals_interrupt;
    if (!signals_handler(gnss, &signals_interrupt))
    {
        printf("Failed to start the signal thread\r\n");
        jp_gnss_ntrip_caster_stop(caster);
        jp_gnss_ntrip_caster_destroy(caster);
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    jp_gnss_navigation_t navigation;
    while (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
    {
        const char* time_str = jp_gnss_utc_time_iso8601(&navigation.pvt);
        const char* fix_type_str =
            jp_gnss_fix_type_to_string(navigation.pvt.fix_type);

        if (navigation.pvt.fix_type != JP_GNSS_FIX_TYPE_TIME_ONLY_FIX)
        {
            printf("[%s] Fix type: %s, waiting for TimeOnlyFix for RTK Base\r\n",
                time_str, fix_type_str);
        }
        else
        {
            printf("[%s] RTK Base ready with TimeOnlyFix\r\n", time_str);

            jp_gnss_rtk_corrections_t* corrections =
                jp_gnss_rtk_get_full_corrections(gnss);
            if (corrections && corrections->count > 0)
            {
                for (uint32_t i = 0; i < corrections->count; ++i)
                {
                    print_rtcm3_frame(&corrections->frames[i]);
                }

                jp_gnss_ntrip_caster_feed(
                    caster, corrections->frames, corrections->count);
                jp_gnss_ntrip_caster_update_position(
                    caster,
                    navigation.pvt.latitude,
                    navigation.pvt.longitude);

                uint32_t clients =
                    jp_gnss_ntrip_caster_client_count(caster);
                if (clients > 0)
                    printf("[%s] Broadcasting to %u client(s)\r\n",
                        time_str, clients);
            }
            jp_gnss_rtk_corrections_free(corrections);
        }
    }

    pthread_join(signals_interrupt, NULL);

    jp_gnss_ntrip_caster_stop(caster);
    jp_gnss_ntrip_caster_destroy(caster);
    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
