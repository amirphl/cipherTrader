#include "Candle.hpp"
#include "Exception.hpp"

#include <gtest/gtest.h>

class CandleTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Common setup code if needed
    }

    // candle function to check if a value is within expected range
    template < typename T >
    bool isInRange(T value, T min, T max)
    {
        return value >= min && value <= max;
    }

    // candle function to validate candle structure
    template < typename T >
    static void validateCandleStructure(const T& candle)
    {
        ASSERT_EQ(candle.size(), 6) << "Candle should have 6 components";

        // Timestamp validation (should be after 2021-01-01)
        EXPECT_GT(candle[0], 1609459080000) << "Timestamp should be after 2021-01-01";

        // Price relationships
        EXPECT_LE(candle[4], candle[3]) << "Low price should be less than or equal to high price";
        EXPECT_LE(candle[1], candle[3]) << "Open price should be less than or equal to high price";
        EXPECT_LE(candle[2], candle[3]) << "Close price should be less than or equal to high price";
        EXPECT_GE(candle[1], candle[4]) << "Open price should be greater than or equal to low price";
        EXPECT_GE(candle[2], candle[4]) << "Close price should be greater than or equal to low price";

        // Volume validation
        EXPECT_GT(candle[5], 0) << "Volume should be positive";
    }
};

// Test random number generation
TEST_F(CandleTest, RandomIntGeneration)
{
    const int min = 1;
    const int max = 100;
    std::vector< int > numbers;

    // Generate multiple random numbers to check distribution
    for (int i = 0; i < 1000; ++i)
    {
        int num = ct::candle::RandomGenerator::getInstance().randint(min, max);
        EXPECT_GE(num, min);
        EXPECT_LE(num, max);
        numbers.push_back(num);
    }

    // Check if we get different numbers (randomness test)
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    EXPECT_GT(numbers.size(), 50) << "Random number generation should produce varied results";
}

// Test single candle generation
TEST_F(CandleTest, FakeCandleGeneration)
{
    // Test with double precision
    blaze::DynamicVector< double, blaze::rowVector > attrs(6, 0.0);
    auto candle = ct::candle::generateFakeCandle(attrs, true);
    validateCandleStructure(candle);

    // Test with custom attributes
    attrs[1]          = 50.0; // Set specific open price
    auto customCandle = ct::candle::generateFakeCandle(attrs, false);
    EXPECT_EQ(customCandle[1], 50.0) << "Custom open price should be preserved";
}

// Test candles from close prices
TEST_F(CandleTest, CandlesFromClosePrices)
{
    std::vector< double > prices = {100.0, 101.0, 99.5, 102.0, 101.5};
    auto candles                 = ct::candle::generateCandlesFromClosePrices(prices, true);

    ASSERT_EQ(candles.rows(), prices.size()) << "Should generate correct number of candles";
    ASSERT_EQ(candles.columns(), 6) << "Each candle should have 6 components";

    // Validate each candle
    for (size_t i = 0; i < candles.rows(); ++i)
    {
        auto row = blaze::row(candles, i);
        validateCandleStructure(row);

        if (i > 0)
        {
            // Check if timestamps are sequential
            EXPECT_GT(candles(i, 0), candles(i - 1, 0)) << "Timestamps should be strictly increasing";

            // Check if close prices match input
            EXPECT_DOUBLE_EQ(candles(i, 2), prices[i]) << "Close prices should match input prices";
        }
    }
}

// Test range candles generation
TEST_F(CandleTest, RangeCandlesGeneration)
{
    const size_t count = 10;
    auto candles       = ct::candle::generateRangeCandles< double >(count, true);

    ASSERT_EQ(candles.rows(), count) << "Should generate requested number of candles";
    ASSERT_EQ(candles.columns(), 6) << "Each candle should have 6 components";

    // Validate each candle
    for (size_t i = 0; i < candles.rows(); ++i)
    {
        auto row = blaze::row(candles, i);
        validateCandleStructure(row);

        if (i > 0)
        {
            // Check if timestamps are sequential
            EXPECT_GT(candles(i, 0), candles(i - 1, 0)) << "Timestamps should be strictly increasing";
        }
    }
}

// Test edge cases
TEST_F(CandleTest, EdgeCases)
{
    // Test empty price list
    auto emptyCandles = ct::candle::generateCandlesFromClosePrices(std::vector< double >{}, true);
    EXPECT_EQ(emptyCandles.rows(), 0) << "Empty price list should produce empty candle matrix";

    // Test zero count range candles
    auto zeroCandles = ct::candle::generateRangeCandles< double >(0, true);
    EXPECT_EQ(zeroCandles.rows(), 0) << "Zero count should produce empty candle matrix";

    // Test single price
    std::vector< double > singlePrice = {100.0};
    auto singleCandle                 = ct::candle::generateCandlesFromClosePrices(singlePrice, true);
    ASSERT_EQ(singleCandle.rows(), 1) << "Single price should produce one candle";
    validateCandleStructure(blaze::row(singleCandle, 0));
}

// Test type compatibility
TEST_F(CandleTest, TypeCompatibility)
{
    // Test with float
    blaze::DynamicVector< float, blaze::rowVector > floatAttrs(6, 0.0f);
    auto floatCandle = ct::candle::generateFakeCandle(floatAttrs, true);
    validateCandleStructure(floatCandle);

    // Test with double
    blaze::DynamicVector< double, blaze::rowVector > doubleAttrs(6, 0.0);
    auto doubleCandle = ct::candle::generateFakeCandle(doubleAttrs, true);
    validateCandleStructure(doubleCandle);
}

// Test sequential behavior
TEST_F(CandleTest, SequentialGeneration)
{
    const size_t count = 5;
    auto candles       = ct::candle::generateRangeCandles< double >(count, true);

    // Check if timestamps are strictly increasing
    for (size_t i = 1; i < count; ++i)
    {
        EXPECT_EQ(candles(i, 0) - candles(i - 1, 0), 60000) << "Timestamps should increase by exactly 60000 (1 minute)";
    }
}

// Test reset behavior
TEST_F(CandleTest, ResetBehavior)
{
    // Generate two sets of candles with reset
    auto firstCandle  = ct::candle::generateFakeCandle(blaze::DynamicVector< double, blaze::rowVector >(6, 0.0), true);
    auto secondCandle = ct::candle::generateFakeCandle(blaze::DynamicVector< double, blaze::rowVector >(6, 0.0), true);

    EXPECT_GT(firstCandle[0], 1609459080000) << "Timestamp should be after base time";
    EXPECT_GT(secondCandle[0], 1609459080000) << "Timestamp should be after base time";
}

// Test fixture
class GetCandleSource : public ::testing::Test
{
   protected:
    blaze::DynamicMatrix< double > candles;

    void SetUp() override
    {
        // Sample candle data: [timestamp, open, close, high, low, volume]
        candles = blaze::DynamicMatrix< double >(2UL, 6UL);
        candles = {{1609459200.0, 100.0, 101.0, 102.0, 99.0, 1000.0},
                   {1609462800.0, 101.0, 102.0, 103.0, 100.0, 1500.0}};
    }

    void TearDown() override {}
};

TEST_F(GetCandleSource, GetCandleSourceEnumClose)
{
    auto result = ct::candle::getCandleSource(candles, ct::candle::Source::Close);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], 101.0);
    EXPECT_DOUBLE_EQ(result[1], 102.0);
}

TEST_F(GetCandleSource, GetCandleSourceEnumHigh)
{
    auto result = ct::candle::getCandleSource(candles, ct::candle::Source::High);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], 102.0);
    EXPECT_DOUBLE_EQ(result[1], 103.0);
}

TEST_F(GetCandleSource, GetCandleSourceEnumHL2)
{
    auto result = ct::candle::getCandleSource(candles, ct::candle::Source::HL2);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0) / 2.0); // 100.5
    EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0) / 2.0);     // 101.5
}

TEST_F(GetCandleSource, GetCandleSourceEnumHLC3)
{
    auto result = ct::candle::getCandleSource(candles, ct::candle::Source::HLC3);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0 + 101.0) / 3.0);  // 100.666...
    EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0 + 102.0) / 3.0); // 101.666...
}

TEST_F(GetCandleSource, GetCandleSourceEnumOHLC4)
{
    auto result = ct::candle::getCandleSource(candles, ct::candle::Source::OHLC4);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (100.0 + 102.0 + 99.0 + 101.0) / 4.0);  // 100.5
    EXPECT_DOUBLE_EQ(result[1], (101.0 + 103.0 + 100.0 + 102.0) / 4.0); // 101.5
}

TEST_F(GetCandleSource, GetCandleSourceEnumEmptyMatrix)
{
    blaze::DynamicMatrix< double > empty(0UL, 6UL);
    EXPECT_THROW(ct::candle::getCandleSource(empty, ct::candle::Source::Close), std::invalid_argument);
}

TEST_F(GetCandleSource, GetCandleSourceEnumInsufficientColumns)
{
    blaze::DynamicMatrix< double > small(2UL, 3UL);
    EXPECT_THROW(ct::candle::getCandleSource(small, ct::candle::Source::Close), std::invalid_argument);
}

class GetNextCandleTimestampTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create test candles with known timestamps
        baseCandle    = blaze::DynamicVector< int64_t, blaze::rowVector >(5); // Typical OHLCV format
        baseCandle[0] = 1609459200000;                                        // 2021-01-01 00:00:00 UTC in milliseconds
    }

    blaze::DynamicVector< int64_t, blaze::rowVector > baseCandle;
};

TEST_F(GetNextCandleTimestampTest, BasicTimeframes)
{
    // Test basic timeframe calculations
    EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, ct::timeframe::Timeframe::MINUTE_1),
              baseCandle[0] + 60'000); // +1 minute

    EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, ct::timeframe::Timeframe::HOUR_1),
              baseCandle[0] + 3600'000); // +1 hour

    EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, ct::timeframe::Timeframe::DAY_1),
              baseCandle[0] + 86400'000); // +1 day
}

TEST_F(GetNextCandleTimestampTest, EmptyCandle)
{
    // Test with empty candle vector
    blaze::DynamicVector< int64_t, blaze::rowVector > emptyCandle(0);
    EXPECT_THROW(ct::candle::getNextCandleTimestamp(emptyCandle, ct::timeframe::Timeframe::MINUTE_1),
                 std::invalid_argument);
}

TEST_F(GetNextCandleTimestampTest, LargeTimeframes)
{
    // Test with larger timeframes
    EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, ct::timeframe::Timeframe::WEEK_1),
              baseCandle[0] + 604800'000); // +1 week

    EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, ct::timeframe::Timeframe::MONTH_1),
              baseCandle[0] + 2592000'000); // +30 days
}

TEST_F(GetNextCandleTimestampTest, MaxTimestampBoundary)
{
    // Test near int64_t maximum value to check for overflow
    blaze::DynamicVector< int64_t, blaze::rowVector > maxCandle(5);
    maxCandle[0] = std::numeric_limits< int64_t >::max() - 60'000; // Just below max

    // Should work with 1-minute timeframe
    EXPECT_NO_THROW(ct::candle::getNextCandleTimestamp(maxCandle, ct::timeframe::Timeframe::MINUTE_1));

    // FIXME:
    // Should throw or handle overflow for larger timeframes
    // EXPECT_EQ(ct::candle::getNextCandleTimestamp(maxCandle, ct::timeframe::Timeframe::DAY_1), ???);
}

TEST_F(GetNextCandleTimestampTest, NegativeTimestamp)
{
    // Test with negative timestamp
    blaze::DynamicVector< int64_t, blaze::rowVector > negativeCandle(5);
    negativeCandle[0] = -1000;

    // Should still calculate correctly with negative timestamps
    EXPECT_EQ(ct::candle::getNextCandleTimestamp(negativeCandle, ct::timeframe::Timeframe::MINUTE_1), -1000 + 60'000);
}

TEST_F(GetNextCandleTimestampTest, AllTimeframes)
{
    // Test all available timeframes
    std::vector< std::pair< ct::timeframe::Timeframe, int64_t > > timeframes = {
        {ct::timeframe::Timeframe::MINUTE_1, 60'000},
        {ct::timeframe::Timeframe::MINUTE_3, 180'000},
        {ct::timeframe::Timeframe::MINUTE_5, 300'000},
        {ct::timeframe::Timeframe::MINUTE_15, 900'000},
        {ct::timeframe::Timeframe::MINUTE_30, 1800'000},
        {ct::timeframe::Timeframe::MINUTE_45, 2700'000},
        {ct::timeframe::Timeframe::HOUR_1, 3600'000},
        {ct::timeframe::Timeframe::HOUR_2, 7200'000},
        {ct::timeframe::Timeframe::HOUR_3, 10800'000},
        {ct::timeframe::Timeframe::HOUR_4, 14400'000},
        {ct::timeframe::Timeframe::HOUR_6, 21600'000},
        {ct::timeframe::Timeframe::HOUR_8, 28800'000},
        {ct::timeframe::Timeframe::HOUR_12, 43200'000},
        {ct::timeframe::Timeframe::DAY_1, 86400'000},
        {ct::timeframe::Timeframe::DAY_3, 259200'000},
        {ct::timeframe::Timeframe::WEEK_1, 604800'000},
        {ct::timeframe::Timeframe::MONTH_1, 2592000'000}};

    for (const auto& [timeframe, expected_ms] : timeframes)
    {
        EXPECT_EQ(ct::candle::getNextCandleTimestamp(baseCandle, timeframe), baseCandle[0] + expected_ms)
            << "Failed for timeframe: " << static_cast< int >(timeframe);
    }
}

TEST_F(GetNextCandleTimestampTest, InvalidTimeframe)
{
    // Test with invalid timeframe enum value
    // Note: This assumes ct::timeframe::Timeframe has an INVALID or similar value
    ct::timeframe::Timeframe invalid_timeframe = static_cast< ct::timeframe::Timeframe >(-1);
    EXPECT_THROW(ct::candle::getNextCandleTimestamp(baseCandle, invalid_timeframe), ct::exception::InvalidTimeframe);
}
