#include <jimmypaputto/GnssHat.h>

int consumerC(void)
{
    jp_gnss_hat_t* (*create)(void) = jp_gnss_hat_create;
    return create == 0;
}
