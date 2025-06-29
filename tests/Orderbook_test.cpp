#include "Orderbook.hpp"

#include <gtest/gtest.h>

class OrderbooksStateTest : public ::testing::Test
{
   protected:
    std::mt19937 rng;

    void SetUp() override { rng.seed(std::random_device()()); }
};

// Tests for orderbookTrimPrice
TEST_F(OrderbooksStateTest, Basic)
{
    // Test ascending orderct::orderbook::OrderbooksState::trim
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, true, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.1, true, 1.0), 101.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.9, true, 1.0), 101.0);

    // Test descending order
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, false, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.1, false, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.9, false, 1.0), 100.0);
}

TEST_F(OrderbooksStateTest, EdgeCases)
{
    // Test with very small unit
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.123456, true, 0.0001), 100.1235);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.123456, false, 0.0001), 100.1234);

    // Test with very large unit
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, true, 1000.0), 1000.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, false, 1000.0), 0.0);

    // Test with unit equal to price
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, true, 100.0), 100.0);
    // FIXME:
    // EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(100.0, false, 100.0), 0.0);

    // Test with zero price
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(0.0, true, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(0.0, false, 1.0), 0.0);

    // Test with negative price
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(-100.1, true, 1.0), -100.0);
    EXPECT_DOUBLE_EQ(ct::orderbook::OrderbooksState::trim(-100.1, false, 1.0), -101.0);

    // Test invalid unit
    EXPECT_THROW(ct::orderbook::OrderbooksState::trim(100.0, true, 0.0), std::invalid_argument);
    EXPECT_THROW(ct::orderbook::OrderbooksState::trim(100.0, true, -1.0), std::invalid_argument);
}

// Stress tests
TEST_F(OrderbooksStateTest, Stress)
{
    std::uniform_real_distribution< double > price_dist(0.0, 1000.0);
    std::uniform_real_distribution< double > unit_dist(0.0001, 100.0);

    for (int i = 0; i < 1000; ++i)
    {
        double price   = price_dist(rng);
        double unit    = unit_dist(rng);
        bool ascending = (rng() % 2) == 0;

        double result = ct::orderbook::OrderbooksState::trim(price, ascending, unit);
        EXPECT_TRUE(std::isfinite(result));
        EXPECT_GE(result, 0.0);
    }
}
