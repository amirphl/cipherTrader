#include "Timeframe.hpp"
#include "Exception.hpp"

#include <gtest/gtest.h>

// Test fixture for timeframe handling
class TimeframeTest : public ::testing::Test
{
   protected:
    std::vector< ct::timeframe::Timeframe > all_timeframes = {ct::timeframe::Timeframe::MINUTE_1,
                                                              ct::timeframe::Timeframe::MINUTE_3,
                                                              ct::timeframe::Timeframe::MINUTE_5,
                                                              ct::timeframe::Timeframe::MINUTE_15,
                                                              ct::timeframe::Timeframe::MINUTE_30,
                                                              ct::timeframe::Timeframe::MINUTE_45,
                                                              ct::timeframe::Timeframe::HOUR_1,
                                                              ct::timeframe::Timeframe::HOUR_2,
                                                              ct::timeframe::Timeframe::HOUR_3,
                                                              ct::timeframe::Timeframe::HOUR_4,
                                                              ct::timeframe::Timeframe::HOUR_6,
                                                              ct::timeframe::Timeframe::HOUR_8,
                                                              ct::timeframe::Timeframe::HOUR_12,
                                                              ct::timeframe::Timeframe::DAY_1,
                                                              ct::timeframe::Timeframe::DAY_3,
                                                              ct::timeframe::Timeframe::WEEK_1,
                                                              ct::timeframe::Timeframe::MONTH_1};
};

TEST_F(TimeframeTest, MaxTimeframeBasic)
{
    std::vector< ct::timeframe::Timeframe > timeframes = {
        ct::timeframe::Timeframe::MINUTE_1, ct::timeframe::Timeframe::HOUR_1, ct::timeframe::Timeframe::DAY_1};
    EXPECT_EQ(ct::timeframe::maxTimeframe(timeframes), ct::timeframe::Timeframe::DAY_1);
}

TEST_F(TimeframeTest, MaxTimeframeEmpty)
{
    std::vector< ct::timeframe::Timeframe > empty;
    EXPECT_EQ(ct::timeframe::maxTimeframe(empty), ct::timeframe::Timeframe::MINUTE_1);
}

TEST_F(TimeframeTest, MaxTimeframeSingle)
{
    std::vector< ct::timeframe::Timeframe > single = {ct::timeframe::Timeframe::HOUR_4};
    EXPECT_EQ(ct::timeframe::maxTimeframe(single), ct::timeframe::Timeframe::HOUR_4);
}

TEST_F(TimeframeTest, MaxTimeframeAll)
{
    EXPECT_EQ(ct::timeframe::maxTimeframe(all_timeframes), ct::timeframe::Timeframe::MONTH_1);
}

TEST_F(TimeframeTest, MaxTimeframeEdgeCases)
{
    // Test with unordered timeframes
    std::vector< ct::timeframe::Timeframe > unordered = {
        ct::timeframe::Timeframe::HOUR_4, ct::timeframe::Timeframe::MINUTE_1, ct::timeframe::Timeframe::DAY_1};
    EXPECT_EQ(ct::timeframe::maxTimeframe(unordered), ct::timeframe::Timeframe::DAY_1);

    // Test with duplicate timeframes
    std::vector< ct::timeframe::Timeframe > duplicates = {
        ct::timeframe::Timeframe::MINUTE_1, ct::timeframe::Timeframe::MINUTE_1, ct::timeframe::Timeframe::HOUR_1};
    EXPECT_EQ(ct::timeframe::maxTimeframe(duplicates), ct::timeframe::Timeframe::HOUR_1);
}
// Test basic timeframe conversions
TEST_F(TimeframeTest, BasicConversions)
{
    // Test minute-based timeframes
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_1), 1);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_3), 3);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_5), 5);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_15), 15);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_30), 30);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_45), 45);

    // Test hour-based timeframes
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_1), 60);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_2), 120);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_3), 180);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_4), 240);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_6), 360);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_8), 480);
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_12), 720);

    // Test day-based timeframes
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::DAY_1), 1440); // 24 * 60
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::DAY_3), 4320); // 3 * 24 * 60

    // Test week-based timeframe
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::WEEK_1), 10080); // 7 * 24 * 60

    // Test month-based timeframe
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MONTH_1), 43200); // 30 * 24 * 60
}

// Test error handling for invalid timeframes
TEST_F(TimeframeTest, InvalidTimeframe)
{
    // Create an invalid timeframe using enum value outside the valid range
    ct::timeframe::Timeframe invalid_timeframe = static_cast< ct::timeframe::Timeframe >(-1);

    // Expect an InvalidTimeframe exception
    EXPECT_THROW({ ct::timeframe::convertTimeframeToOneMinutes(invalid_timeframe); }, ct::exception::InvalidTimeframe);
}

// Test consistency of results
TEST_F(TimeframeTest, ConsistencyCheck)
{
    // Test that multiple calls return the same result
    ct::timeframe::Timeframe test_timeframe = ct::timeframe::Timeframe::HOUR_1;
    int64_t first_result                    = ct::timeframe::convertTimeframeToOneMinutes(test_timeframe);

    for (int i = 0; i < 100; i++)
    {
        EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(test_timeframe), first_result);
    }
}

// Test relative relationships between timeframes
TEST_F(TimeframeTest, RelativeTimeframes)
{
    // Test that larger timeframes return proportionally larger values
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_2),
              ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_1) * 2);

    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::WEEK_1),
              ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::DAY_1) * 7);
}

// Test boundary values
TEST_F(TimeframeTest, BoundaryValues)
{
    // Test smallest timeframe
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MINUTE_1), 1);

    // Test largest timeframe
    EXPECT_EQ(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MONTH_1), 43200);

    // Verify that the largest timeframe doesn't overflow int64_t
    EXPECT_LT(ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::MONTH_1),
              std::numeric_limits< int64_t >::max());
}

// Stress test with multiple rapid calls
TEST_F(TimeframeTest, StressTest)
{
    std::vector< ct::timeframe::Timeframe > timeframes = {ct::timeframe::Timeframe::MINUTE_1,
                                                          ct::timeframe::Timeframe::HOUR_1,
                                                          ct::timeframe::Timeframe::DAY_1,
                                                          ct::timeframe::Timeframe::WEEK_1,
                                                          ct::timeframe::Timeframe::MONTH_1};

    // Make multiple rapid calls to test static map performance
    for (int i = 0; i < 10000; i++)
    {
        for (const auto &tf : timeframes)
        {
            EXPECT_NO_THROW({ ct::timeframe::convertTimeframeToOneMinutes(tf); });
        }
    }
}

// Test thread safety of static map
TEST_F(TimeframeTest, ThreadSafety)
{
    const int num_threads = 3;
    const int iterations  = 100;
    std::vector< std::thread > threads;
    std::atomic< bool > had_error{false};

    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back(
            [&]()
            {
                try
                {
                    for (int j = 0; j < iterations; j++)
                    {
                        ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::HOUR_1);
                        ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::DAY_1);
                        ct::timeframe::convertTimeframeToOneMinutes(ct::timeframe::Timeframe::WEEK_1);
                    }
                }
                catch (...)
                {
                    had_error = true;
                }
            });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_FALSE(had_error);
}
