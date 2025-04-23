// CandleTest.cpp
#include "Candle.hpp"
#include <gtest/gtest.h>

class CandleTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Common setup code if needed
    }

    // Helper function to check if a value is within expected range
    template < typename T >
    bool isInRange(T value, T min, T max)
    {
        return value >= min && value <= max;
    }

    // Helper function to validate candle structure
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
        int num = ct::candle::randint(min, max);
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
    blaze::DynamicVector< double > attrs(6, 0.0);
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
    auto candles       = ct::candle::generateRangeCandles< double >(count);

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
    auto zeroCandles = ct::candle::generateRangeCandles< double >(0);
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
    blaze::DynamicVector< float > floatAttrs(6, 0.0f);
    auto floatCandle = ct::candle::generateFakeCandle(floatAttrs, true);
    validateCandleStructure(floatCandle);

    // Test with double
    blaze::DynamicVector< double > doubleAttrs(6, 0.0);
    auto doubleCandle = ct::candle::generateFakeCandle(doubleAttrs, true);
    validateCandleStructure(doubleCandle);
}

// Test sequential behavior
TEST_F(CandleTest, SequentialGeneration)
{
    const size_t count = 5;
    auto candles       = ct::candle::generateRangeCandles< double >(count);

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
    auto firstCandle  = ct::candle::generateFakeCandle(blaze::DynamicVector< double >(6, 0.0), true);
    auto secondCandle = ct::candle::generateFakeCandle(blaze::DynamicVector< double >(6, 0.0), true);

    EXPECT_GT(firstCandle[0], 1609459080000) << "Timestamp should be after base time";
    EXPECT_GT(secondCandle[0], 1609459080000) << "Timestamp should be after base time";
}
