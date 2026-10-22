#include <gtest/gtest.h>

#include "GnssHat.h"


TEST(CApi, CorrectionsWithoutHatAreNull)
{
    EXPECT_EQ(jp_gnss_rtk_get_full_corrections(nullptr), nullptr);
    EXPECT_EQ(jp_gnss_rtk_get_tiny_corrections(nullptr), nullptr);
    jp_gnss_rtk_corrections_free(nullptr);
}
