#include <gtest/gtest.h>

#include "ublox/Timepulse.hpp"

using namespace JimmyPaputto;


TEST(Timepulse, ListensForTheEdgeAtTopOfSecond)
{
    EXPECT_EQ(
        Timepulse::edgeAtTopOfSecond(
            ETimepulsePinPolarity::RisingEdgeAtTopOfSecond),
        GpioInterruptLine::Edge::Rising
    );
    EXPECT_EQ(
        Timepulse::edgeAtTopOfSecond(
            ETimepulsePinPolarity::FallingEdgeAtTopOfSecond),
        GpioInterruptLine::Edge::Falling
    );
}

TEST(Timepulse, DefaultPolarityIsRisingEdge)
{
    EXPECT_EQ(
        TimepulsePinConfig{}.polarity,
        ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    );
}
