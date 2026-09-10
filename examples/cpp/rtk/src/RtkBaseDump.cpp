/*
 * Jimmy Paputto 2026
 */

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <map>
#include <string>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>

using namespace JimmyPaputto;

static std::atomic<bool> g_running{true};

static void signalHandler(int)
{
    g_running = false;
}

static uint16_t frameId(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 6 || frame[0] != 0xD3)
        return 0;

    // Message number is the top 12 bits of the payload.
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(frame[3]) << 4) | (frame[4] >> 4));
}

static void printFrame(const std::vector<uint8_t>& frame)
{
    printf("    RTCM3 %4u  %3zu bytes  ", frameId(frame), frame.size());

    for (size_t i = 0; i < frame.size(); ++i)
    {
        printf("%02X ", frame[i]);
        if ((i + 1) % 16 == 0 && i + 1 < frame.size())
            printf("\n                             ");
    }
    printf("\r\n");
}

static GnssConfig createSurveyInConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        // TP1 still drives the board LED over USB; only the host-side
        // interrupt needs the HAT header.
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = RtkConfig {
            .mode = ERtkMode::Base,
            .base = BaseConfig {
                // Deliberately short and loose: enough to reach base mode
                // quickly for a smoke test, not a usable survey.
                .mode = BaseConfig::SurveyIn {
                    .minimumObservationTime_s = 120,
                    .requiredPositionAccuracy_m = 50.0
                }
            }
        }
    };
}

auto main() -> int
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // The library picks the transport itself: the HAT on the header, or a
    // receiver on USB when there is none.
    auto* receiver = IGnssHat::create();
    printf("Opened %s (%s) on %s\r\n",
        std::string(receiver->name()).c_str(),
        Utils::ubloxModel2string(receiver->model()).c_str(),
        receiver->devicePath().c_str());

    if (!receiver->rtk())
    {
        printf("RTK base mode needs an F9P\r\n");
        return -1;
    }

    receiver->softResetUbloxSom_HotStart();
    if (!receiver->start(createSurveyInConfig()))
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }

    printf("Survey-in running: 120 s minimum, 50 m required accuracy.\r\n");
    printf("Waiting for the receiver to enter base mode...\r\n");

    std::map<uint16_t, uint32_t> seen;

    while (g_running)
    {
        const auto pvt = receiver->waitAndGetFreshNavigation().pvt;
        const auto time = Utils::utcTimeFromGnss_ISO8601(pvt);

        // A base that has finished survey-in stops solving for position and
        // reports a time-only fix.
        if (pvt.fixType != EFixType::TimeOnlyFix)
        {
            printf("[%s] Survey-in in progress, fix: %s, satellites: %u\r\n",
                time.c_str(),
                Utils::eFixType2string(pvt.fixType).c_str(),
                pvt.visibleSatellites);
            continue;
        }

        const auto frames = receiver->rtk()->base()->getFullCorrections();
        if (frames.empty())
        {
            printf("[%s] Base active, no RTCM3 buffered yet\r\n", time.c_str());
            continue;
        }

        printf("[%s] Base active, %zu RTCM3 frame(s):\r\n",
            time.c_str(), frames.size());

        std::for_each(frames.cbegin(), frames.cend(),
            [&seen](const auto& frame) {
                seen[frameId(frame)]++;
                printFrame(frame);
            });
    }

    printf("\r\nRTCM3 message types received:\r\n");
    for (const auto& [id, count] : seen)
        printf("    %4u  x%u\r\n", id, count);

    return 0;
}
