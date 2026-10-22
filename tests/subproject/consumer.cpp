#include <jimmypaputto/GnssHat.hpp>

extern "C" int consumerC(void);

// Only built, never run: taking the addresses makes the link step resolve
// symbols from the subproject's libgnsshat without touching hardware.
int main()
{
    auto create = &JimmyPaputto::IGnssHat::create;
    return create == nullptr || consumerC() != 0;
}
