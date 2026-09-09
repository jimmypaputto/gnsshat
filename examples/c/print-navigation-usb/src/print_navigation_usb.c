/*
 * Jimmy Paputto 2026
 */

#include <stdio.h>
#include <stdlib.h>

#include <jimmypaputto/GnssHat.h>


static void print_navigation(const jp_gnss_navigation_t* navigation)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Type: %s\r\n",
        jp_gnss_fix_type_to_string(navigation->pvt.fix_type));
    printf("\tVisible Satellites: %d\r\n", navigation->pvt.visible_satellites);
    printf("\tLatitude: %.7f\r\n", navigation->pvt.latitude);
    printf("\tLongitude: %.7f\r\n", navigation->pvt.longitude);
    printf("\tAltitude: %.2f m\r\n", navigation->pvt.altitude);
}

int main(int argc, char** argv)
{
    /* Without an argument the library picks the only attached receiver and
     * aborts if there is more than one. */
    const char* device_path = (argc > 1) ? argv[1] : NULL;

    jp_gnss_hat_t* receiver = jp_gnss_hat_create_usb(device_path);
    if (!receiver)
    {
        printf("Failed to open u-blox USB receiver\r\n");
        return -1;
    }

    printf("Opened %s on %s\r\n",
        jp_gnss_ublox_model_to_string(jp_gnss_hat_usb_model(receiver)),
        jp_gnss_hat_usb_device_path(receiver));

    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);
    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_PORTABLE;
    /* The timepulse pin is on the HAT header and unreachable over USB. */
    config.timepulse_pin_config.active = false;

    jp_gnss_hat_soft_reset_hot_start(receiver);
    if (!jp_gnss_hat_start(receiver, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(receiver);
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    for (;;)
    {
        jp_gnss_navigation_t navigation;
        if (jp_gnss_hat_wait_and_get_fresh_navigation(receiver, &navigation))
            print_navigation(&navigation);
    }

    jp_gnss_hat_destroy(receiver);
    return 0;
}
