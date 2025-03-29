#include "Helper.hpp"
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <fstream>
#include <limits>
#include <random>
#include <regex>
#include <set>
#include <stdexcept>
#include <thread>
#include "Candle.hpp"
#include "Config.hpp"
#include "Enum.hpp"
#include "Exception.hpp"
#include "Route.hpp"
#include <blaze/Math.h>
#include <date/date.h>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class AssetTest : public ::testing::Test
{
   protected:
    std::string typical_symbol = "BTC-USD";
    std::string no_dash        = "BTCUSD";
    std::string empty          = "";
};

// Tests forHelper::quoteAsset
TEST_F(AssetTest, QuoteAsset_TypicalSymbol)
{
    auto result = Helper::quoteAsset(typical_symbol);
    EXPECT_EQ(result, "USD");
}

TEST_F(AssetTest, QuoteAsset_NoDash)
{
    try
    {
        auto result = Helper::quoteAsset(no_dash);
        FAIL() << "Expected std::invalid_argument but no exception was thrown";
    }
    catch (const std::invalid_argument &e)
    {
        EXPECT_STREQ("Symbol is invalid", e.what()) << "Exception message mismatch";
    }
    catch (const std::exception &e)
    {
        FAIL() << "Expected std::invalid_argument but got different exception: " << e.what();
    }
}

TEST_F(AssetTest, QuoteAsset_EmptyString)
{
    try
    {
        auto result = Helper::quoteAsset(empty);
        FAIL() << "Expected std::invalid_argument but no exception was thrown";
    }
    catch (const std::invalid_argument &e)
    {
        EXPECT_STREQ("Symbol is invalid", e.what()) << "Exception message mismatch";
    }
    catch (const std::exception &e)
    {
        FAIL() << "Expected std::invalid_argument but got different exception: " << e.what();
    }
}

TEST_F(AssetTest, QuoteAsset_OnlyDash)
{
    auto result = Helper::quoteAsset("-");
    EXPECT_EQ(result, "");
}

TEST_F(AssetTest, QuoteAsset_DashAtStart)
{
    auto result = Helper::quoteAsset("-USD");
    EXPECT_EQ(result, "USD");
}

TEST_F(AssetTest, QuoteAsset_DashAtEnd)
{
    auto result = Helper::quoteAsset("BTC-");
    EXPECT_EQ(result, "");
}

TEST_F(AssetTest, QuoteAsset_MultipleDashes)
{
    auto result = Helper::quoteAsset("BTC-USD-TEST");
    EXPECT_EQ(result, "USD-TEST"); // Takes everything after dash
}

// Tests forHelper::Helper::baseAsset
TEST_F(AssetTest, BaseAsset_TypicalSymbol)
{
    EXPECT_EQ(Helper::baseAsset(typical_symbol), "BTC");
}

TEST_F(AssetTest, BaseAsset_NoDash)
{
    EXPECT_EQ(Helper::baseAsset(no_dash), "BTCUSD");
}

TEST_F(AssetTest, BaseAsset_EmptyString)
{
    EXPECT_EQ(Helper::baseAsset(empty), "");
}

TEST_F(AssetTest, BaseAsset_OnlyDash)
{
    EXPECT_EQ(Helper::baseAsset("-"), "");
}

TEST_F(AssetTest, BaseAsset_DashAtStart)
{
    EXPECT_EQ(Helper::baseAsset("-USD"), "");
}

TEST_F(AssetTest, BaseAsset_DashAtEnd)
{
    EXPECT_EQ(Helper::baseAsset("BTC-"), "BTC");
}

TEST_F(AssetTest, BaseAsset_MultipleDashes)
{
    EXPECT_EQ(Helper::baseAsset("BTC-USD-TEST"),
              "BTC"); // Takes everything before dash
}

class AppCurrencyTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        std::vector< nlohmann::json > routes_data = {
            {{"exchange", "Binance Spot"},
             {"symbol", "BTC-USD"},
             {"timeframe", "1h"},
             {"strategy_name", "MyStrategy"},
             {"dna", "abc123"}},
        };
        Route::Router::getInstance().setRoutes(routes_data);
    };

    void TearDown() override { Route::Router::getInstance().reset(); }
};

TEST_F(AppCurrencyTest, NoSettlementCurrency)
{
    auto result = Helper::appCurrency();
    EXPECT_EQ(result, "USD");
}

TEST_F(AppCurrencyTest, WithSettlementCurrency)
{
    Route::Router::getInstance().setRoutes({{{"exchange", "Bybit USDC Perpetual"},
                                             {"symbol", "ETH-ART"},
                                             {"timeframe", "1h"},
                                             {"strategy_name", "MyStrategy"},
                                             {"dna", "abc123"}}});
    auto result = Helper::appCurrency();
    EXPECT_EQ(result, "USDT");
}

class BinarySearchTest : public ::testing::Test
{
   protected:
    std::vector< int > sorted_ints{1, 3, 5, 7, 9};
    std::vector< std::string > sorted_strings{"apple", "banana", "cherry"};
};

// Basic functionality tests
TEST_F(BinarySearchTest, FindsExistingElement)
{
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 5), 2);
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 1), 0);
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 9), 4);

    EXPECT_EQ(Helper::binarySearch(sorted_strings, std::string("banana")), 1);
}

TEST_F(BinarySearchTest, ReturnsMinusOneForNonExistingElement)
{
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 4), -1);
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 0), -1);
    EXPECT_EQ(Helper::binarySearch(sorted_ints, 10), -1);

    EXPECT_EQ(Helper::binarySearch(sorted_strings, std::string("date")), -1);
}

// Edge case tests
TEST_F(BinarySearchTest, EmptyVector)
{
    std::vector< int > empty;
    EXPECT_EQ(Helper::binarySearch(empty, 5), -1);
}

TEST_F(BinarySearchTest, SingleElementFound)
{
    std::vector< int > single{42};
    EXPECT_EQ(Helper::binarySearch(single, 42), 0);
}

TEST_F(BinarySearchTest, SingleElementNotFound)
{
    std::vector< int > single{42};
    EXPECT_EQ(Helper::binarySearch(single, 43), -1);
}

TEST_F(BinarySearchTest, AllElementsSame)
{
    std::vector< int > same_elements(5, 7); // 5 elements, all 7
    EXPECT_GE(Helper::binarySearch(same_elements, 7),
              0); // Should find some index
    EXPECT_EQ(Helper::binarySearch(same_elements, 8), -1);
}

TEST_F(BinarySearchTest, LargeVector)
{
    std::vector< int > large;
    for (int i = 0; i < 1000; i += 2)
    {
        large.push_back(i);
    }
    EXPECT_EQ(Helper::binarySearch(large, 500), 250);
    EXPECT_EQ(Helper::binarySearch(large, 501), -1);
}

TEST_F(BinarySearchTest, AndLastElements)
{
    std::vector< int > nums{1, 2, 3, 4, 5};
    EXPECT_EQ(Helper::binarySearch(nums, 1), 0); // element
    EXPECT_EQ(Helper::binarySearch(nums, 5), 4); // Last element
}

class StyleTest : public ::testing::Test
{
};

// Tests for style function
TEST_F(StyleTest, StyleBasic)
{
    // Test bold style
    std::string bold_result = Helper::style("test", "bold");
    EXPECT_EQ(bold_result, "\033[1mtest\033[0m");

    // Test underline style
    std::string underline_result = Helper::style("test", "underline");
    EXPECT_EQ(underline_result, "\033[4mtest\033[0m");

    // Test abbreviations
    EXPECT_EQ(Helper::style("test", "b"), "\033[1mtest\033[0m");
    EXPECT_EQ(Helper::style("test", "u"), "\033[4mtest\033[0m");
}

TEST_F(StyleTest, StyleEdgeCases)
{
    // Test empty style
    EXPECT_EQ(Helper::style("test", ""), "test");

    // Test empty text
    EXPECT_EQ(Helper::style("", "bold"), "\033[1m\033[0m");

    // Test invalid style
    EXPECT_THROW(Helper::style("test", "invalid"), std::invalid_argument);

    // Test case insensitivity
    EXPECT_EQ(Helper::style("test", "BOLD"), "\033[1mtest\033[0m");
    EXPECT_EQ(Helper::style("test", "UNDERLINE"), "\033[4mtest\033[0m");
}

class ErrorTest : public ::testing::Test
{
};

// Tests for error function
TEST_F(ErrorTest, ErrorOutput)
{
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf *old_buf = std::cerr.rdbuf(buffer.rdbuf());

    // Call error function
    Helper::error("Test error", true);

    // Restore cerr
    std::cerr.rdbuf(old_buf);

    // Check if output contains error message
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("Test error") != std::string::npos);
    EXPECT_TRUE(output.find("CRITICAL ERROR") != std::string::npos);
}

class DebugTest : public ::testing::Test
{
};

// Tests for debug function
TEST_F(DebugTest, DebugBasic)
{
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf *old_buf = std::cout.rdbuf(buffer.rdbuf());

    // Test with single item
    Helper::debug("test");
    EXPECT_EQ(buffer.str(), "==> test\nTODO: Log this - ==> test\n");
    buffer.str("");

    // Test with multiple items
    Helper::debug("test", 123, 3.14);
    EXPECT_EQ(buffer.str(), "==> test, 123, 3.14\nTODO: Log this - ==> test, 123, 3.14\n");
    buffer.str("");

    // Test with no items
    Helper::debug();
    EXPECT_EQ(buffer.str(), "==> \nTODO: Log this - ==> \n");

    // Restore cerr
    std::cout.rdbuf(old_buf);
}

TEST_F(DebugTest, DebugEdgeCases)
{
    // Redirect cerr to capture output
    std::stringstream buffer;
    std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

    // Test with empty string
    Helper::debug("");
    EXPECT_EQ(buffer.str(), "==> \nTODO: Log this - ==> \n");
    buffer.str("");

    // Test with special characters
    Helper::debug("test\n\t");
    EXPECT_EQ(buffer.str(), "==> test\n\t\nTODO: Log this - ==> test\n\t\n");
    buffer.str("");

    // Test with nullptr
    const char *null_str = nullptr;
    Helper::debug(null_str);
    EXPECT_EQ(buffer.str(), "==> (null)\nTODO: Log this - ==> (null)\n");
    buffer.str("");

    // Test with large numbers
    Helper::debug(INT_MAX, UINT_MAX, LLONG_MAX);
    EXPECT_TRUE(buffer.str().find(std::to_string(INT_MAX)) != std::string::npos);
    EXPECT_TRUE(buffer.str().find(std::to_string(UINT_MAX)) != std::string::npos);
    EXPECT_TRUE(buffer.str().find(std::to_string(LLONG_MAX)) != std::string::npos);

    // Restore cerr
    std::cout.rdbuf(old);
}

// Tests for clearOutput function
// TEST_F(DebugTest, ClearOutput)
// {
//     // Test that it doesn't crash
//     EXPECT_NO_THROW(Helper::clearOutput());
// }

class JoinItemsTest : public ::testing::Test
{
};

// Tests for joinItems function
TEST_F(JoinItemsTest, JoinItemsBasic)
{
    // Test with strings
    EXPECT_EQ(Helper::joinItems("hello", "world"), "==> hello, world");

    // Test with numbers
    EXPECT_EQ(Helper::joinItems(123, 456), "==> 123, 456");

    // Test with mixed types
    EXPECT_EQ(Helper::joinItems("test", 123, 3.14), "==> test, 123, 3.14");
}

TEST_F(JoinItemsTest, JoinItemsEdgeCases)
{
    // Test with empty string
    EXPECT_EQ(Helper::joinItems(""), "==> ");

    // Test with no items
    EXPECT_EQ(Helper::joinItems(), "==> ");

    // Test with nullptr
    const char *null_str = nullptr;
    EXPECT_EQ(Helper::joinItems(null_str), "==> (null)");

    // Test with special characters
    EXPECT_EQ(Helper::joinItems("test\n", "world\t"), "==> test\n, world\t");

    // FIXME:Iter template instantiation
    // Test with large number of items
    // std::vector< std::string > items(1000, "test");
    // std::string expected;
    // for (size_t i = 0; i < items.size(); ++i)
    // {
    //     if (i > 0)
    //         expected += " ";
    //     expected += "test";
    // }
    // EXPECT_EQ(Helper::joinItems(items.begin(), items.end()), expected);
}

// FIXME:Iter template instantiation
// Stress tests
// TEST_F(JoinItemsTest, StressTestJoinItems)
// {
//     // Test with large number of items
//     std::vector< std::string > items(10000, "test");
//     std::string result = Helper::joinItems(items.begin(), items.end());

// // Verify length and content
// EXPECT_EQ(result.length(), 49999); // 10000 * 5 - 9999 spaces
// EXPECT_TRUE(result.find("test") != std::string::npos);
// }

// Test fixture
class DashySymbolTest : public ::testing::Test
{
};

// Test cases
TEST(DashySymbolTest, AlreadyHasDash)
{
    EXPECT_EQ(Helper::dashySymbol("BTC-USD"), "BTC-USD");
    EXPECT_EQ(Helper::dashySymbol("XRP-EUR"), "XRP-EUR");
}

TEST(DashySymbolTest, MatchesConfigSymbol)
{
    EXPECT_EQ(Helper::dashySymbol("BTCUSD"), "BTC-USD");
    EXPECT_EQ(Helper::dashySymbol("ETHUSDT"), "ETH-USDT");
    EXPECT_EQ(Helper::dashySymbol("XRPEUR"), "XRP-EUR");
}

TEST(DashySymbolTest, SuffixEUR)
{
    EXPECT_EQ(Helper::dashySymbol("ADAEUR"), "ADA-EUR");
}

TEST(DashySymbolTest, SuffixUSDT)
{
    EXPECT_EQ(Helper::dashySymbol("SOLUSDT"), "SOL-USDT");
}

TEST(DashySymbolTest, SuffixSUSDT)
{
    EXPECT_EQ(Helper::dashySymbol("SETHSUSDT"), "SETHS-USDT");
}

TEST(DashySymbolTest, DefaultSplit)
{
    EXPECT_EQ(Helper::dashySymbol("SOLANA"), "SOL-ANA");
    EXPECT_EQ(Helper::dashySymbol("XLMXRP"), "XLM-XRP");
}

TEST(DashySymbolTest, ShortString)
{
    EXPECT_EQ(Helper::dashySymbol("BTC"), "BTC");
    EXPECT_EQ(Helper::dashySymbol(""), "");
}

TEST(DashySymbolTest, OtherSuffixes)
{
    EXPECT_EQ(Helper::dashySymbol("LTCGBP"), "LTC-GBP");
    EXPECT_EQ(Helper::dashySymbol("BNBFDUSD"), "BNB-FDUSD");
    EXPECT_EQ(Helper::dashySymbol("XTZUSDC"), "XTZ-USDC");
}

// Test fixture
class UnderlineToDashyTest : public ::testing::Test
{
};

TEST(UnderlineToDashyTest, UnderlineToDashyNormal)
{
    EXPECT_EQ(Helper::underlineToDashySymbol("BTC_USD"), "BTC-USD");
    EXPECT_EQ(Helper::underlineToDashySymbol("ETH_USDT"), "ETH-USDT");
}

TEST(UnderlineToDashyTest, UnderlineToDashyNoUnderscore)
{
    EXPECT_EQ(Helper::underlineToDashySymbol("BTCUSD"), "BTCUSD");
    EXPECT_EQ(Helper::underlineToDashySymbol(""), "");
}

TEST(UnderlineToDashyTest, UnderlineToDashyMultipleUnderscores)
{
    EXPECT_EQ(Helper::underlineToDashySymbol("BTC_USD_ETH"), "BTC-USD-ETH");
}

TEST(UnderlineToDashyTest, DashyToUnderlineNormal)
{
    EXPECT_EQ(Helper::dashyToUnderline("BTC-USD"), "BTC_USD");
    EXPECT_EQ(Helper::dashyToUnderline("ETH-USDT"), "ETH_USDT");
}

TEST(UnderlineToDashyTest, DashyToUnderlineNoDash)
{
    EXPECT_EQ(Helper::dashyToUnderline("BTCUSD"), "BTCUSD");
    EXPECT_EQ(Helper::dashyToUnderline(""), "");
}

TEST(UnderlineToDashyTest, DashyToUnderlineMultipleDashes)
{
    EXPECT_EQ(Helper::dashyToUnderline("BTC-USD-ETH"), "BTC_USD_ETH");
}

class DateDiffInDaysTest : public ::testing::Test
{
};

TEST(DateDiffInDaysTest, DateDiffNormal)
{
    using namespace std::chrono;
    auto now       = system_clock::now();
    auto yesterday = now - hours(24);
    EXPECT_EQ(Helper::dateDiffInDays(yesterday, now), 1);
    EXPECT_EQ(Helper::dateDiffInDays(now, yesterday), 1); // Absolute value
}

TEST(DateDiffInDaysTest, DateDiffSameDay)
{
    using namespace std::chrono;
    auto now = system_clock::now();
    EXPECT_EQ(Helper::dateDiffInDays(now, now), 0);
}

TEST(DateDiffInDaysTest, DateDiffMultipleDays)
{
    using namespace std::chrono;
    auto now            = system_clock::now();
    auto three_days_ago = now - hours(72); // 3 days
    EXPECT_EQ(Helper::dateDiffInDays(three_days_ago, now), 3);
}

TEST(DateDiffInDaysTest, DateDiffSmallDifference)
{
    using namespace std::chrono;
    auto now           = system_clock::now();
    auto few_hours_ago = now - hours(5); // Less than a day
    EXPECT_EQ(Helper::dateDiffInDays(few_hours_ago, now), 0);
}

// Test fixture for common setup
class ToTimestampTest : public ::testing::Test
{
   protected:
    // Unix epoch start (1970-01-01 00:00:00 UTC)
    std::chrono::system_clock::time_point epoch = std::chrono::system_clock::from_time_t(0);
};

// Basic functionality tests
TEST_F(ToTimestampTest, EpochTime)
{
    EXPECT_EQ(Helper::toTimestamp(epoch), 0);
}

TEST_F(ToTimestampTest, PositiveTime)
{
    auto time = epoch + std::chrono::seconds(3600); // 1 hour after epoch
    EXPECT_EQ(Helper::toTimestamp(time),
              3600000); // 3600 seconds * 1000 = milliseconds
}

// Edge case tests
TEST_F(ToTimestampTest, NegativeTime)
{
    auto time = epoch - std::chrono::seconds(3600); // 1 hour before epoch
    EXPECT_EQ(Helper::toTimestamp(time), -3600000);
}

TEST_F(ToTimestampTest, LargeFutureTime)
{
    auto time          = epoch + std::chrono::hours(1000000); // ~114 years in future
    long long expected = 1000000LL * 3600 * 1000;             // hours to milliseconds
    EXPECT_EQ(Helper::toTimestamp(time), expected);
}

TEST_F(ToTimestampTest, LargePastTime)
{
    auto time          = epoch - std::chrono::hours(1000000); // ~114 years in past
    long long expected = -1000000LL * 3600 * 1000;            // hours to milliseconds
    EXPECT_EQ(Helper::toTimestamp(time), expected);
}

TEST_F(ToTimestampTest, MillisecondPrecision)
{
    auto time = epoch + std::chrono::milliseconds(1500); // 1.5 seconds
    EXPECT_EQ(Helper::toTimestamp(time), 1500);
}

TEST_F(ToTimestampTest, MaximumTimePoint)
{
    auto max_time    = std::chrono::system_clock::time_point::max();
    long long result = Helper::toTimestamp(max_time);
    EXPECT_GT(result, 0); // Should handle max value without overflow
}

TEST_F(ToTimestampTest, MinimumTimePoint)
{
    auto min_time    = std::chrono::system_clock::time_point::min();
    long long result = Helper::toTimestamp(min_time);
    EXPECT_LT(result, 0); // Should handle min value without overflow
}

TEST_F(ToTimestampTest, ValidDate)
{
    long long ts = Helper::toTimestamp("2015-08-01");
    // UTC: 1438387200000 ms (adjust if toTimestamp uses different units)
    EXPECT_EQ(ts, 1438387200000); // Exact UTC timestamp in milliseconds
}

TEST_F(ToTimestampTest, EpochStart)
{
    long long ts = Helper::toTimestamp("1970-01-01");
    EXPECT_EQ(ts, 0); // UTC epoch start
}

TEST_F(ToTimestampTest, LeapYear)
{
    long long ts = Helper::toTimestamp("2020-02-29");
    EXPECT_EQ(ts, 1582934400000); // UTC timestamp for leap year
}

TEST_F(ToTimestampTest, InvalidFormat)
{
    EXPECT_THROW(Helper::toTimestamp("2020/02/29"), std::invalid_argument);
    EXPECT_THROW(Helper::toTimestamp("2020-2-29"), std::invalid_argument);
    EXPECT_THROW(Helper::toTimestamp(""), std::invalid_argument);
}

TEST_F(ToTimestampTest, InvalidDate)
{
    EXPECT_THROW(Helper::toTimestamp("2020-02-30"), std::invalid_argument);
    EXPECT_THROW(Helper::toTimestamp("2021-04-31"), std::invalid_argument);
}

// Test fixture
class DnaToHpTest : public ::testing::Test
{
};

// Normal case: int and float parameters
TEST(DnaToHpTest, NormalCase)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

    std::string dna = "AB"; // A=65, B=66
    auto hp         = Helper::dnaToHp(strategy_hp, dna);

    EXPECT_EQ(std::get< int >(hp["param1"]),
              32); // 65 scales from [40,119] to [0,100]
    EXPECT_FLOAT_EQ(std::get< float >(hp["param2"]),
                    3.2911392); // 66 scales to [0,10]
}

// Empty input (valid case)
TEST(DnaToHpTest, EmptyInput)
{
    nlohmann::json strategy_hp = nlohmann::json::array();
    std::string dna            = "";
    auto hp                    = Helper::dnaToHp(strategy_hp, dna);
    EXPECT_TRUE(hp.empty());
}

// Edge case: Min ASCII value (40)
TEST(DnaToHpTest, MinAsciiValue)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

    std::string dna = "(("; // 40, 40 (ASCII '(')
    auto hp         = Helper::dnaToHp(strategy_hp, dna);

    EXPECT_EQ(std::get< int >(hp["param1"]), 0);           // Min of range
    EXPECT_FLOAT_EQ(std::get< float >(hp["param2"]), 0.0); // Min of range
}

// Edge case: Max ASCII value (119)
TEST(DnaToHpTest, MaxAsciiValue)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

    std::string dna = "ww"; // 119, 119 (ASCII 'w')
    auto hp         = Helper::dnaToHp(strategy_hp, dna);

    EXPECT_EQ(std::get< int >(hp["param1"]), 100);          // Max of range
    EXPECT_FLOAT_EQ(std::get< float >(hp["param2"]), 10.0); // Max of range
}

// Edge case: Single character
TEST(DnaToHpTest, SingleCharacter)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": -10, "max": 10}
    ])"_json;

    std::string dna = "M"; // 77
    auto hp         = Helper::dnaToHp(strategy_hp, dna);

    EXPECT_EQ(std::get< int >(hp["param1"]),
              -1); // 77 scales from [40,119] to [-10,10]
}

// Invalid JSON: not an array
TEST(DnaToHpTest, NotAnArray)
{
    nlohmann::json strategy_hp = R"({"key": "value"})"_json;
    std::string dna            = "A";
    EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::invalid_argument);
}

// Invalid: DNA length mismatch
TEST(DnaToHpTest, LengthMismatch)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

    std::string dna = "A"; // Too short
    EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::invalid_argument);

    std::string dna_long = "ABC"; // Too long
    EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna_long), std::invalid_argument);
}

// Invalid: Missing JSON fields
TEST(DnaToHpTest, MissingFields)
{
    // Missing min
    // Missing name
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "max": 100},
        {"type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

    std::string dna = "AB";
    EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::invalid_argument);
}

// Invalid: Unsupported type
TEST(DnaToHpTest, UnsupportedType)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "string", "min": 0, "max": 100}
    ])"_json;

    std::string dna = "A";
    EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::runtime_error);
}

// Edge case: Zero range in strategy_hp (valid, but scaleToRange handles it)
TEST(DnaToHpTest, ZeroNewRange)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 5, "max": 5}
    ])"_json;

    std::string dna = "A";
    auto hp         = Helper::dnaToHp(strategy_hp, dna);
    EXPECT_EQ(std::get< int >(hp["param1"]),
              5); // Should return min (or max) due to zero range
}

// Edge case: Extreme min/max values
TEST(DnaToHpTest, ExtremeMinMax)
{
    nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "float", "min": -1e6, "max": 1e6}
    ])"_json;

    std::string dna = "A"; // 65
    auto hp         = Helper::dnaToHp(strategy_hp, dna);
    float expected  = Helper::scaleToRange(119.0f, 40.0f, 1e6f, -1e6f, 65.0f);
    EXPECT_FLOAT_EQ(std::get< float >(hp["param1"]), expected);
}

// Tests for stringAfterCharacter function
TEST(DnaToHpTest, StringAfterCharacterBasic)
{
    // Test basic functionality
    EXPECT_EQ(Helper::stringAfterCharacter("hello:world", ':'), "world");
    EXPECT_EQ(Helper::stringAfterCharacter("prefix-suffix", '-'), "suffix");
}

TEST(DnaToHpTest, StringAfterCharacterEdgeCases)
{
    // Test character not found
    EXPECT_EQ(Helper::stringAfterCharacter("hello", ':'), "");

    // Test empty string
    EXPECT_EQ(Helper::stringAfterCharacter("", ':'), "");

    // Test character at beginning
    EXPECT_EQ(Helper::stringAfterCharacter(":hello", ':'), "hello");

    // Test character at end
    EXPECT_EQ(Helper::stringAfterCharacter("hello:", ':'), "");

    // Test multiple occurrences
    EXPECT_EQ(Helper::stringAfterCharacter("first:second:third", ':'), "second:third");
}

// Test fixture
class EstimateAveragePriceTest : public ::testing::Test
{
};

// Normal case: Positive quantities
TEST(EstimateAveragePriceTest, NormalPositiveQuantities)
{
    float result = Helper::estimateAveragePrice(2.0f, 100.0f, 3.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = (200+270)/5 = 94
}

// Normal case: Negative order quantity (e.g., short position)
TEST(EstimateAveragePriceTest, NegativeOrderQuantity)
{
    float result = Helper::estimateAveragePrice(-2.0f, 100.0f, 3.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Normal case: Negative current quantity
TEST(EstimateAveragePriceTest, NegativeCurrentQuantity)
{
    float result = Helper::estimateAveragePrice(2.0f, 100.0f, -3.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Normal case: Both quantities negative
TEST(EstimateAveragePriceTest, BothQuantitiesNegative)
{
    float result = Helper::estimateAveragePrice(-2.0f, 100.0f, -3.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Edge case: Current quantity is zero
TEST(EstimateAveragePriceTest, CurrentQuantityZero)
{
    float result = Helper::estimateAveragePrice(2.0f, 100.0f, 0.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 100.0f); // (2*100 + 0*90) / (2+0) = 200/2 = 100
}

// Edge case: Order quantity is zero
TEST(EstimateAveragePriceTest, OrderQuantityZero)
{
    float result = Helper::estimateAveragePrice(0.0f, 100.0f, 3.0f, 90.0f);
    EXPECT_FLOAT_EQ(result, 90.0f); // (0*100 + 3*90) / (0+3) = 270/3 = 90
}

// Edge case: Both quantities zero
TEST(EstimateAveragePriceTest, BothQuantitiesZero)
{
    EXPECT_THROW(Helper::estimateAveragePrice(0.0f, 100.0f, 0.0f, 90.0f), std::invalid_argument);
}

// Typical trading scenario: Averaging up
TEST(EstimateAveragePriceTest, AveragingUp)
{
    float result = Helper::estimateAveragePrice(1.0f, 110.0f, 2.0f, 100.0f);
    EXPECT_FLOAT_EQ(result,
                    103.33333f); // (1*110 + 2*100) / (1+2) = 310/3 ≈ 103.333
}

// Typical trading scenario: Averaging down
TEST(EstimateAveragePriceTest, AveragingDown)
{
    float result = Helper::estimateAveragePrice(1.0f, 90.0f, 2.0f, 100.0f);
    EXPECT_FLOAT_EQ(result, 96.66667f); // (1*90 + 2*100) / (1+2) = 290/3 ≈ 96.667
}

// Edge case: Large quantities and prices
TEST(EstimateAveragePriceTest, LargeValues)
{
    float result = Helper::estimateAveragePrice(1000.0f, 5000.0f, 2000.0f, 4000.0f);
    EXPECT_FLOAT_EQ(result,
                    4333.3333f); // (1000*5000 + 2000*4000) / (1000+2000) = 13M/3
}

// Edge case: Very small quantities
TEST(EstimateAveragePriceTest, SmallQuantities)
{
    float result = Helper::estimateAveragePrice(0.001f, 100.0f, 0.002f, 90.0f);
    EXPECT_FLOAT_EQ(result, 93.33333f); // (0.001*100 + 0.002*90) / 0.003 ≈ 93.333
}

// Test fixture
class PnlUtilsTest : public ::testing::Test
{
};

// --- estimate_PNL Tests ---

TEST(PnlUtilsTest, EstimatePnlLongNoFee)
{
    float pnl = Helper::estimatePNL(2.0f, 100.0f, 110.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pnl, 20.0f); // 2 * (110 - 100) = 20
}

TEST(PnlUtilsTest, EstimatePnlShortNoFee)
{
    float pnl = Helper::estimatePNL(3.0f, 100.0f, 90.0f, Enum::TradeType::SHORT);
    EXPECT_FLOAT_EQ(pnl, 30.0f); // 3 * (90 - 100) * -1 = 30
}

TEST(PnlUtilsTest, EstimatePnlLongWithFee)
{
    float pnl = Helper::estimatePNL(2.0f, 100.0f, 110.0f, Enum::TradeType::LONG, 0.001f);
    EXPECT_FLOAT_EQ(pnl, 19.58f); // 20 - (0.001 * 2 * (100 + 110)) = 20 - 0.42
}

TEST(PnlUtilsTest, EstimatePnlShortWithFee)
{
    float pnl = Helper::estimatePNL(3.0f, 100.0f, 90.0f, Enum::TradeType::SHORT, 0.001f);
    EXPECT_FLOAT_EQ(pnl, 29.43f); // 30 - (0.001 * 3 * (100 + 90)) = 30 - 0.57
}

TEST(PnlUtilsTest, EstimatePnlNegativeQty)
{
    float pnl = Helper::estimatePNL(-2.0f, 100.0f, 110.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pnl, 20.0f); // |-2| * (110 - 100) = 20
}

TEST(PnlUtilsTest, EstimatePnlZeroQty)
{
    EXPECT_THROW(Helper::estimatePNL(0.0f, 100.0f, 110.0f, Enum::TradeType::LONG), std::invalid_argument);
}

// TEST(PnlUtilsTest, EstimatePnlInvalidTradeType)
// {
//     EXPECT_THROW(Helper::estimatePNL(2.0f, 100.0f, 110.0f, "invalid"), std::invalid_argument);
// }

TEST(PnlUtilsTest, EstimatePnlLargeValues)
{
    float pnl = Helper::estimatePNL(1000.0f, 5000.0f, 5100.0f, Enum::TradeType::LONG, 0.0001f);
    EXPECT_FLOAT_EQ(pnl, 98990.0f); // 1000 * (5100 - 5000) - 0.0001 * 1000 * (5000 + 5100)
}

TEST(PnlUtilsTest, EstimatePnlSmallValues)
{
    float pnl = Helper::estimatePNL(0.001f, 100.0f, 101.0f, Enum::TradeType::LONG, 0.001f);
    EXPECT_FLOAT_EQ(pnl, 0.000799f); // 0.001 * (101 - 100) - 0.001 * 0.001 * (100 + 101)
}

TEST(PnlUtilsTest, EstimatePnlPercentageLong)
{
    float pct = Helper::estimatePNLPercentage(2.0f, 100.0f, 110.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pct, 10.0f); // (2 * (110 - 100)) / (2 * 100) * 100 = 10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageShort)
{
    float pct = Helper::estimatePNLPercentage(3.0f, 100.0f, 90.0f, Enum::TradeType::SHORT);
    EXPECT_FLOAT_EQ(pct, 10.0f); // (3 * (90 - 100) * -1) / (3 * 100) * 100 = 10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageNegativeQty)
{
    float pct = Helper::estimatePNLPercentage(-2.0f, 100.0f, 110.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pct, 10.0f); // Same as positive qty due to abs
}

TEST(PnlUtilsTest, EstimatePnlPercentageZeroQty)
{
    EXPECT_THROW(Helper::estimatePNLPercentage(0.0f, 100.0f, 110.0f, Enum::TradeType::LONG), std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlPercentageZeroEntryPrice)
{
    EXPECT_THROW(Helper::estimatePNLPercentage(2.0f, 0.0f, 10.0f, Enum::TradeType::LONG), std::invalid_argument);
}

// TEST(PnlUtilsTest, EstimatePnlPercentageInvalidTradeType)
// {
//     EXPECT_THROW(Helper::estimatePNLPercentage(2.0f, 100.0f, 110.0f, "invalid"), std::invalid_argument);
// }

TEST(PnlUtilsTest, EstimatePnlPercentageLoss)
{
    float pct = Helper::estimatePNLPercentage(2.0f, 100.0f, 90.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pct, -10.0f); // (2 * (90 - 100)) / (2 * 100) * 100 = -10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageLargeValues)
{
    float pct = Helper::estimatePNLPercentage(1000.0f, 5000.0f, 5100.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pct,
                    2.0f); // (1000 * (5100 - 5000)) / (1000 * 5000) * 100 = 2%
}

TEST(PnlUtilsTest, EstimatePnlPercentageSmallValues)
{
    float pct = Helper::estimatePNLPercentage(0.001f, 100.0f, 101.0f, Enum::TradeType::LONG);
    EXPECT_FLOAT_EQ(pct,
                    1.0f); // (0.001 * (101 - 100)) / (0.001 * 100) * 100 = 1%
}

// FIXME:
// The observation that `max_float / 2` and `max_float / 2 + 1.0f` are equal in
// your tests stems from the limitations of floating-point precision in C++.
// Specifically, when dealing with very large numbers like
// `std::numeric_limits<float>::max()` (approximately \(3.4028235 \times
// 10^{38}\)), adding a small value like `1.0f` doesn't change the result due to
// the finite precision of the `float` type. Let's break this down:

// ### Why They Are Equal

// 1. **Floating-Point Representation**:
//    - A `float` in C++ (typically IEEE 754 single-precision) uses 32 bits: 1
//    sign bit, 8 exponent bits, and 23 mantissa bits.
//    - `std::numeric_limits<float>::max()` is \(2^{128} \times (1 + (1 -
//    2^{-23})) \approx 3.4028235 \times 10^{38}\), the largest representable
//    finite value.

// 2. **Precision Limits**:
//    - The precision of a `float` is determined by its 23-bit mantissa, which
//    provides about 6–7 decimal digits of precision.
//    - For a number as large as `max_float / 2` (around \(1.70141175 \times
//    10^{38}\)), the smallest distinguishable difference (the "unit in the last
//    place" or ULP) is much larger than `1.0f`.
//    - At this scale, the ULP is approximately \(2^{128 - 23} = 2^{105}
//    \approx 4.056 \times 10^{31}\). This means the next representable value
//    after `max_float / 2` is about \(4 \times 10^{31}` larger, far exceeding
//    `1.0f`.

// 3. **Addition Effect**:
//    - When you compute `max_float / 2 + 1.0f`, the `1.0f` is so small relative
//    to `max_float / 2` that it falls below the precision threshold (ULP).
//    - In floating-point arithmetic, if the difference between two numbers is
//    smaller than the ULP at that scale, they are rounded to the same
//    representable value.
//    - Thus, `max_float / 2 + 1.0f` gets rounded back to `max_float / 2`.

// 4. **Example Calculation**:
//    - `max_float ≈ 3.4028235e38`
//    - `max_float / 2 ≈ 1.70141175e38`
//    - ULP at this magnitude ≈ \(4.056e31\)
//    - `1.0f` is \(1.0e0\), which is orders of magnitude smaller than
//    \(4.056e31\).
//    - Adding `1.0f` doesn't shift the value enough to reach the next
//    representable `float`, so the result remains `1.70141175e38`.

// ### Demonstration
// Here's a small program to illustrate this:

// ```cpp
// #include <iostream>
// #include <limits>

// int main() {
//     float max_float = std::numeric_limits<float>::max();
//     float half_max = max_float / 2;
//     float half_max_plus_one = half_max + 1.0f;

// std::cout << "max_float: " << max_float << std::endl;
// std::cout << "half_max: " << half_max << std::endl;
// std::cout << "half_max + 1.0f: " << half_max_plus_one << std::endl;
// std::cout << "Equal? " << (half_max == half_max_plus_one ? "Yes" : "No")
// << std::endl;

// return 0;
// }
// ```

// **Output (approximate)**:
// ```
// max_float: 3.40282e+38
// half_max: 1.70141e+38
// half_max + 1.0f: 1.70141e+38
// Equal? Yes
// ```

// ### Implications for Your Tests
// In your tests (`EstimatePnlMaxFloat` and `EstimatePnlPercentageMaxFloat`),
// using `max_float / 2` and `max_float / 2 + 1.0f` as `entry_price` and
// `exit_price` results in a profit calculation of effectively zero (`exit_price
// - entry_price = 0`), because the two values are equal in `float` precision.
// This leads to:
// - `estimate_PNL`: Returns `0.0f - fee`, which isn't a meaningful test of
// large values.
// - `estimate_PNL_percentage`: Returns `0.0f`, which is correct but trivial.

// ### Fixing the Tests
// To test large values meaningfully, use a difference that exceeds the ULP at
// that scale. For example, add a value like `1e30f` (still large but within
// precision limits):

// #### Updated Test Snippet
// ```cpp
// TEST(PnlUtilsTest, EstimatePnlMaxFloat) {
//     float max_float = std::numeric_limits<float>::max();
//     float entry = max_float / 2;
//     float exit = max_float / 2 + 1e30f; // Significant difference
//     float pnl = Helper::estimate_PNL(1.0f, entry, exit, "long");
//     EXPECT_NEAR(pnl, 1e30f, 1e28f); // Approximate due to precision
// }

// TEST(PnlUtilsTest, EstimatePnlPercentageMaxFloat) {
//     float max_float = std::numeric_limits<float>::max();
//     float entry = max_float / 2;
//     float exit = max_float / 2 + 1e30f;
//     float pct = Helper::estimate_PNL_percentage(1.0f, entry, exit, "long");
//     EXPECT_NEAR(pct, (1e30f / entry) * 100.0f, 0.01f); // Percentage of entry
// }
// ```

// ### Why This Works
// - `1e30f` is smaller than the ULP at `max_float / 2` (\(4e31\)) but large
// enough to ensure a distinguishable difference in most practical ranges,
// making the test meaningful.
// - `EXPECT_NEAR` accounts for floating-point imprecision at this scale.

// ### Conclusion
// `max_float / 2` and `max_float / 2 + 1.0f` are equal due to the limited
// precision of `float`. To test edge cases with large values, use a larger
// increment (e.g., `1e30f`) that fits within the representable range and
// exceeds the ULP. Let me know if you'd like the full updated test file with
// these changes! TEST(PnlUtilsTest, EstimatePnlMaxFloat) {
//   float max_float = std::numeric_limits<float>::max();
//   float pnl =
//       Helper::estimatePNL(1.0f, max_float / 2, max_float / 2 + 1.0f, "long");
//   EXPECT_FLOAT_EQ(pnl, 1.0f); // Limited by float precision
// }

// --- estimate_PNL_percentage Tests ---
TEST(PnlUtilsTest, EstimatePnlPercentageMaxFloat)
{
    float max_float = std::numeric_limits< float >::max();
    float pct       = Helper::estimatePNLPercentage(1.0f, max_float / 2, max_float / 2 + 1.0f, Enum::TradeType::LONG);
    EXPECT_NEAR(pct, 0.0f, 0.0001f); // Small % due to float limits
}

// Test fixture to manage temporary files and directories
class FileUtilsTest : public ::testing::Test
{
   protected:
    const std::string test_file  = "test_file.txt";
    const std::string test_dir   = "test_dir";
    const std::string nested_dir = "test_dir/nested";

    void SetUp() override
    {
        // Clean up any existing test artifacts
        std::filesystem::remove_all(test_file);
        std::filesystem::remove_all(test_dir);
    }

    void TearDown() override
    {
        // Clean up after each test
        std::filesystem::remove_all(test_file);
        std::filesystem::remove_all(test_dir);
    }

    // Helper to create a file with content
    void createFile(const std::string &path, const std::string &content = "")
    {
        std::ofstream file(path);
        file << content;
        file.close();
    }
};

// --- file_exists Tests ---

TEST_F(FileUtilsTest, FileExistsTrue)
{
    createFile(test_file, "content");
    EXPECT_TRUE(Helper::fileExists(test_file));
}

TEST_F(FileUtilsTest, FileExistsFalseNonExistent)
{
    EXPECT_FALSE(Helper::fileExists(test_file));
}

TEST_F(FileUtilsTest, FileExistsFalseDirectory)
{
    std::filesystem::create_directory(test_dir);
    EXPECT_FALSE(Helper::fileExists(test_dir)); // Should return false for directories
}

TEST_F(FileUtilsTest, FileExistsEmptyPath)
{
    EXPECT_FALSE(Helper::fileExists(""));
}

// --- clear_file Tests ---

TEST_F(FileUtilsTest, ClearFileCreatesEmptyFile)
{
    Helper::clearFile(test_file);
    EXPECT_TRUE(Helper::fileExists(test_file));
    std::ifstream file(test_file);
    std::string content;
    std::getline(file, content);
    EXPECT_TRUE(content.empty());
}

TEST_F(FileUtilsTest, ClearFileOverwritesExisting)
{
    createFile(test_file, "existing content");
    Helper::clearFile(test_file);
    EXPECT_TRUE(Helper::fileExists(test_file));
    std::ifstream file(test_file);
    std::string content;
    std::getline(file, content);
    EXPECT_TRUE(content.empty());
}

TEST_F(FileUtilsTest, ClearFileEmptyPath)
{
    EXPECT_THROW(Helper::clearFile(""), std::runtime_error);
}

// Note: Testing permission-denied cases requires OS-specific setup (e.g.,
// chmod), omitted here

// --- make_directory Tests ---

TEST_F(FileUtilsTest, MakeDirectoryCreatesNew)
{
    Helper::makeDirectory(test_dir);
    EXPECT_TRUE(std::filesystem::exists(test_dir));
    EXPECT_TRUE(std::filesystem::is_directory(test_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryNested)
{
    Helper::makeDirectory(nested_dir);
    EXPECT_TRUE(std::filesystem::exists(nested_dir));
    EXPECT_TRUE(std::filesystem::is_directory(nested_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryExists)
{
    std::filesystem::create_directory(test_dir);
    Helper::makeDirectory(test_dir); // Should not throw if already exists
    EXPECT_TRUE(std::filesystem::exists(test_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryEmptyPath)
{
    EXPECT_THROW(Helper::makeDirectory(""), std::runtime_error);
}

TEST_F(FileUtilsTest, MakeDirectoryFileExists)
{
    createFile(test_file);
    EXPECT_THROW(Helper::makeDirectory(test_file), std::runtime_error);
}

// Tests for relativeToAbsolute
TEST_F(FileUtilsTest, RelativeToAbsoluteBasic)
{
    // Test current directory
    std::string current = Helper::relativeToAbsolute(".");
    EXPECT_FALSE(current.empty());
    EXPECT_TRUE(std::filesystem::exists(current));

    // Test parent directory
    std::string parent = Helper::relativeToAbsolute("..");
    EXPECT_FALSE(parent.empty());
    EXPECT_TRUE(std::filesystem::exists(parent));
}

// FIXME:
// TEST_F(FileUtilsTest, RelativeToAbsoluteEdgeCases) {
//   // Test empty path
//   EXPECT_THROW(Helper::relativeToAbsolute(""),
//                std::filesystem::filesystem_error);

// // Test non-existent path
// EXPECT_THROW(Helper::relativeToAbsolute("/nonexistent/path"),
//              std::filesystem::filesystem_error);

// // Test path with special characters
// std::string pathWithSpaces = Helper::relativeToAbsolute("path with
// spaces"); EXPECT_FALSE(pathWithSpaces.empty());
// }

// Test fixture
class RoundTests : public ::testing::Test
{
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RoundTests, FloorWithPrecisionNormal)
{
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 2), 123.45);
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 1), 123.4);
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 0), 123.0);
}

TEST_F(RoundTests, FloorWithPrecisionNegativeNumber)
{
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 2), -123.46);
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 1), -123.5);
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 0), -124.0);
}

TEST_F(RoundTests, FloorWithPrecisionZero)
{
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(0.0, 2), 0.0);
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(0.0, 0), 0.0);
}

TEST_F(RoundTests, FloorWithPrecisionHighPrecision)
{
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456789, 5), 123.45678);
}

TEST_F(RoundTests, FloorWithPrecisionNegativePrecision)
{
    EXPECT_THROW(Helper::floorWithPrecision(123.456, -1), std::invalid_argument);
}

TEST_F(RoundTests, FloorWithPrecisionLargeNumber)
{
    EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(1e10 + 0.5, 1),
                     1e10 + 0.5); // Precision exceeds double's capability
}

// Tests for roundOrNone
TEST_F(RoundTests, RoundOrNoneBasic)
{
    // Test with value
    auto result1 = Helper::round(std::optional< double >(100.123456), 2);
    EXPECT_TRUE(result1.has_value());
    EXPECT_DOUBLE_EQ(result1.value(), 100.12);

    // Test with nullopt
    auto result2 = Helper::round(std::nullopt, 2);
    EXPECT_FALSE(result2.has_value());
}

TEST_F(RoundTests, RoundOrNoneEdgeCases)
{
    // Test zero digits
    auto result1 = Helper::round(std::optional< double >(100.123456), 0);
    EXPECT_TRUE(result1.has_value());
    EXPECT_DOUBLE_EQ(result1.value(), 100.0);

    // Test negative digits
    auto result2 = Helper::round(std::optional< double >(100.123456), -1);
    EXPECT_TRUE(result2.has_value());
    EXPECT_DOUBLE_EQ(result2.value(), 100.0);

    // Test very large number
    auto result3 = Helper::round(std::optional< double >(1e20), 2);
    EXPECT_TRUE(result3.has_value());
    EXPECT_DOUBLE_EQ(result3.value(), 1e20);

    // Test very small number
    auto result4 = Helper::round(std::optional< double >(1e-20), 2);
    EXPECT_TRUE(result4.has_value());
    EXPECT_DOUBLE_EQ(result4.value(), 0);
}

// Tests for roundPriceForLiveMode
TEST_F(RoundTests, RoundPriceForLiveModeBasic)
{
    // Test basic rounding
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(100.123456, 2), 100.12);
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(100.123456, 1), 100.1);
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(100.123456, 0), 100.0);
}

TEST_F(RoundTests, RoundPriceForLiveModeEdgeCases)
{
    // Test zero precision
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(100.123456, 0), 100.0);

    // Test negative precision
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(100.123456, -1), 100.0);

    // Test very large number
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(1e20, 2), 1e20);

    // Test very small number
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(1e-20, 2), 0);

    // Test negative numbers
    EXPECT_DOUBLE_EQ(Helper::roundPriceForLiveMode(-100.123456, 2), -100.12);
}

TEST_F(RoundTests, RoundQtyForLiveMode_NormalCase)
{
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(5.6789, 2), 5.67);
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.12345, 3), 0.123);
}

TEST_F(RoundTests, RoundQtyForLiveMode_ZeroBecomesMin)
{
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.0001, 2),
                     0.01); // Rounds to 0, then to min
}

TEST_F(RoundTests, RoundQtyForLiveMode_NegativePrecision)
{
    EXPECT_THROW(Helper::roundQtyForLiveMode(5.6789, -1), std::invalid_argument);
}

TEST_F(RoundTests, RoundQtyForLiveMode_EdgeVerySmall)
{
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.00001, 5),
                     0.00001); // Min value for precision 5
}

TEST_F(RoundTests, RoundQtyForLiveModeBasic)
{
    // Test basic rounding
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(100.123456, 2), 100.12);
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(100.123456, 1), 100.1);
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(100.123456, 0), 100.0);
}

TEST_F(RoundTests, RoundQtyForLiveModeEdgeCases)
{
    // Test zero quantity
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.0, 2), 0.01);

    // Test very small quantity
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.001, 2), 0.01);

    // Test negative precision
    EXPECT_THROW(Helper::roundQtyForLiveMode(100.0, -1), std::invalid_argument);

    // FIXME:
    // Test negative quantity
    // EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(-100.123456, 2), -100.12);
}

TEST_F(RoundTests, RoundDecimalsDown_ZeroDecimals)
{
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(5.6789, 0), 5.0);
}

TEST_F(RoundTests, RoundDecimalsDown_PositiveDecimals)
{
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(5.6789, 2), 5.67);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(0.12345, 3), 0.123);
}

TEST_F(RoundTests, RoundDecimalsDown_NegativeDecimals)
{
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(123.45, -1), 120.0);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(987.65, -2), 900.0);
}

TEST_F(RoundTests, RoundDecimalsDown_EdgeCases)
{
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(0.0, 2), 0.0);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(-5.6789, 2), -5.68); // FIXME:
}

// Tests for roundDecimalsDown function
TEST_F(RoundTests, RoundDecimalsDownBasic)
{
    // Test basic rounding
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(100.123456, 2), 100.12);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(100.123456, 1), 100.1);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(100.123456, 0), 100.0);
}

TEST_F(RoundTests, RoundDecimalsDownEdgeCases)
{
    // Test negative precision
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(123.456, -1), 120.0);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(123.456, -2), 100.0);

    // Test zero
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(0.0, 2), 0.0);

    // Test negative numbers
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(-100.123456, 2), -100.13);
    EXPECT_DOUBLE_EQ(Helper::roundDecimalsDown(-123.456, -1), -130.0); // FIXME:
}

// Integration tests
TEST_F(RoundTests, IntegrationRoundingFunctions)
{
    // Test that roundQtyForLiveMode uses roundDecimalsDown correctly
    double qty    = 123.456789;
    int precision = 3;

    double expected = Helper::roundDecimalsDown(qty, precision);
    double actual   = Helper::roundQtyForLiveMode(qty, precision);

    EXPECT_DOUBLE_EQ(actual, expected);

    // Test with zero quantity
    EXPECT_NE(Helper::roundQtyForLiveMode(0.0, precision), Helper::roundDecimalsDown(0.0, precision));
    EXPECT_DOUBLE_EQ(Helper::roundQtyForLiveMode(0.0, precision), std::pow(10, -precision));
}

// Tests for doubleOrNone functions
TEST_F(RoundTests, DoubleOrNoneString)
{
    // Test valid numbers
    EXPECT_EQ(Helper::doubleOrNone("123.45"), 123.45);
    EXPECT_EQ(Helper::doubleOrNone("-123.45"), -123.45);
    EXPECT_EQ(Helper::doubleOrNone("0"), 0.0);

    // Test invalid strings
    EXPECT_EQ(Helper::doubleOrNone(""), std::nullopt);
    EXPECT_EQ(Helper::doubleOrNone("abc"), std::nullopt);
    EXPECT_EQ(Helper::doubleOrNone("123.45abc"), std::nullopt);
    EXPECT_EQ(Helper::doubleOrNone("abc123.45"), std::nullopt);

    // Test special cases
    EXPECT_EQ(Helper::doubleOrNone("inf"), std::numeric_limits< double >::infinity());
    EXPECT_TRUE(std::isnan(Helper::doubleOrNone("nan").value()));
    EXPECT_EQ(Helper::doubleOrNone("1e999"), std::nullopt);
}

TEST_F(RoundTests, DoubleOrNoneDouble)
{
    // Test valid numbers
    EXPECT_EQ(Helper::doubleOrNone(123.45), 123.45);
    EXPECT_EQ(Helper::doubleOrNone(-123.45), -123.45);
    EXPECT_EQ(Helper::doubleOrNone(0.0), 0.0);

    // Test special values
    EXPECT_EQ(Helper::doubleOrNone(std::numeric_limits< double >::infinity()),
              std::numeric_limits< double >::infinity());
    EXPECT_TRUE(std::isnan(Helper::doubleOrNone(std::numeric_limits< double >::quiet_NaN()).value()));
    EXPECT_EQ(Helper::doubleOrNone(std::numeric_limits< double >::max()), std::numeric_limits< double >::max());
    EXPECT_EQ(Helper::doubleOrNone(std::numeric_limits< double >::min()), std::numeric_limits< double >::min());
}

// Tests for strOrNone functions
TEST_F(RoundTests, StrOrNoneString)
{
    // Test valid strings
    EXPECT_EQ(Helper::strOrNone("test"), "test");
    EXPECT_EQ(Helper::strOrNone(""), "");
    EXPECT_EQ(Helper::strOrNone("123"), "123");

    // Test with different encodings
    EXPECT_EQ(Helper::strOrNone("test", "utf-8"), "test");
    EXPECT_EQ(Helper::strOrNone("test", "ascii"), "test");

    // Test with empty encoding
    EXPECT_EQ(Helper::strOrNone("test", ""), "test");
}

TEST_F(RoundTests, StrOrNoneDouble)
{
    // Test valid numbers
    EXPECT_EQ(Helper::strOrNone(123.45), "123.450000");   // FIXME:
    EXPECT_EQ(Helper::strOrNone(-123.45), "-123.450000"); // FIXME:
    EXPECT_EQ(Helper::strOrNone(0.0), "0.000000");        // FIXME:

    // Test special values
    EXPECT_EQ(Helper::strOrNone(std::numeric_limits< double >::infinity()), "inf");
    EXPECT_EQ(Helper::strOrNone(std::numeric_limits< double >::quiet_NaN()), "nan");
    EXPECT_EQ(Helper::strOrNone(std::numeric_limits< double >::max()),
              std::to_string(std::numeric_limits< double >::max()));
    EXPECT_EQ(Helper::strOrNone(std::numeric_limits< double >::min()),
              std::to_string(std::numeric_limits< double >::min()));
}

TEST_F(RoundTests, StrOrNoneCharPtr)
{
    // Test valid strings
    EXPECT_EQ(Helper::strOrNone("test"), "test");
    EXPECT_EQ(Helper::strOrNone(""), "");
    EXPECT_EQ(Helper::strOrNone("123"), "123");

    // Test nullptr
    const char *null_str = nullptr;
    EXPECT_EQ(Helper::strOrNone(null_str), std::nullopt);

    // Test with different encodings
    EXPECT_EQ(Helper::strOrNone("test", "utf-8"), "test");
    EXPECT_EQ(Helper::strOrNone("test", "ascii"), "test");
}

TEST_F(RoundTests, IntegrationDoubleAndStr)
{
    // Test conversion between double and string
    double test_value = 123.45;
    auto str_value    = Helper::strOrNone(test_value);
    auto double_value = Helper::doubleOrNone(str_value.value());

    EXPECT_TRUE(double_value.has_value());
    EXPECT_DOUBLE_EQ(double_value.value(), test_value);
}

// Tests for convertToEnvName function
TEST_F(RoundTests, ConvertToEnvNameBasic)
{
    // Test basic conversion
    EXPECT_EQ(Helper::convertToEnvName("test"), "TEST");
    EXPECT_EQ(Helper::convertToEnvName("test_name"), "TEST_NAME");
    EXPECT_EQ(Helper::convertToEnvName("test name"), "TEST_NAME");
}

TEST_F(RoundTests, ConvertToEnvNameEdgeCases)
{
    // Test empty string
    EXPECT_EQ(Helper::convertToEnvName(""), "");

    // Test already uppercase
    EXPECT_EQ(Helper::convertToEnvName("TEST"), "TEST");

    // Test mixed case
    EXPECT_EQ(Helper::convertToEnvName("Test Name"), "TEST_NAME");

    // Test special characters
    EXPECT_EQ(Helper::convertToEnvName("test@ name"), "TEST@_NAME");
    EXPECT_EQ(Helper::convertToEnvName("test! name"), "TEST!_NAME");

    // Test multiple separators
    EXPECT_EQ(Helper::convertToEnvName("test_ name"), "TEST__NAME");
    EXPECT_EQ(Helper::convertToEnvName("test_ name"), "TEST__NAME");

    // Test leading/trailing separators
    EXPECT_EQ(Helper::convertToEnvName("-test-name-"), "-TEST-NAME-");
    EXPECT_EQ(Helper::convertToEnvName("_test_name_"), "_TEST_NAME_");
}

// --- format_currency Tests ---

TEST_F(RoundTests, FormatCurrencyNormal)
{
    EXPECT_EQ(Helper::formatCurrency(1234567.89), "1,234,567.890000");
    EXPECT_EQ(Helper::formatCurrency(1000.0), "1,000.000000");
}

TEST_F(RoundTests, FormatCurrencyNegative)
{
    EXPECT_EQ(Helper::formatCurrency(-1234567.89), "-1,234,567.890000");
}

TEST_F(RoundTests, FormatCurrencyZero)
{
    EXPECT_EQ(Helper::formatCurrency(0.0), "0.000000");
}

TEST_F(RoundTests, FormatCurrencySmallNumber)
{
    EXPECT_EQ(Helper::formatCurrency(0.123456), "0.123456");
}

TEST_F(RoundTests, FormatCurrencyLargeNumber)
{
    double large = 1e12;
    EXPECT_EQ(Helper::formatCurrency(large), "1,000,000,000,000.000000");
}

TEST_F(RoundTests, FormatCurrencyMaxDouble)
{
    double max_double  = std::numeric_limits< double >::max();
    std::string result = Helper::formatCurrency(max_double);
    EXPECT_TRUE(result.find(',') != std::string::npos); // Ensure thousands separator exists
}

// Test fixture
class UUIDTest : public ::testing::Test
{
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

// --- generate_unique_id Tests ---

TEST_F(UUIDTest, GenerateUniqueIdLength)
{
    std::string id = Helper::generateUniqueId();
    EXPECT_EQ(id.length(), 36); // UUID v4: 8-4-4-4-12
}

TEST_F(UUIDTest, GenerateUniqueIdFormat)
{
    std::string id = Helper::generateUniqueId();
    std::regex uuid_regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    EXPECT_TRUE(std::regex_match(id, uuid_regex));
}

TEST_F(UUIDTest, GenerateUniqueIdUniqueness)
{
    std::set< std::string > ids;
    const int iterations = 1000;
    for (int i = 0; i < iterations; ++i)
    {
        std::string id = Helper::generateUniqueId();
        EXPECT_TRUE(ids.insert(id).second); // Ensure no duplicates
    }
}

// --- generate_short_unique_id Tests ---

TEST_F(UUIDTest, GenerateShortUniqueIdLength)
{
    std::string short_id = Helper::generateShortUniqueId();
    EXPECT_EQ(short_id.length(), 22); // First 22 chars of UUID
}

// FIXME:
// TEST_F(UUIDTest, GenerateShortUniqueIdFormat) {
//   std::string short_id = Helper::generateShortUniqueId();

// // Remove hyphens for hex validation
// std::string hex_only_id = short_id;
// hex_only_id.erase(std::remove(hex_only_id.begin(), hex_only_id.end(), '-'),
//                   hex_only_id.end());

// // Verify length (22 includes hyphens)
// EXPECT_EQ(short_id.length(), 22);

// // Verify hex characters
// std::regex short_id_regex(
//     "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{2}$");
// EXPECT_TRUE(std::regex_match(short_id, short_id_regex));

// // Verify hex-only part
// std::regex hex_regex("^[0-9a-f]+$");
// EXPECT_TRUE(std::regex_match(hex_only_id, hex_regex));
// }

TEST_F(UUIDTest, GenerateShortUniqueIdUniqueness)
{
    std::set< std::string > short_ids;
    const int iterations = 1000;
    for (int i = 0; i < iterations; ++i)
    {
        std::string short_id = Helper::generateShortUniqueId();
        EXPECT_TRUE(short_ids.insert(short_id).second); // Ensure no duplicates
    }
}

// Test fixture for UUID validation
class UUIDValidationTest : public ::testing::Test
{
   protected:
    std::string valid_uuid_v4  = "550e8400-e29b-41d4-a716-446655440000";
    std::string valid_uuid_v1  = "550e8400-e29b-11d4-a716-446655440000";
    std::string invalid_uuid   = "not-a-uuid";
    std::string empty_uuid     = "";
    std::string malformed_uuid = "550e8400-e29b-41d4-a716-44665544000"; // Missing last digit
};

TEST_F(UUIDValidationTest, ValidUUIDv4)
{
    EXPECT_TRUE(Helper::isValidUUID(valid_uuid_v4, 4));
    EXPECT_TRUE(Helper::isValidUUID(valid_uuid_v4)); // Default version is 4
}

TEST_F(UUIDValidationTest, ValidUUIDv1)
{
    EXPECT_TRUE(Helper::isValidUUID(valid_uuid_v1, 1));
    EXPECT_FALSE(Helper::isValidUUID(valid_uuid_v1, 4)); // Wrong version
}

TEST_F(UUIDValidationTest, InvalidUUID)
{
    EXPECT_FALSE(Helper::isValidUUID(invalid_uuid));
    EXPECT_FALSE(Helper::isValidUUID(empty_uuid));
    EXPECT_FALSE(Helper::isValidUUID(malformed_uuid));
}

TEST_F(UUIDValidationTest, EdgeCases)
{
    // Test with maximum length string
    std::string max_length(1000, 'a');
    EXPECT_FALSE(Helper::isValidUUID(max_length));

    // Test with special characters
    EXPECT_FALSE(Helper::isValidUUID("550e8400-e29b-41d4-a716-44665544000g"));

    // Test with wrong format
    EXPECT_FALSE(Helper::isValidUUID("550e8400e29b41d4a716446655440000")); // No dashes
}

class RandomStrTest : public ::testing::Test
{
};

// Tests for randomStr
TEST_F(RandomStrTest, RandomStrBasic)
{
    // Test default length
    std::string str1 = Helper::randomStr();
    EXPECT_EQ(str1.length(), 8);

    // Test custom length
    std::string str2 = Helper::randomStr(16);
    EXPECT_EQ(str2.length(), 16);

    // Test zero length
    std::string str3 = Helper::randomStr(0);
    EXPECT_EQ(str3.length(), 0);
}

TEST_F(RandomStrTest, RandomStrEdgeCases)
{
    // Test very large length
    std::string str1 = Helper::randomStr(1000);
    EXPECT_EQ(str1.length(), 1000);

    // Test character set
    std::string str2 = Helper::randomStr(100);
    for (char c : str2)
    {
        EXPECT_TRUE(std::isalpha(c));
    }

    // Test uniqueness (statistical)
    std::set< std::string > strings;
    for (int i = 0; i < 1000; ++i)
    {
        strings.insert(Helper::randomStr(8));
    }
    EXPECT_GT(strings.size(), 900); // Should have high uniqueness
}

TEST_F(RandomStrTest, RandomStrStress)
{
    std::set< std::string > strings;
    for (int i = 0; i < 10000; ++i)
    {
        strings.insert(Helper::randomStr(8));
    }
    EXPECT_GT(strings.size(), 9000); // Should have high uniqueness
}

// Test fixture
class TimestampToTest : public ::testing::Test
{
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

// --- timestamp_to_time_point Tests ---

TEST_F(TimestampToTest, TimestampToTimePointNormal)
{
    int64_t timestamp = 1609804800000; // 2021-01-05 00:00:00 UTC
    auto tp           = Helper::timestampToTimePoint(timestamp);
    auto duration     = tp.time_since_epoch();
    EXPECT_EQ(std::chrono::duration_cast< std::chrono::milliseconds >(duration).count(), timestamp);
}

TEST_F(TimestampToTest, TimestampToTimePointZero)
{
    auto tp = Helper::timestampToTimePoint(0);
    EXPECT_EQ(std::chrono::duration_cast< std::chrono::milliseconds >(tp.time_since_epoch()).count(), 0);
}

TEST_F(TimestampToTest, TimestampToTimePointNegative)
{
    int64_t timestamp = -31557600000; // 1969-01-01 00:00:00 UTC
    auto tp           = Helper::timestampToTimePoint(timestamp);
    EXPECT_EQ(std::chrono::duration_cast< std::chrono::milliseconds >(tp.time_since_epoch()).count(), timestamp);
}

// --- timestamp_to_date Tests ---

TEST_F(TimestampToTest, TimestampToDateNormal)
{
    EXPECT_EQ(Helper::timestampToDate(1609804800000), "2021-01-05");
}

TEST_F(TimestampToTest, TimestampToDateZero)
{
    EXPECT_EQ(Helper::timestampToDate(0), "1970-01-01");
}

// FIXME:
// TEST_F(TimestampToTest, TimestampToDateNegative) {
//   EXPECT_EQ(Helper::timestampToDate(-31557600000), "1969-01-01");
// }

TEST_F(TimestampToTest, TimestampToDateLarge)
{
    EXPECT_EQ(Helper::timestampToDate(4102444800000),
              "2100-01-01"); // Far future
}

// --- timestamp_to_time Tests ---

TEST_F(TimestampToTest, TimestampToTimeNormal)
{
    EXPECT_EQ(Helper::timestampToTime(1609804800000), "2021-01-05 00:00:00");
}

TEST_F(TimestampToTest, TimestampToTimeWithMs)
{
    EXPECT_EQ(Helper::timestampToTime(1609804800123),
              "2021-01-05 00:00:00"); // Ms truncated
}

TEST_F(TimestampToTest, TimestampToTimeZero)
{
    EXPECT_EQ(Helper::timestampToTime(0), "1970-01-01 00:00:00");
}

// FIXME:
// TEST_F(TimestampToTest, TimestampToTimeNegative) {
//   EXPECT_EQ(Helper::timestampToTime(-31557600000), "1969-01-01 00:00:00");
// }

// --- timestamp_to_iso8601 Tests ---

TEST_F(TimestampToTest, TimestampToIso8601Normal)
{
    EXPECT_EQ(Helper::timestampToIso8601(1609804800000), "2021-01-05T00:00:00.000000.000Z");
}

// FIXME:
// TEST_F(TimestampToTest, TimestampToIso8601WithMs) {
//   EXPECT_EQ(Helper::timestampToIso8601(1609804800123),
//             "2021-01-05T00:00:00.000000.123Z");
// }

TEST_F(TimestampToTest, TimestampToIso8601Zero)
{
    EXPECT_EQ(Helper::timestampToIso8601(0), "1970-01-01T00:00:00.000000.000Z");
}

// FIXME:
// TEST_F(TimestampToTest, TimestampToIso8601Negative) {
//   EXPECT_EQ(Helper::timestampToIso8601(-31557600000),
//             "1969-01-01T00:00:00.000000.000Z");
// }

// FIXME:
// TEST_F(TimestampToTest, TimestampToIso8601Large) {
//   EXPECT_EQ(Helper::timestampToIso8601(4102444800123),
//             "2100-01-01T00:00:00.123Z");
// }

// --- iso8601_to_timestamp Tests ---

TEST_F(TimestampToTest, Iso8601ToTimestampNormal)
{
    EXPECT_EQ(Helper::iso8601ToTimestamp("2021-01-05T00:00:00.000Z"), 1609804800000);
}

TEST_F(TimestampToTest, Iso8601ToTimestampWithMs)
{
    EXPECT_EQ(Helper::iso8601ToTimestamp("2021-01-05T00:00:00.123Z"), 1609804800123);
}

TEST_F(TimestampToTest, Iso8601ToTimestampZero)
{
    EXPECT_EQ(Helper::iso8601ToTimestamp("1970-01-01T00:00:00.000Z"), 0);
}

// FIXME:
// TEST_F(TimestampToTest, Iso8601ToTimestampNegative) {
//   EXPECT_EQ(Helper::iso8601ToTimestamp("1969-01-01T00:00:00.000Z"),
//             -31557600000);
// }

TEST_F(TimestampToTest, Iso8601ToTimestampInvalidFormat)
{
    EXPECT_THROW(Helper::iso8601ToTimestamp("2021-01-05"), std::invalid_argument);
    EXPECT_THROW(Helper::iso8601ToTimestamp("2021-01-05T00:00:00"),
                 std::invalid_argument); // No Z
    EXPECT_THROW(Helper::iso8601ToTimestamp("invalid"), std::invalid_argument);
}

// --- today_to_timestamp Tests ---

// FIXME:
// TEST_F(TimestampToTest, TodayToTimestampBasic) {
//   int64_t ts = Helper::todayToTimestamp();
//   std::string date_str = Helper::timestampToDate(ts);
//   EXPECT_EQ(date_str.substr(8, 2), "00"); // Should be start of day
//   auto tp = Helper::timestampToTimePoint(ts);
//   auto time = date::format("%T", tp);
//   EXPECT_EQ(time, "00:00:00"); // Verify midnight
// }

TEST_F(TimestampToTest, TodayToTimestampConsistency)
{
    int64_t ts1 = Helper::todayToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int64_t ts2 = Helper::todayToTimestamp();
    EXPECT_EQ(ts1, ts2); // Should be same day start despite small delay
}

class NowTimestampDateTimeTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Reset any cached timestamps before each test
        Helper::nowToTimestamp(true);
    }

    void TearDown() override
    {
        // Clean up after each test
    }
};

// Tests for nowToTimestamp
TEST_F(NowTimestampDateTimeTest, NowToTimestampBasic)
{
    int64_t ts = Helper::nowToTimestamp();
    EXPECT_GT(ts, 0); // Should be positive
    EXPECT_LE(
        ts,
        std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
            .count()); // Should not be in future
}

TEST_F(NowTimestampDateTimeTest, NowToTimestampForceFresh)
{
    int64_t ts1 = Helper::nowToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t ts2 = Helper::nowToTimestamp(true);
    EXPECT_GT(ts2, ts1); // Forced fresh should be newer
}

TEST_F(NowTimestampDateTimeTest, NowToTimestampConsistency)
{
    int64_t ts1 = Helper::nowToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t ts2 = Helper::nowToTimestamp();
    EXPECT_EQ(ts1, ts2); // Without force_fresh, should be same
}

TEST_F(NowTimestampDateTimeTest, NowToTimestampLiveTrading)
{
    // Set up live trading mode
    setenv("APP_TRADING_MODE", "livetrade", 1);
    Config::Config::getInstance().reload();

    int64_t ts1 = Helper::nowToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t ts2 = Helper::nowToTimestamp();
    EXPECT_GT(ts2, ts1); // In live mode, should always be fresh

    // Clean up
    unsetenv("APP_TRADING_MODE");
    Config::Config::getInstance().reload();
}

TEST_F(NowTimestampDateTimeTest, NowToTimestampImportingCandles)
{
    // Set up importing candles mode
    setenv("APP_TRADING_MODE", "candles", 1);
    Config::Config::getInstance().reload();

    int64_t ts1 = Helper::nowToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t ts2 = Helper::nowToTimestamp();
    EXPECT_GT(ts2, ts1); // In importing mode, should always be fresh

    // Clean up
    unsetenv("APP_TRADING_MODE");
    Config::Config::getInstance().reload();
}

TEST_F(NowTimestampDateTimeTest, NowToTimestampBacktesting)
{
    // Set up backtesting mode
    setenv("APP_TRADING_MODE", "backtest", 1);
    Config::Config::getInstance().reload();

    int64_t ts1 = Helper::nowToTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int64_t ts2 = Helper::nowToTimestamp();
    EXPECT_EQ(ts1, ts2); // In backtest mode, should use cached time

    // Clean up
    unsetenv("APP_TRADING_MODE");
    Config::Config::getInstance().reload();
}

// Tests for nowToDateTime
TEST_F(NowTimestampDateTimeTest, NowToDateTimeBasic)
{
    auto dt = Helper::nowToDateTime();
    EXPECT_GT(dt.time_since_epoch().count(), 0); // Should be positive
    EXPECT_LE(dt.time_since_epoch().count(),
              std::chrono::system_clock::now().time_since_epoch().count()); // Should not be in future
}

TEST_F(NowTimestampDateTimeTest, NowToDateTimeConsistency)
{
    auto dt1 = Helper::nowToDateTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto dt2 = Helper::nowToDateTime();
    EXPECT_GT(dt2.time_since_epoch().count(),
              dt1.time_since_epoch().count()); // Should be newer
}

TEST_F(NowTimestampDateTimeTest, NowToDateTimePrecision)
{
    auto dt1 = Helper::nowToDateTime();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    auto dt2 = Helper::nowToDateTime();
    EXPECT_GT(dt2.time_since_epoch().count(),
              dt1.time_since_epoch().count()); // Should detect microsecond changes
}

TEST_F(NowTimestampDateTimeTest, NowToDateTimeSystemTimeChange)
{
    auto dt1 = Helper::nowToDateTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto dt2  = Helper::nowToDateTime();
    auto diff = std::chrono::duration_cast< std::chrono::milliseconds >(dt2 - dt1).count();
    EXPECT_GE(diff, 10); // Should reflect actual time difference
}

TEST_F(NowTimestampDateTimeTest, NowToDateTimeHighPrecision)
{
    auto dt1 = Helper::nowToDateTime();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    auto dt2  = Helper::nowToDateTime();
    auto diff = std::chrono::duration_cast< std::chrono::microseconds >(dt2 - dt1).count();
    EXPECT_GE(diff, 100); // Should have microsecond precision
}

// Edge cases and stress tests
TEST_F(NowTimestampDateTimeTest, NowToTimestampStress)
{
    const int iterations = 1000;
    std::vector< int64_t > timestamps;
    timestamps.reserve(iterations);

    for (int i = 0; i < iterations; ++i)
    {
        timestamps.push_back(Helper::nowToTimestamp(true));
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // Verify monotonic increase
    for (size_t i = 1; i < timestamps.size(); ++i)
    {
        EXPECT_GE(timestamps[i], timestamps[i - 1]);
    }
}

TEST_F(NowTimestampDateTimeTest, NowToDateTimeStress)
{
    const int iterations = 1000;
    std::vector< std::chrono::system_clock::time_point > times;
    times.reserve(iterations);

    for (int i = 0; i < iterations; ++i)
    {
        times.push_back(Helper::nowToDateTime());
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // Verify monotonic increase
    for (size_t i = 1; i < times.size(); ++i)
    {
        EXPECT_GE(times[i].time_since_epoch().count(), times[i - 1].time_since_epoch().count());
    }
}

class ReadableDurationTest : public ::testing::Test
{
};

// Tests for readableDuration
TEST_F(ReadableDurationTest, ReadableDurationBasic)
{
    // Test single unit
    EXPECT_EQ(Helper::readableDuration(60, 1), "1 minute");
    EXPECT_EQ(Helper::readableDuration(3600, 1), "1 hour");
    EXPECT_EQ(Helper::readableDuration(86400, 1), "1 day");

    // Test multiple units
    EXPECT_EQ(Helper::readableDuration(3661, 2), "1 hour, 1 minute");
    EXPECT_EQ(Helper::readableDuration(86461, 2), "1 day, 1 minute");
}

TEST_F(ReadableDurationTest, ReadableDurationEdgeCases)
{
    // Test zero seconds
    EXPECT_EQ(Helper::readableDuration(0, 2), "");

    // FIXME:
    // Test negative seconds
    // EXPECT_EQ(Helper::readableDuration(-60, 1), "1 minute");

    // Test very large duration
    EXPECT_EQ(Helper::readableDuration(604800 * 2 + 86400 * 3 + 3600 * 4 + 60 * 5 + 6, 5),
              "2 weeks, 3 days, 4 hours, 5 minutes, 6 seconds");

    // Test granularity larger than available units
    EXPECT_EQ(Helper::readableDuration(60, 10), "1 minute");

    // Test granularity of 1
    EXPECT_EQ(Helper::readableDuration(3661, 1), "1 hour");
}

// Helper function to create a strategy file
void createStrategyFile(const fs::path &sourcePath, const std::string &className)
{
    std::ofstream outFile(sourcePath);
    ASSERT_TRUE(outFile) << "Failed to create " << sourcePath;
    outFile << R"(
#include "Helper.hpp"

namespace YourStrategy {
    class )" << className
            << R"( : public Helper::Strategy {
    public:
        void execute() override {}
    };
}

extern "C" Helper::Strategy* createStrategy() {
    return new YourStrategy::)"
            << className << R"(();
}
)";
    outFile.close();
}

// Helper function to compile a strategy into .so
void compileStrategy(const fs::path &srcPath,
                     const fs::path &outputPath,
                     const fs::path &includePath,
                     const fs::path &libraryPath)
{
    // -lmy_trading_lib
    std::string cmd = "g++ -shared -pthread -ldl -lssl -lcrypto -fPIC -std=c++17 -I" + includePath.string() +
                      " -I/opt/homebrew/include -L" + libraryPath.string() + " -L/opt/homebrew/lib -o" +
                      outputPath.string() + " " + srcPath.string();
    int result = system(cmd.c_str());
    if (result != 0)
    {
        std::cerr << "Compilation failed with code " << result << "\n";
        ASSERT_EQ(result, 0) << "Compilation failed for " << srcPath;
    }
    if (!fs::exists(outputPath))
    {
        std::cerr << "Output .so not created: " << outputPath << "\n";
        ASSERT_TRUE(fs::exists(outputPath)) << "Output .so not created";
    }
    // ASSERT_EQ(result, 0) << "Compilation failed for " << srcPath;
    // ASSERT_TRUE(fs::exists(outputPath))
    //     << "Output .so not created: " << outputPath;
}

// Get the project directory dynamically
fs::path getProjectDir()
{
    // Use the source file's path (__FILE__) to locate projectDir
    fs::path sourcePath = fs::canonical(__FILE__);                // Absolute path to this .cpp file
    fs::path projectDir = sourcePath.parent_path().parent_path(); // Assumes projectDir/build/
    if (!fs::exists(projectDir / "include") || !fs::exists(projectDir / "src"))
    {
        std::cerr << "Error: include/ or src/ not found in " << projectDir << "\n";
        throw std::runtime_error("Invalid project directory");
    }
    return projectDir;
}

///////////////////////////////////////////////////////////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// NOTE: Following tests take too much time to execute.
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
///////////////////////////////////////////////////////////

class StrategyLoaderTest : public ::testing::Test
{
   protected:
    Helper::StrategyLoader &loader = Helper::StrategyLoader::getInstance();
    fs::path tempDir               = fs::temp_directory_path() / "strategy_test";
    fs::path srcPath               = tempDir / "src";
    fs::path libraryPath           = tempDir / "lib";
    fs::path includePath           = tempDir / "include";
    fs::path strategiesDir         = tempDir / "strategies";

    void SetUp() override
    {
        fs::create_directories(srcPath);
        fs::create_directories(libraryPath);
        fs::create_directories(includePath);
        fs::create_directories(strategiesDir);

        fs::path projectDir = getProjectDir();

        fs::copy(
            projectDir / "include", includePath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

        fs::copy(projectDir / "src", srcPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        // Since Helper.cpp uses dlopen/dlsym (for loading .so files), ensure -ldl
        // is included
        // Add -O2 or -O3 for performance in a production-like test setup
        //
        std::string libCmd = "g++ -shared -pthread -ldl -lssl -lcrypto -fPIC -std=c++17 -I" + includePath.string() +
                             " -I/opt/homebrew/include" + " -L/opt/homebrew/lib -o " +
                             (libraryPath / "libciphertrader.so").string() + " " + srcPath.string() + "/*";
        system(libCmd.c_str());

        loader.setBasePath(tempDir);
        loader.setIncludePath(includePath);
        loader.setLibraryPath(libraryPath);
        loader.setTestingMode(false);
    }

    void TearDown() override { fs::remove_all(tempDir); }

    std::optional< std::filesystem::path > resolveModulePath(const std::string &name) const
    {
        return loader.resolveModulePath(name);
    }

    std::pair< std::unique_ptr< Helper::Strategy >, void * > loadFromDynamicLib(const std::filesystem::path &path) const
    {
        return loader.loadFromDynamicLib(path);
    }

    std::pair< std::unique_ptr< Helper::Strategy >, void * > adjustAndReload(
        const std::string &name, const std::filesystem::path &sourcePath) const
    {
        return loader.adjustAndReload(name, sourcePath);
    }

    std::pair< std::unique_ptr< Helper::Strategy >, void * > createFallback(
        const std::string &name, const std::filesystem::path &modulePath) const
    {
        return loader.createFallback(name, modulePath);
    }
};

// --- Singleton Tests ---
TEST_F(StrategyLoaderTest, InstanceReturnsSameObject)
{
    auto &loader1 = Helper::StrategyLoader::getInstance();
    auto &loader2 = Helper::StrategyLoader::getInstance();
    EXPECT_EQ(&loader1, &loader2);
}

// --- getStrategy Tests ---
TEST_F(StrategyLoaderTest, GetStrategyValid)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::path soPath     = strategiesDir / "TestStrategy" / "TestStrategy.so";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "TestStrategy");
    compileStrategy(sourcePath, soPath, includePath, libraryPath);

    auto [strategy, handle] = loader.getStrategy("TestStrategy");
    if (!strategy)
    {
        if (handle)
        {
            dlclose(handle);
        }

        FAIL() << "Strategy loading failed";
    }

    EXPECT_NE(strategy, nullptr);
}

TEST_F(StrategyLoaderTest, GetStrategyEmptyName)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
    EXPECT_THROW(loader.getStrategy(""), std::invalid_argument);
#pragma clang diagnostic pop
}

TEST_F(StrategyLoaderTest, GetStrategyMissing)
{
    auto [strategy, handle] = loader.getStrategy("NonExistent");
    EXPECT_EQ(strategy, nullptr);
}

// --- resolveModulePath Tests ---
TEST_F(StrategyLoaderTest, ResolveModulePathNonTesting)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::path soPath     = strategiesDir / "TestStrategy" / "TestStrategy.so";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "TestStrategy");
    compileStrategy(sourcePath, soPath, includePath, libraryPath);

    auto path = resolveModulePath("TestStrategy");
    EXPECT_TRUE(path.has_value());
    EXPECT_EQ(*path, soPath);
}

TEST_F(StrategyLoaderTest, ResolveModulePathTestingLive)
{
    loader.setTestingMode(true);
    fs::create_directories(tempDir / "tests" / "strategies" / "TestStrategy");
    fs::path soPath = tempDir / "tests" / "strategies" / "TestStrategy" / "TestStrategy.so";
    createStrategyFile(tempDir / "tests" / "strategies" / "TestStrategy" / "main.cpp", "TestStrategy");
    compileStrategy(tempDir / "tests" / "strategies" / "TestStrategy" / "main.cpp", soPath, includePath, libraryPath);

    // fs::rename(tempDir, tempDir.parent_path() / "ciphertrader-live");
    //
    // Remove existing ciphertrader-live if it exists
    fs::path newDir = tempDir.parent_path() / "ciphertrader-live";
    if (fs::exists(newDir))
    {
        fs::remove_all(newDir);
    }
    fs::rename(tempDir, newDir);
    auto soPath2 =
        std::filesystem::path("ciphertrader-live") / "tests" / "strategies" / "TestStrategy" / "TestStrategy.so";
    loader.setBasePath(tempDir.parent_path() / "ciphertrader-live");
    auto path = resolveModulePath("TestStrategy");

    EXPECT_TRUE(path.has_value());
    EXPECT_TRUE(Helper::endsWith((*path).string(), soPath2.string()));
}

TEST_F(StrategyLoaderTest, ResolveModulePathInvalid)
{
    auto path = resolveModulePath("NonExistent");
    EXPECT_FALSE(path.has_value());
}

// --- loadFromDynamicLib Tests ---
TEST_F(StrategyLoaderTest, LoadFromDynamicLibValid)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::path soPath     = strategiesDir / "TestStrategy" / "TestStrategy.so";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "TestStrategy");
    compileStrategy(sourcePath, soPath, includePath, libraryPath);

    auto [strategy, handle] = loadFromDynamicLib(soPath);
    EXPECT_NE(strategy, nullptr) << "Failed to load valid strategy from " << soPath;
}

TEST_F(StrategyLoaderTest, LoadFromDynamicLibInvalidPath)
{
    auto [strategy, handle] = loadFromDynamicLib("invalid.so");
    EXPECT_EQ(strategy, nullptr) << "Expected nullptr for invalid .so path";
    if (handle)
    {
        dlclose(handle);
    }
}

// --- adjustAndReload Tests ---
TEST_F(StrategyLoaderTest, AdjustAndReloadRenamesClass)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::path soPath     = strategiesDir / "TestStrategy" / "TestStrategy.so";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "OldStrategy"); // Different name

    auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
    EXPECT_NE(strategy, nullptr);

    std::ifstream updatedFile(sourcePath);
    std::string content((std::istreambuf_iterator< char >(updatedFile)), std::istreambuf_iterator< char >());
    EXPECT_TRUE(content.find("class OldStrategy") != std::string::npos);
    EXPECT_TRUE(fs::exists(soPath));
}

TEST_F(StrategyLoaderTest, AdjustAndReloadNoChangeNeeded)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "TestStrategy"); // Same name
    auto originalModTime = fs::last_write_time(sourcePath);

    auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
    EXPECT_EQ(strategy, nullptr); // No reload needed, returns nullptr
    EXPECT_EQ(fs::last_write_time(sourcePath),
              originalModTime); // File unchanged
    if (handle)
    {
        dlclose(handle);
    }
}

// --- createFallback Tests ---
TEST_F(StrategyLoaderTest, CreateFallbackValid)
{
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::path soPath     = strategiesDir / "TestStrategy" / "TestStrategy.so";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "OldStrategy"); // Different name
    compileStrategy(sourcePath, soPath, includePath, libraryPath);

    auto [strategy, handle] = createFallback("TestStrategy", soPath);
    EXPECT_NE(strategy, nullptr);
}

TEST_F(StrategyLoaderTest, CreateFallbackInvalid)
{
    auto [strategy, handle] = createFallback("TestStrategy", "invalid.so");
    EXPECT_EQ(strategy, nullptr);
    if (handle)
    {
        dlclose(handle);
    }
}

// FIXME:
// --- Edge Cases ---
// TEST_F(StrategyLoaderTest, EdgeCaseLongName) {
//   std::string longName(100, 'a'); // Very long strategy name
// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wunused-result"
//   EXPECT_THROW(loader.getStrategy(longName),
//                std::invalid_argument); // May fail due to filesystem limits
// #pragma clang diagnostic pop
// }

TEST_F(StrategyLoaderTest, EdgeCaseInvalidPathCharacters)
{
    fs::path sourcePath = strategiesDir / "Test/Strategy" / "main.cpp";
    EXPECT_THROW(fs::create_directory(strategiesDir / "Test/Strategy"), std::filesystem::filesystem_error);
    // Do not proceed with further operations since the directory creation fails

    // createStrategyFile(sourcePath, "TestStrategy");
    // auto [strategy, handle] =
    //     loader.getStrategy("Test/Strategy"); // Invalid path character
    // EXPECT_EQ(strategy, nullptr);            // Should fail gracefully
}

TEST_F(StrategyLoaderTest, EdgeCaseNoLib)
{
    fs::remove(libraryPath / "libmy_trading_lib.so");
    fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
    fs::create_directory(strategiesDir / "TestStrategy");
    createStrategyFile(sourcePath, "TestStrategy");
    auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
    EXPECT_EQ(strategy, nullptr); // Compilation should fail without lib
    if (handle)
    {
        dlclose(handle);
    }
}

///////////////////////////////////////////////////////////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// ------------------ Until ------------------ ///////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
/////// ------------------------------------------- ///////
///////////////////////////////////////////////////////////

// Test fixture for secure hash computations
class ComputeSecureHashTest : public ::testing::Test
{
   protected:
    // Helper function to verify hash format
    bool isValidHashFormat(const std::string &hash)
    {
        // SHA-256 produces a 32-byte (64 hex character) hash
        if (hash.length() != 64)
        {
            return false;
        }

        // Check if all characters are valid hex
        for (char c : hash)
        {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            {
                return false;
            }
        }
        return true;
    }
};

// Basic functionality tests for computeSecureHash
TEST_F(ComputeSecureHashTest, BasicFunctionality)
{
    std::string hash1 = Helper::computeSecureHash("test");
    std::string hash2 = Helper::computeSecureHash("different");

    // Check valid format
    EXPECT_TRUE(isValidHashFormat(hash1));
    EXPECT_TRUE(isValidHashFormat(hash2));

    // Check deterministic behavior
    EXPECT_EQ(hash1, Helper::computeSecureHash("test"));

    // Check different strings produce different hashes
    EXPECT_NE(hash1, hash2);
}

TEST_F(ComputeSecureHashTest, KnownValues)
{
    // Known SHA-256 hash values
    EXPECT_EQ(Helper::computeSecureHash(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(Helper::computeSecureHash("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// Edge case tests for computeSecureHash
TEST_F(ComputeSecureHashTest, EdgeCases)
{
    // Empty string
    std::string emptyHash = Helper::computeSecureHash("");
    EXPECT_TRUE(isValidHashFormat(emptyHash));

    // Long string
    std::string longString(10000, 'a');
    std::string longHash = Helper::computeSecureHash(longString);
    EXPECT_TRUE(isValidHashFormat(longHash));

    // String with special characters
    std::string specialChars = "!@#$%^&*()_+{}|:<>?[]\\;',./";
    std::string specialHash  = Helper::computeSecureHash(specialChars);
    EXPECT_TRUE(isValidHashFormat(specialHash));

    // String with null characters
    std::string_view nullView("\0test\0", 6);
    std::string nullHash = Helper::computeSecureHash(nullView);
    EXPECT_TRUE(isValidHashFormat(nullHash));

    // Unicode string
    std::string unicode     = "こんにちは世界";
    std::string unicodeHash = Helper::computeSecureHash(unicode);
    EXPECT_TRUE(isValidHashFormat(unicodeHash));
}

// Test fixture for insertList function
class InsertListTest : public ::testing::Test
{
   protected:
    std::vector< int > intList{1, 2, 3, 4, 5};
    std::vector< std::string > stringList{"one", "two", "three"};
};

// Basic functionality tests for insertList
TEST_F(InsertListTest, InsertAtBeginning)
{
    auto result = Helper::insertList(0, 0, intList);
    EXPECT_EQ(result.size(), intList.size() + 1);
    EXPECT_EQ(result[0], 0);
    for (size_t i = 0; i < intList.size(); i++)
    {
        EXPECT_EQ(result[i + 1], intList[i]);
    }
}

TEST_F(InsertListTest, InsertInMiddle)
{
    auto result = Helper::insertList(2, 99, intList);
    EXPECT_EQ(result.size(), intList.size() + 1);
    EXPECT_EQ(result[0], intList[0]);
    EXPECT_EQ(result[1], intList[1]);
    EXPECT_EQ(result[2], 99);
    EXPECT_EQ(result[3], intList[2]);
    EXPECT_EQ(result[4], intList[3]);
    EXPECT_EQ(result[5], intList[4]);
}

TEST_F(InsertListTest, InsertAtEnd)
{
    auto result = Helper::insertList(intList.size(), 6, intList);
    EXPECT_EQ(result.size(), intList.size() + 1);
    for (size_t i = 0; i < intList.size(); i++)
    {
        EXPECT_EQ(result[i], intList[i]);
    }
    EXPECT_EQ(result.back(), 6);
}

TEST_F(InsertListTest, AppendUsingSpecialIndex)
{
    // Test the special -1 index that appends
    auto result = Helper::insertList(static_cast< size_t >(-1), 6, intList);
    EXPECT_EQ(result.size(), intList.size() + 1);
    for (size_t i = 0; i < intList.size(); i++)
    {
        EXPECT_EQ(result[i], intList[i]);
    }
    EXPECT_EQ(result.back(), 6);
}

// Edge case tests for insertList
TEST_F(InsertListTest, InsertIntoEmptyVector)
{
    std::vector< int > empty;
    auto result = Helper::insertList(0, 42, empty);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 42);

    // Special -1 index with empty vector
    auto result2 = Helper::insertList(static_cast< size_t >(-1), 42, empty);
    EXPECT_EQ(result2.size(), 1);
    EXPECT_EQ(result2[0], 42);
}

TEST_F(InsertListTest, IndexOutOfBounds)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
    // Index beyond vector size should throw an exception
    EXPECT_THROW(Helper::insertList(intList.size() + 1, 99, intList), std::out_of_range);

    // Multiple positions beyond the end should also throw
    EXPECT_THROW(Helper::insertList(intList.size() + 5, 99, intList), std::out_of_range);

    // Test with empty vector
    std::vector< int > empty;
    EXPECT_THROW(Helper::insertList(1, 42, empty), std::out_of_range);
#pragma clang diagnostic pop
}

// Add a test for the boundary case - inserting at exactly the end of the
// vector
TEST_F(InsertListTest, InsertAtExactEndOfVector)
{
    auto result = Helper::insertList(intList.size(), 99, intList);
    EXPECT_EQ(result.size(), intList.size() + 1);

    // Check that all original elements are preserved
    for (size_t i = 0; i < intList.size(); i++)
    {
        EXPECT_EQ(result[i], intList[i]);
    }

    // The new element should be at the end
    EXPECT_EQ(result.back(), 99);
}

TEST_F(InsertListTest, ComplexTypes)
{
    // Test with string vector
    auto strResult = Helper::insertList(1, std::string("inserted"), stringList);
    EXPECT_EQ(strResult.size(), stringList.size() + 1);
    EXPECT_EQ(strResult[0], stringList[0]);
    EXPECT_EQ(strResult[1], "inserted");
    EXPECT_EQ(strResult[2], stringList[1]);
    EXPECT_EQ(strResult[3], stringList[2]);

    // Test with a custom class/struct
    std::vector< std::pair< int, std::string > > pairs = {{1, "one"}, {2, "two"}, {3, "three"}};

    std::pair< int, std::string > newItem{4, "four"};
    auto res = Helper::insertList(1, newItem, pairs);
    EXPECT_EQ(res.size(), pairs.size() + 1);
    EXPECT_EQ(res[0], pairs[0]);
    EXPECT_EQ(res[1], newItem);
    EXPECT_EQ(res[2], pairs[1]);
    EXPECT_EQ(res[3], pairs[2]);
}

class MergeMapTest : public ::testing::Test
{
   protected:
    // Common test maps
    std::map< std::string, int > map1_int;
    std::map< std::string, int > map2_int;
    std::map< std::string, std::string > map1_str;
    std::map< std::string, std::string > map2_str;
    std::map< std::string, std::map< std::string, int > > nested_map1;
    std::map< std::string, std::map< std::string, int > > nested_map2;

    void SetUp() override
    {
        // Initialize basic maps
        map1_int = {{"a", 1}, {"b", 2}};
        map2_int = {{"b", 3}, {"c", 4}};
        map1_str = {{"x", "one"}, {"y", "two"}};
        map2_str = {{"y", "three"}, {"z", "four"}};

        // Initialize nested maps
        nested_map1 = {{"outer1", {{"inner1", 1}, {"inner2", 2}}}, {"outer2", {{"inner3", 3}}}};
        nested_map2 = {{"outer1", {{"inner2", 4}, {"inner3", 5}}}, {"outer3", {{"inner4", 6}}}};
    }
};

// Test basic merging with integer values
TEST_F(MergeMapTest, BasicIntegerMerge)
{
    auto result = Helper::mergeMaps(map1_int, map2_int);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["a"], 1); // From map1
    EXPECT_EQ(result["b"], 3); // Overwritten by map2
    EXPECT_EQ(result["c"], 4); // From map2
}

// Test basic merging with string values
TEST_F(MergeMapTest, BasicStringMerge)
{
    auto result = Helper::mergeMaps(map1_str, map2_str);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["x"], "one");   // From map1
    EXPECT_EQ(result["y"], "three"); // Overwritten by map2
    EXPECT_EQ(result["z"], "four");  // From map2
}

// Test nested map merging
TEST_F(MergeMapTest, NestedMapMerge)
{
    auto result = Helper::mergeMaps(nested_map1, nested_map2);

    EXPECT_EQ(result.size(), 3);

    // Check outer1 merged contents
    EXPECT_EQ(result["outer1"]["inner1"], 1); // From nested_map1
    EXPECT_EQ(result["outer1"]["inner2"], 4); // Overwritten by nested_map2
    EXPECT_EQ(result["outer1"]["inner3"], 5); // From nested_map2

    // Check other keys
    EXPECT_EQ(result["outer2"]["inner3"], 3); // From nested_map1
    EXPECT_EQ(result["outer3"]["inner4"], 6); // From nested_map2
}

// Test merging empty maps
TEST_F(MergeMapTest, EmptyMapMerge)
{
    std::map< std::string, int > empty_map;

    // Empty + Non-empty
    auto result1 = Helper::mergeMaps(empty_map, map1_int);
    EXPECT_EQ(result1, map1_int);

    // Non-empty + Empty
    auto result2 = Helper::mergeMaps(map1_int, empty_map);
    EXPECT_EQ(result2, map1_int);

    // Empty + Empty
    auto result3 = Helper::mergeMaps(empty_map, empty_map);
    EXPECT_TRUE(result3.empty());
}

// Test merging with different value types
TEST_F(MergeMapTest, DifferentValueTypes)
{
    std::map< std::string, std::variant< int, std::string > > map1 = {{"a", 1}, {"b", "hello"}};
    std::map< std::string, std::variant< int, std::string > > map2 = {{"b", 2}, {"c", "world"}};

    auto result = Helper::mergeMaps(map1, map2);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(std::get< int >(result["a"]), 1);
    EXPECT_EQ(std::get< int >(result["b"]), 2);
    EXPECT_EQ(std::get< std::string >(result["c"]), "world");
}

// Test edge cases
TEST_F(MergeMapTest, EdgeCases)
{
    // Map with single element
    std::map< std::string, int > single_map = {{"a", 1}};
    auto result1                            = Helper::mergeMaps(single_map, single_map);
    EXPECT_EQ(result1.size(), 1);
    EXPECT_EQ(result1["a"], 1);

    // Maps with same keys but different values
    std::map< std::string, int > map1 = {{"a", 1}, {"b", 2}};
    std::map< std::string, int > map2 = {{"a", 3}, {"b", 4}};
    auto result2                      = Helper::mergeMaps(map1, map2);
    EXPECT_EQ(result2["a"], 3);
    EXPECT_EQ(result2["b"], 4);
}

// Test deeply nested maps
TEST_F(MergeMapTest, DeeplyNestedMaps)
{
    std::map< std::string, std::map< std::string, std::map< std::string, int > > > deep_map1 = {
        {"l1", {{"l2", {{"l3", 1}}}}}};
    std::map< std::string, std::map< std::string, std::map< std::string, int > > > deep_map2 = {
        {"l1", {{"l2", {{"l3", 2}, {"l3_new", 3}}}}}};

    auto result = Helper::mergeMaps(deep_map1, deep_map2);

    EXPECT_EQ(result["l1"]["l2"]["l3"], 2);
    EXPECT_EQ(result["l1"]["l2"]["l3_new"], 3);
}

// Test stress test with large maps
TEST_F(MergeMapTest, StressTest)
{
    std::map< std::string, int > large_map1;
    std::map< std::string, int > large_map2;

    // Create large maps
    for (int i = 0; i < 1000; i++)
    {
        large_map1["key" + std::to_string(i)]       = i;
        large_map2["key" + std::to_string(i + 500)] = i + 1000;
    }

    auto result = Helper::mergeMaps(large_map1, large_map2);

    EXPECT_EQ(result.size(), 1500);
    // Check some random elements
    EXPECT_EQ(result["key0"], 0);
    EXPECT_EQ(result["key999"], 1499);
    EXPECT_EQ(result["key1000"], 1500);
    EXPECT_EQ(result["key1499"], 1999);
}

// Test fixture for trading mode and debug functions
class TradingModeTest : public ::testing::Test
{
   protected:
    void TearDown() override { reset(); }

    void setEnv(const std::string &key, const std::string &val)
    {
        // Use environment variable to override config
        setenv(key.c_str(), val.c_str(), 1);
        envKeys.push_back(key);
    }

    void reset()
    {
        // Remove environment variable
        for (auto &key : envKeys)
            unsetenv(key.c_str());

        Config::Config::getInstance().reload();
    }

   private:
    std::vector< std::string > envKeys;
};

// Tests for isBacktesting
TEST_F(TradingModeTest, IsBacktestingTrue)
{
    EXPECT_FALSE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "backtest");
    EXPECT_TRUE(Helper::isBacktesting());

    reset();
}

TEST_F(TradingModeTest, IsBacktestingFalse)
{
    EXPECT_FALSE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "backtest");
    EXPECT_TRUE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "livetrade");
    EXPECT_FALSE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "papertrade");
    EXPECT_FALSE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "candles");
    EXPECT_FALSE(Helper::isBacktesting());

    Config::Config::getInstance().reload();
    setEnv("WHAT____", "steve austin");
    EXPECT_FALSE(Helper::isBacktesting());

    reset();
}

// Tests for isDebugging
TEST_F(TradingModeTest, IsDebugging)
{
    EXPECT_FALSE(Helper::isDebugging());

    Config::Config::getInstance().reload();
    setEnv("APP_DEBUG_MODE", "true");
    EXPECT_TRUE(Helper::isDebugging());

    reset();
    EXPECT_FALSE(Helper::isDebugging());

    reset();
}

// Tests for isDebuggable
TEST_F(TradingModeTest, IsDebuggable)
{
    EXPECT_FALSE(Helper::isDebugging());

    Config::Config::getInstance().reload();
    setEnv("APP_DEBUG_MODE", "true");
    EXPECT_TRUE(Helper::isDebugging());
    EXPECT_TRUE(Helper::isDebuggable("position_closed"));

    setEnv("ENV_LOGGING_POSITION_CLOSED", "true");
    EXPECT_TRUE(Helper::isDebuggable("position_closed"));

    reset();
    setEnv("ENV_LOGGING_POSITION_CLOSED", "true");
    EXPECT_FALSE(Helper::isDebuggable("position_closed"));

    reset();
}

TEST_F(TradingModeTest, IsDebuggableItemNotFound)
{
    EXPECT_FALSE(Helper::isDebugging());

    Config::Config::getInstance().reload();
    setEnv("APP_DEBUG_MODE", "true");
    EXPECT_TRUE(Helper::isDebugging());
    EXPECT_FALSE(Helper::isDebuggable("no-item"));

    setEnv("ENV_LOGGING_NO_ITEM", "true");
    EXPECT_FALSE(Helper::isDebuggable("no-item"));

    reset();
    setEnv("ENV_LOGGING_NO_ITEM", "true");
    EXPECT_FALSE(Helper::isDebuggable("no-item"));

    reset();
}

// Tests for isImportingCandles
TEST_F(TradingModeTest, IsImportingCandlesTrue)
{
    EXPECT_FALSE(Helper::isImportingCandles());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "candles");
    EXPECT_TRUE(Helper::isImportingCandles());

    reset();
}

TEST_F(TradingModeTest, IsImportingCandlesFalse)
{
    EXPECT_FALSE(Helper::isImportingCandles());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "backtest");
    EXPECT_FALSE(Helper::isImportingCandles());

    reset();
}

// Tests for isLiveTrading
TEST_F(TradingModeTest, IsLiveTradingTrue)
{
    EXPECT_FALSE(Helper::isLiveTrading());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "livetrade");
    EXPECT_TRUE(Helper::isLiveTrading());

    reset();
}

TEST_F(TradingModeTest, IsLiveTradingFalse)
{
    EXPECT_FALSE(Helper::isLiveTrading());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "candles");
    EXPECT_FALSE(Helper::isLiveTrading());

    reset();
}

// Tests for isPaperTrading
TEST_F(TradingModeTest, IsPaperTradingTrue)
{
    EXPECT_FALSE(Helper::isPaperTrading());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "papertrade");
    EXPECT_TRUE(Helper::isPaperTrading());

    reset();
}

TEST_F(TradingModeTest, IsPaperTradingFalse)
{
    EXPECT_FALSE(Helper::isPaperTrading());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "candles");
    EXPECT_FALSE(Helper::isPaperTrading());

    reset();
}

// Tests for isLive
TEST_F(TradingModeTest, IsLiveWithLiveTrading)
{
    EXPECT_FALSE(Helper::isLive());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "livetrade");
    EXPECT_TRUE(Helper::isLive());

    reset();
}

TEST_F(TradingModeTest, IsLiveWithPaperTrading)
{
    EXPECT_FALSE(Helper::isLive());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "papertrade");
    EXPECT_TRUE(Helper::isLive());

    reset();
}

TEST_F(TradingModeTest, IsLiveFalse)
{
    EXPECT_FALSE(Helper::isLive());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "backtest");
    EXPECT_FALSE(Helper::isLive());

    reset();
}

TEST_F(TradingModeTest, ShouldExecuteSilently)
{
    EXPECT_FALSE(Helper::shouldExecuteSilently());

    Config::Config::getInstance().reload();
    setEnv("APP_TRADING_MODE", "optimize");
    EXPECT_TRUE(Helper::shouldExecuteSilently());

    reset();
}

// Test edge cases for all trading mode functions
TEST_F(TradingModeTest, EdgeCaseEmptyTradingMode)
{
    EXPECT_FALSE(Helper::isBacktesting());
    EXPECT_FALSE(Helper::isLiveTrading());
    EXPECT_FALSE(Helper::isPaperTrading());
    EXPECT_FALSE(Helper::isImportingCandles());
    EXPECT_FALSE(Helper::isLive());

    reset();
}

TEST_F(TradingModeTest, EdgeCaseInvalidTradingMode)
{
    setEnv("APP_WHAT", "ha");
    EXPECT_FALSE(Helper::isBacktesting());
    EXPECT_FALSE(Helper::isLiveTrading());
    EXPECT_FALSE(Helper::isPaperTrading());
    EXPECT_FALSE(Helper::isImportingCandles());
    EXPECT_FALSE(Helper::isLive());

    reset();
}

// Test fixture for composite key generation
class CompositeKeyTest : public ::testing::Test
{
   protected:
    std::string exchange      = "Binance";
    std::string symbol        = "BTC-USD";
    Enum::Timeframe timeframe = Enum::Timeframe::HOUR_1;
};

TEST_F(CompositeKeyTest, WithTimeframe)
{
    auto result = Helper::generateCompositeKey(exchange, symbol, timeframe);
    EXPECT_EQ(result, "Binance-BTC-USD-1h");
}

TEST_F(CompositeKeyTest, WithoutTimeframe)
{
    auto result = Helper::generateCompositeKey(exchange, symbol, std::nullopt);
    EXPECT_EQ(result, "Binance-BTC-USD");
}

TEST_F(CompositeKeyTest, EdgeCases)
{
    // Empty strings
    EXPECT_EQ(Helper::generateCompositeKey("", "", std::nullopt), "-");
    EXPECT_EQ(Helper::generateCompositeKey("", "", timeframe), "--1h");

    // Special characters in exchange/symbol
    EXPECT_EQ(Helper::generateCompositeKey("Binance-Spot", "BTC-USD", std::nullopt), "Binance-Spot-BTC-USD");

    // Maximum timeframe
    EXPECT_EQ(Helper::generateCompositeKey(exchange, symbol, Enum::Timeframe::MONTH_1), "Binance-BTC-USD-1M");
}

// Test fixture for timeframe handling
class TimeframeTest : public ::testing::Test
{
   protected:
    std::vector< Enum::Timeframe > all_timeframes = {Enum::Timeframe::MINUTE_1,
                                                     Enum::Timeframe::MINUTE_3,
                                                     Enum::Timeframe::MINUTE_5,
                                                     Enum::Timeframe::MINUTE_15,
                                                     Enum::Timeframe::MINUTE_30,
                                                     Enum::Timeframe::MINUTE_45,
                                                     Enum::Timeframe::HOUR_1,
                                                     Enum::Timeframe::HOUR_2,
                                                     Enum::Timeframe::HOUR_3,
                                                     Enum::Timeframe::HOUR_4,
                                                     Enum::Timeframe::HOUR_6,
                                                     Enum::Timeframe::HOUR_8,
                                                     Enum::Timeframe::HOUR_12,
                                                     Enum::Timeframe::DAY_1,
                                                     Enum::Timeframe::DAY_3,
                                                     Enum::Timeframe::WEEK_1,
                                                     Enum::Timeframe::MONTH_1};
};

TEST_F(TimeframeTest, MaxTimeframeBasic)
{
    std::vector< Enum::Timeframe > timeframes = {
        Enum::Timeframe::MINUTE_1, Enum::Timeframe::HOUR_1, Enum::Timeframe::DAY_1};
    EXPECT_EQ(Helper::maxTimeframe(timeframes), Enum::Timeframe::DAY_1);
}

TEST_F(TimeframeTest, MaxTimeframeEmpty)
{
    std::vector< Enum::Timeframe > empty;
    EXPECT_EQ(Helper::maxTimeframe(empty), Enum::Timeframe::MINUTE_1);
}

TEST_F(TimeframeTest, MaxTimeframeSingle)
{
    std::vector< Enum::Timeframe > single = {Enum::Timeframe::HOUR_4};
    EXPECT_EQ(Helper::maxTimeframe(single), Enum::Timeframe::HOUR_4);
}

TEST_F(TimeframeTest, MaxTimeframeAll)
{
    EXPECT_EQ(Helper::maxTimeframe(all_timeframes), Enum::Timeframe::MONTH_1);
}

TEST_F(TimeframeTest, MaxTimeframeEdgeCases)
{
    // Test with unordered timeframes
    std::vector< Enum::Timeframe > unordered = {
        Enum::Timeframe::HOUR_4, Enum::Timeframe::MINUTE_1, Enum::Timeframe::DAY_1};
    EXPECT_EQ(Helper::maxTimeframe(unordered), Enum::Timeframe::DAY_1);

    // Test with duplicate timeframes
    std::vector< Enum::Timeframe > duplicates = {
        Enum::Timeframe::MINUTE_1, Enum::Timeframe::MINUTE_1, Enum::Timeframe::HOUR_1};
    EXPECT_EQ(Helper::maxTimeframe(duplicates), Enum::Timeframe::HOUR_1);
}

// Test basic timeframe conversions
TEST_F(TimeframeTest, BasicConversions)
{
    // Test minute-based timeframes
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_1), 1);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_3), 3);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_5), 5);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_15), 15);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_30), 30);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_45), 45);

    // Test hour-based timeframes
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_1), 60);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_2), 120);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_3), 180);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_4), 240);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_6), 360);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_8), 480);
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_12), 720);

    // Test day-based timeframes
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::DAY_1), 1440); // 24 * 60
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::DAY_3), 4320); // 3 * 24 * 60

    // Test week-based timeframe
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::WEEK_1), 10080); // 7 * 24 * 60

    // Test month-based timeframe
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MONTH_1), 43200); // 30 * 24 * 60
}

// Test error handling for invalid timeframes
TEST_F(TimeframeTest, InvalidTimeframe)
{
    // Create an invalid timeframe using enum value outside the valid range
    Enum::Timeframe invalid_timeframe = static_cast< Enum::Timeframe >(-1);

    // Expect an InvalidTimeframe exception
    EXPECT_THROW({ Helper::getTimeframeToOneMinutes(invalid_timeframe); }, Exception::InvalidTimeframe);
}

// Test consistency of results
TEST_F(TimeframeTest, ConsistencyCheck)
{
    // Test that multiple calls return the same result
    Enum::Timeframe test_timeframe = Enum::Timeframe::HOUR_1;
    int64_t first_result           = Helper::getTimeframeToOneMinutes(test_timeframe);

    for (int i = 0; i < 100; i++)
    {
        EXPECT_EQ(Helper::getTimeframeToOneMinutes(test_timeframe), first_result);
    }
}

// Test relative relationships between timeframes
TEST_F(TimeframeTest, RelativeTimeframes)
{
    // Test that larger timeframes return proportionally larger values
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_2),
              Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_1) * 2);

    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::WEEK_1),
              Helper::getTimeframeToOneMinutes(Enum::Timeframe::DAY_1) * 7);
}

// Test boundary values
TEST_F(TimeframeTest, BoundaryValues)
{
    // Test smallest timeframe
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MINUTE_1), 1);

    // Test largest timeframe
    EXPECT_EQ(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MONTH_1), 43200);

    // Verify that the largest timeframe doesn't overflow int64_t
    EXPECT_LT(Helper::getTimeframeToOneMinutes(Enum::Timeframe::MONTH_1), std::numeric_limits< int64_t >::max());
}

// Stress test with multiple rapid calls
TEST_F(TimeframeTest, StressTest)
{
    std::vector< Enum::Timeframe > timeframes = {Enum::Timeframe::MINUTE_1,
                                                 Enum::Timeframe::HOUR_1,
                                                 Enum::Timeframe::DAY_1,
                                                 Enum::Timeframe::WEEK_1,
                                                 Enum::Timeframe::MONTH_1};

    // Make multiple rapid calls to test static map performance
    for (int i = 0; i < 10000; i++)
    {
        for (const auto &tf : timeframes)
        {
            EXPECT_NO_THROW({ Helper::getTimeframeToOneMinutes(tf); });
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
                        Helper::getTimeframeToOneMinutes(Enum::Timeframe::HOUR_1);
                        Helper::getTimeframeToOneMinutes(Enum::Timeframe::DAY_1);
                        Helper::getTimeframeToOneMinutes(Enum::Timeframe::WEEK_1);
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

// Test fixture (optional, for shared setup if needed)
class ScaleToRangeTest : public ::testing::Test
{
};

// Test cases
TEST(ScaleToRangeTest, DoubleNormalCase)
{
    double result = Helper::scaleToRange(100.0, 0.0, 1.0, 0.0, 50.0);
    EXPECT_DOUBLE_EQ(result, 0.5); // 50 is halfway between 0 and 100, maps to 0.5 in [0, 1]
}

TEST(ScaleToRangeTest, IntNormalCase)
{
    int result = Helper::scaleToRange(10, 0, 100, 0, 5);
    EXPECT_EQ(result,
              50); // 5 is halfway between 0 and 10, maps to 50 in [0, 100]
}

TEST(ScaleToRangeTest, FloatEdgeMin)
{
    float result = Helper::scaleToRange(10.0f, 0.0f, 100.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(result, 0.0f); // Min value maps to newMin
}

TEST(ScaleToRangeTest, FloatEdgeMax)
{
    float result = Helper::scaleToRange(10.0f, 0.0f, 100.0f, 0.0f, 10.0f);
    EXPECT_FLOAT_EQ(result, 100.0f); // Max value maps to newMax
}

TEST(ScaleToRangeTest, NegativeRange)
{
    double result = Helper::scaleToRange(0.0, -100.0, 1.0, 0.0, -50.0);
    EXPECT_DOUBLE_EQ(result,
                     0.5); // -50 is halfway between -100 and 0, maps to 0.5 in [0, 1]
}

TEST(ScaleToRangeTest, ThrowsWhenValueBelowMin)
{
    EXPECT_THROW(Helper::scaleToRange(10, 0, 100, 0, -1), std::invalid_argument);
}

TEST(ScaleToRangeTest, ThrowsWhenValueAboveMax)
{
    EXPECT_THROW(Helper::scaleToRange(10, 0, 100, 0, 11), std::invalid_argument);
}

TEST(ScaleToRangeTest, ThrowsWhenOldRangeZero)
{
    EXPECT_THROW(Helper::scaleToRange(5, 5, 100, 0, 5), std::invalid_argument);
}

TEST(ScaleToRangeTest, DoublePrecision)
{
    double result = Helper::scaleToRange(200.0, 100.0, 2.0, 1.0, 150.0);
    EXPECT_DOUBLE_EQ(result,
                     1.5); // 150 is halfway between 100 and 200, maps to 1.5 in [1, 2]
}

// Test fixture for normalization
class NormalizationTest : public ::testing::Test
{
   protected:
    // Test data for different numeric types
    const int int_min       = 0;
    const int int_max       = 100;
    const float float_min   = 0.0f;
    const float float_max   = 1.0f;
    const double double_min = -100.0;
    const double double_max = 100.0;
};

TEST_F(NormalizationTest, IntegerNormalization)
{
    EXPECT_EQ(Helper::normalize(50, int_min, int_max), 0);
    EXPECT_EQ(Helper::normalize(0, int_min, int_max), 0);
    EXPECT_EQ(Helper::normalize(100, int_min, int_max), 1);
}

TEST_F(NormalizationTest, FloatNormalization)
{
    EXPECT_FLOAT_EQ(Helper::normalize(0.5f, float_min, float_max), 0.5f);
    EXPECT_FLOAT_EQ(Helper::normalize(0.0f, float_min, float_max), 0.0f);
    EXPECT_FLOAT_EQ(Helper::normalize(1.0f, float_min, float_max), 1.0f);
}

TEST_F(NormalizationTest, DoubleNormalization)
{
    EXPECT_DOUBLE_EQ(Helper::normalize(0.0, double_min, double_max), 0.5);
    EXPECT_DOUBLE_EQ(Helper::normalize(-100.0, double_min, double_max), 0.0);
    EXPECT_DOUBLE_EQ(Helper::normalize(100.0, double_min, double_max), 1.0);
}

TEST_F(NormalizationTest, EdgeCases)
{
    // Test with equal min and max
    EXPECT_EQ(Helper::normalize(5, 5, 5), 0);

    // Test with negative ranges
    EXPECT_EQ(Helper::normalize(-5, -10, 0), 0);

    // Test with zero range
    EXPECT_EQ(Helper::normalize(0, 0, 0), 0);

    // Test with value equal to min
    EXPECT_EQ(Helper::normalize(0, 0, 100), 0);

    // Test with value equal to max
    EXPECT_EQ(Helper::normalize(100, 0, 100), 1);
}

class EnumsConversionTest : public ::testing::Test
{
};

// Tests for oppositeSide
TEST_F(EnumsConversionTest, OppositeSideBasic)
{
    EXPECT_EQ(Helper::oppositeSide(Enum::Side::BUY), Enum::Side::SELL);
    EXPECT_EQ(Helper::oppositeSide(Enum::Side::SELL), Enum::Side::BUY);
}

TEST_F(EnumsConversionTest, OppositeSideInvalid)
{
    EXPECT_THROW(Helper::oppositeSide(static_cast< Enum::Side >(999)), std::invalid_argument);
}

// Tests for oppositeTradeType
TEST_F(EnumsConversionTest, OppositeTradeTypeBasic)
{
    EXPECT_EQ(Helper::oppositeTradeType(Enum::TradeType::LONG), Enum::TradeType::SHORT);
    EXPECT_EQ(Helper::oppositeTradeType(Enum::TradeType::SHORT), Enum::TradeType::LONG);
}

TEST_F(EnumsConversionTest, OppositeTradeTypeInvalid)
{
    EXPECT_THROW(Helper::oppositeTradeType(static_cast< Enum::TradeType >(999)), std::invalid_argument);
}

TEST_F(EnumsConversionTest, SideToTypeBasic)
{
    // Test buy side
    EXPECT_EQ(Helper::sideToType(Enum::Side::BUY), Enum::TradeType::LONG);

    // Test sell side
    EXPECT_EQ(Helper::sideToType(Enum::Side::SELL), Enum::TradeType::SHORT);
}

TEST_F(EnumsConversionTest, TypeToSideBasic)
{
    EXPECT_EQ(Helper::typeToSide(Enum::TradeType::LONG), Enum::Side::BUY);

    EXPECT_EQ(Helper::typeToSide(Enum::TradeType::SHORT), Enum::Side::SELL);
}

TEST_F(EnumsConversionTest, closingSideBasic)
{
    EXPECT_EQ(Helper::closingSide(Enum::Position::LONG), Enum::Side::SELL);

    EXPECT_EQ(Helper::closingSide(Enum::Position::SHORT), Enum::Side::BUY);
}

TEST_F(NormalizationTest, TypeSafety)
{
    // These should compile
    EXPECT_NO_THROW(Helper::normalize(1, 0, 10));
    EXPECT_NO_THROW(Helper::normalize(1.0f, 0.0f, 10.0f));
    EXPECT_NO_THROW(Helper::normalize(1.0, 0.0, 10.0));

    // These should not compile (commented out to avoid compilation errors)
    /*
    EXPECT_NO_THROW(Helper::normalize("string", "min", "max")); // Should fail
    EXPECT_NO_THROW(Helper::normalize(true, false, true)); // Should fail
    */
}

// Test fixture for matrix operations
class MatrixOperationsTest : public ::testing::Test
{
   protected:
    void SetUp() override { reset(); }

    blaze::DynamicMatrix< double > test_matrix;
    blaze::DynamicMatrix< double > empty_matrix;
    blaze::DynamicMatrix< double > single_element_matrix;
    blaze::DynamicMatrix< double > all_nan_matrix;
    std::mt19937 rng;

    void reset()
    {
        // Initialize test matrices
        test_matrix  = {{1.0, 2.0, 3.0}, {4.0, std::numeric_limits< double >::quiet_NaN(), 6.0}, {7.0, 8.0, 9.0}};
        empty_matrix = blaze::DynamicMatrix< double >(0, 0);
        single_element_matrix = blaze::DynamicMatrix< double >(1, 1, 1.0);
        all_nan_matrix        = blaze::DynamicMatrix< double >(2, 2, std::numeric_limits< double >::quiet_NaN());

        // Initialize random number generator
        rng.seed(std::random_device()());
    }
};

// Tests for current1mCandleTimestamp
TEST_F(MatrixOperationsTest, Current1mCandleTimestampBasic)
{
    reset();

    int64_t ts = Helper::current1mCandleTimestamp();
    EXPECT_GT(ts, 0);
    EXPECT_EQ(ts % 60000, 0); // Should be aligned to minute boundary
    EXPECT_LE(
        ts,
        std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

TEST_F(MatrixOperationsTest, Current1mCandleTimestampPrecision)
{
    reset();

    int64_t ts1 = Helper::current1mCandleTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int64_t ts2 = Helper::current1mCandleTimestamp();
    EXPECT_EQ(ts1, ts2); // Should be same minute
}

// Tests for forwardFill
TEST_F(MatrixOperationsTest, ForwardFillBasic)
{
    reset();

    auto result = Helper::forwardFill(test_matrix, 0);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_EQ(result.columns(), 3);
    EXPECT_EQ(result(0, 0), 1.0);
    EXPECT_EQ(result(1, 1), 2.0); // Should be filled with 2.0
    EXPECT_EQ(result(2, 1), 8.0); // Should be filled with 8.0
}

TEST_F(MatrixOperationsTest, ForwardFillEmptyMatrix)
{
    reset();

    auto result = Helper::forwardFill(empty_matrix, 0);
    EXPECT_EQ(result.rows(), 0);
    EXPECT_EQ(result.columns(), 0);
}

TEST_F(MatrixOperationsTest, ForwardFillSingleElement)
{
    reset();

    auto result = Helper::forwardFill(single_element_matrix, 0);
    EXPECT_EQ(result.rows(), 1);
    EXPECT_EQ(result.columns(), 1);
    EXPECT_EQ(result(0, 0), 1.0);
}

TEST_F(MatrixOperationsTest, ForwardFillAllNaN)
{
    reset();

    auto result = Helper::forwardFill(all_nan_matrix, 0);
    EXPECT_EQ(result.rows(), 2);
    EXPECT_EQ(result.columns(), 2);
    EXPECT_TRUE(std::isnan(result(0, 0)));
    EXPECT_TRUE(std::isnan(result(0, 1)));
    EXPECT_TRUE(std::isnan(result(1, 0)));
    EXPECT_TRUE(std::isnan(result(1, 1)));
}

TEST_F(MatrixOperationsTest, ForwardFillColumnAxis)
{
    reset();

    auto result = Helper::forwardFill(test_matrix, 1);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_EQ(result.columns(), 3);
    EXPECT_EQ(result(1, 0), 4.0);
    EXPECT_EQ(result(1, 1), 2.0); // Should be filled with 2.0
    EXPECT_EQ(result(1, 2), 6.0);
}

// Tests for shift
TEST_F(MatrixOperationsTest, ShiftBasic)
{
    reset();

    auto result = Helper::shift(test_matrix, 1);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_EQ(result.columns(), 3);
    EXPECT_EQ(result(0, 0), 0.0); // Fill value
    EXPECT_EQ(result(1, 0), 1.0);
    EXPECT_EQ(result(2, 0), 4.0);
}

TEST_F(MatrixOperationsTest, ShiftNegative)
{
    reset();

    auto result = Helper::shift(test_matrix, -1);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_EQ(result.columns(), 3);
    EXPECT_EQ(result(0, 0), 4.0);
    EXPECT_EQ(result(1, 0), 7.0);
    EXPECT_EQ(result(2, 0), 0.0); // Fill value
}

TEST_F(MatrixOperationsTest, ShiftZero)
{
    reset();

    auto result = Helper::shift(test_matrix, 0);
    EXPECT_TRUE(Helper::matricesEqualWithTolerance(result, test_matrix));
    // EXPECT_EQ(result, test_matrix); // Should be identical // ISSUE: Why?
}

TEST_F(MatrixOperationsTest, ShiftEmptyMatrix)
{
    reset();

    EXPECT_THROW(Helper::shift(empty_matrix, 1), std::invalid_argument);
}

TEST_F(MatrixOperationsTest, ShiftCustomFillValue)
{
    reset();

    auto result = Helper::shift(test_matrix, 1, -1.0);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_EQ(result.columns(), 3);
    EXPECT_EQ(result(0, 0), -1.0); // Custom fill value
    EXPECT_EQ(result(1, 0), 1.0);
    EXPECT_EQ(result(2, 0), 4.0);
}

// Edge cases and stress tests
TEST_F(MatrixOperationsTest, ForwardFillEdgeCases)
{
    reset();

    // Test with very large values
    blaze::DynamicMatrix< double > large_matrix({{std::numeric_limits< double >::max(),
                                                  std::numeric_limits< double >::quiet_NaN(),
                                                  std::numeric_limits< double >::max()},
                                                 {std::numeric_limits< double >::quiet_NaN(),
                                                  std::numeric_limits< double >::max(),
                                                  std::numeric_limits< double >::quiet_NaN()},
                                                 {std::numeric_limits< double >::max(),
                                                  std::numeric_limits< double >::quiet_NaN(),
                                                  std::numeric_limits< double >::max()}});

    auto result = Helper::forwardFill(large_matrix, 0);
    EXPECT_EQ(result(0, 0), std::numeric_limits< double >::max());
    EXPECT_EQ(result(1, 0), std::numeric_limits< double >::max());
    EXPECT_EQ(result(2, 0), std::numeric_limits< double >::max());
}

TEST_F(MatrixOperationsTest, ShiftEdgeCases)
{
    // Test with shift larger than matrix size
    EXPECT_THROW(Helper::shift(test_matrix, 5), std::invalid_argument);

    // Test with negative shift larger than matrix size
    EXPECT_THROW(Helper::shift(test_matrix, -5), std::invalid_argument);

    // Test with shift equal to matrix size (should not throw)
    auto result1 = Helper::shift(test_matrix, 3);
    EXPECT_EQ(result1.rows(), 3);
    EXPECT_EQ(result1.columns(), 3);
    // All elements should be fill value (0.0)
    for (size_t i = 0; i < result1.rows(); ++i)
    {
        for (size_t j = 0; j < result1.columns(); ++j)
        {
            EXPECT_EQ(result1(i, j), 0.0);
        }
    }

    // Test with negative shift equal to matrix size (should not throw)
    auto result2 = Helper::shift(test_matrix, -3);
    EXPECT_EQ(result2.rows(), 3);
    EXPECT_EQ(result2.columns(), 3);
    // All elements should be fill value (0.0)
    for (size_t i = 0; i < result2.rows(); ++i)
    {
        for (size_t j = 0; j < result2.columns(); ++j)
        {
            EXPECT_EQ(result2(i, j), 0.0);
        }
    }
}

TEST_F(MatrixOperationsTest, SameLength_NormalCase)
{
    blaze::DynamicMatrix< double > bigger(5, 2, 1.0);
    blaze::DynamicMatrix< double > shorter(3, 2, 2.0);
    auto result = Helper::sameLength(bigger, shorter);
    EXPECT_EQ(result.rows(), 5);
    EXPECT_EQ(result.columns(), 2);
    EXPECT_TRUE(std::isnan(result(0, 0)));
    EXPECT_TRUE(std::isnan(result(1, 1)));
    EXPECT_DOUBLE_EQ(result(2, 0), 2.0);
    EXPECT_DOUBLE_EQ(result(4, 1), 2.0);
}

TEST_F(MatrixOperationsTest, SameLength_EdgeEmptyShorter)
{
    blaze::DynamicMatrix< double > bigger(3, 2, 1.0);
    blaze::DynamicMatrix< double > shorter(0, 2);
    auto result = Helper::sameLength(bigger, shorter);
    EXPECT_EQ(result.rows(), 3);
    EXPECT_TRUE(std::isnan(result(0, 0)));
    EXPECT_TRUE(std::isnan(result(2, 1)));
}

// Tests for sameLength function
TEST_F(MatrixOperationsTest, SameLengthBasic)
{
    // Create test matrices
    blaze::DynamicMatrix< double > bigger(5, 3, 1.0);
    blaze::DynamicMatrix< double > shorter(3, 3, 2.0);

    // Call function
    auto result = Helper::sameLength(bigger, shorter);

    // Verify dimensions
    EXPECT_EQ(result.rows(), bigger.rows());
    EXPECT_EQ(result.columns(), bigger.columns());

    // Verify NaN values in first rows
    for (size_t i = 0; i < bigger.rows() - shorter.rows(); ++i)
    {
        for (size_t j = 0; j < bigger.columns(); ++j)
        {
            EXPECT_TRUE(std::isnan(result(i, j)));
        }
    }

    // Verify copied values
    for (size_t i = 0; i < shorter.rows(); ++i)
    {
        for (size_t j = 0; j < shorter.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i + bigger.rows() - shorter.rows(), j), shorter(i, j));
        }
    }
}

TEST_F(MatrixOperationsTest, SameLengthEdgeCases)
{
    // Test with empty matrices
    blaze::DynamicMatrix< double > empty(0, 0);
    blaze::DynamicMatrix< double > normal(3, 3, 1.0);

    // Empty bigger
    EXPECT_NO_THROW(Helper::sameLength(empty, empty));

    // Same size matrices
    blaze::DynamicMatrix< double > same1(3, 3, 1.0);
    blaze::DynamicMatrix< double > same2(3, 3, 2.0);
    auto result = Helper::sameLength(same1, same2);

    // Should have no NaN values
    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), same2(i, j));
        }
    }

    // Different column count
    blaze::DynamicMatrix< double > diff_cols1(5, 4, 1.0);
    blaze::DynamicMatrix< double > diff_cols2(3, 3, 2.0);
    auto result2 = Helper::sameLength(diff_cols1, diff_cols2);

    EXPECT_EQ(result2.rows(), diff_cols1.rows());
    EXPECT_EQ(result2.columns(), diff_cols1.columns());
}

// Stress tests
TEST_F(MatrixOperationsTest, ForwardFillStress)
{
    reset();

    const size_t size = 1000;
    blaze::DynamicMatrix< double > large_matrix(size, size);

    // Fill with random values and some NaN
    std::uniform_real_distribution< double > dist(0.0, 1.0);
    for (size_t i = 0; i < size; ++i)
    {
        for (size_t j = 0; j < size; ++j)
        {
            if (dist(rng) < 0.1)
            { // 10% chance of NaN
                large_matrix(i, j) = std::numeric_limits< double >::quiet_NaN();
            }
            else
            {
                large_matrix(i, j) = dist(rng);
            }
        }
    }

    auto result = Helper::forwardFill(large_matrix, 0);
    EXPECT_EQ(result.rows(), size);
    EXPECT_EQ(result.columns(), size);
}

TEST_F(MatrixOperationsTest, ShiftStress)
{
    reset();

    const size_t size = 1000;
    blaze::DynamicMatrix< double > large_matrix(size, size);

    // Fill with random values
    std::uniform_real_distribution< double > dist(0.0, 1.0);
    for (size_t i = 0; i < size; ++i)
    {
        for (size_t j = 0; j < size; ++j)
        {
            large_matrix(i, j) = dist(rng);
        }
    }

    auto result = Helper::shift(large_matrix, 100);
    EXPECT_EQ(result.rows(), size);
    EXPECT_EQ(result.columns(), size);
}

TEST_F(MatrixOperationsTest, StressTestSameLength)
{
    // Create large matrices
    blaze::DynamicMatrix< double > bigger(10000, 10, 1.0);
    blaze::DynamicMatrix< double > shorter(5000, 10, 2.0);

    // Measure performance
    auto start  = std::chrono::high_resolution_clock::now();
    auto result = Helper::sameLength(bigger, shorter);
    auto end    = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count();

    // Just check that it completes in a reasonable time
    SUCCEED() << "sameLength completed in " << duration << "ms";

    // Verify dimensions
    EXPECT_EQ(result.rows(), bigger.rows());
    EXPECT_EQ(result.columns(), bigger.columns());
}

class FindOrderbookInsertionIndexTest : public ::testing::Test
{
   protected:
    void SetUp() override { rng.seed(std::random_device()()); }

    std::mt19937 rng;
};

// Tests for findOrderbookInsertionIndex
TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexBasic)
{
    blaze::DynamicMatrix< double > orderbook({{1.0}, {2.0}, {3.0}});

    auto [found, index] = Helper::findOrderbookInsertionIndex(orderbook, 2.0, true);
    EXPECT_TRUE(found);
    EXPECT_EQ(index, 1);
}

TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexNotFound)
{
    blaze::DynamicMatrix< double > orderbook({{1.0}, {2.0}, {3.0}});

    auto [found, index] = Helper::findOrderbookInsertionIndex(orderbook, 2.5, true);
    EXPECT_FALSE(found);
    EXPECT_EQ(index, 2);
}

TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexEmpty)
{
    blaze::DynamicMatrix< double > empty_orderbook(0, 1);

    auto [found, index] = Helper::findOrderbookInsertionIndex(empty_orderbook, 1.0, true);
    EXPECT_FALSE(found);
    EXPECT_EQ(index, 0);
}

TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexStress)
{
    const size_t size = 10000;
    blaze::DynamicMatrix< double > large_orderbook(size, 1);

    // Fill with sorted values
    for (size_t i = 0; i < size; ++i)
    {
        large_orderbook(i, 0) = static_cast< double >(i);
    }

    // Test with random values
    std::uniform_real_distribution< double > dist(0.0, size);
    for (int i = 0; i < 1000; ++i)
    {
        double target       = dist(rng);
        auto [found, index] = Helper::findOrderbookInsertionIndex(large_orderbook, target, true);
        EXPECT_LE(index, size);
    }
}

TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexDescending)
{
    blaze::DynamicMatrix< double > orderbook({{3.0}, {2.0}, {1.0}});

    auto [found, index] = Helper::findOrderbookInsertionIndex(orderbook, 2.0, false);
    EXPECT_TRUE(found);
    EXPECT_EQ(index, 1);
}

TEST_F(FindOrderbookInsertionIndexTest, FindOrderbookInsertionIndexEdgeCases)
{
    // Test with extreme values
    blaze::DynamicMatrix< double > orderbook({{std::numeric_limits< double >::min()},
                                              {std::numeric_limits< double >::max()},
                                              {std::numeric_limits< double >::infinity()}});

    auto [found1, index1] = Helper::findOrderbookInsertionIndex(orderbook, std::numeric_limits< double >::min(), true);
    EXPECT_TRUE(found1);
    EXPECT_EQ(index1, 0);

    auto [found2, index2] = Helper::findOrderbookInsertionIndex(orderbook, std::numeric_limits< double >::max(), true);
    EXPECT_TRUE(found2);
    EXPECT_EQ(index2, 1);
}

// Test fixture for common setup
class CleanOrderbookListTest : public ::testing::Test
{
   protected:
    std::vector< std::vector< std::string > > string_input = {{"1.23", "4.56"}, {"2.34", "5.67"}};
    std::vector< std::vector< int > > int_input            = {{1, 2}, {3, 4}};
};

// Tests for Version 1 (static_cast)
TEST_F(CleanOrderbookListTest, IntToDouble)
{
    auto result = Helper::cleanOrderbookList< int, double >(int_input);
    EXPECT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(result[0][0], 1.0);
    EXPECT_DOUBLE_EQ(result[0][1], 2.0);
    EXPECT_DOUBLE_EQ(result[1][0], 3.0);
    EXPECT_DOUBLE_EQ(result[1][1], 4.0);
}

TEST_F(CleanOrderbookListTest, IntToFloat)
{
    auto result = Helper::cleanOrderbookList< int, float >(int_input);
    EXPECT_EQ(result.size(), 2);
    EXPECT_FLOAT_EQ(result[0][0], 1.0f);
    EXPECT_FLOAT_EQ(result[0][1], 2.0f);
    EXPECT_FLOAT_EQ(result[1][0], 3.0f);
    EXPECT_FLOAT_EQ(result[1][1], 4.0f);
}

TEST_F(CleanOrderbookListTest, EmptyInput)
{
    std::vector< std::vector< int > > empty;
    auto result = Helper::cleanOrderbookList< int, double >(empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(CleanOrderbookListTest, InsufficientElements)
{
    std::vector< std::vector< int > > invalid = {{1}, {2, 3}};
    EXPECT_THROW((Helper::cleanOrderbookList< int, double >(invalid)), std::invalid_argument);
}

// Tests for Version 2 (custom converter)
TEST_F(CleanOrderbookListTest, StringToDouble)
{
    auto result = Helper::cleanOrderbookList< std::string, double >(string_input, Helper::strToDouble);
    EXPECT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(result[0][0], 1.23);
    EXPECT_DOUBLE_EQ(result[0][1], 4.56);
    EXPECT_DOUBLE_EQ(result[1][0], 2.34);
    EXPECT_DOUBLE_EQ(result[1][1], 5.67);
}

TEST_F(CleanOrderbookListTest, StringToFloat)
{
    auto result = Helper::cleanOrderbookList< std::string, float >(string_input, Helper::strToFloat);
    EXPECT_EQ(result.size(), 2);
    EXPECT_FLOAT_EQ(result[0][0], 1.23f);
    EXPECT_FLOAT_EQ(result[0][1], 4.56f);
    EXPECT_FLOAT_EQ(result[1][0], 2.34f);
    EXPECT_FLOAT_EQ(result[1][1], 5.67f);
}

TEST_F(CleanOrderbookListTest, InvalidStringConversion)
{
    std::vector< std::vector< std::string > > invalid = {{"abc", "4.56"}, {"2.34", "5.67"}};
    EXPECT_THROW((Helper::cleanOrderbookList< std::string, double >(invalid, Helper::strToDouble)),
                 std::invalid_argument);
}

TEST_F(CleanOrderbookListTest, EmptyInputWithConverter)
{
    std::vector< std::vector< std::string > > empty;
    auto result = Helper::cleanOrderbookList< std::string, double >(empty, Helper::strToDouble);
    EXPECT_TRUE(result.empty());
}

TEST_F(CleanOrderbookListTest, InsufficientElementsWithConverter)
{
    std::vector< std::vector< std::string > > invalid = {{"1.23"}, {"2.34", "5.67"}};
    EXPECT_THROW((Helper::cleanOrderbookList< std::string, double >(invalid, Helper::strToDouble)),
                 std::invalid_argument);
}

class OrderbookTrimPriceTest : public ::testing::Test
{
   protected:
    std::mt19937 rng;

    void SetUp() override { rng.seed(std::random_device()()); }
};

// Tests for orderbookTrimPrice
TEST_F(OrderbookTrimPriceTest, OrderbookTrimPriceBasic)
{
    // Test ascending order
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, true, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.1, true, 1.0), 101.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.9, true, 1.0), 101.0);

    // Test descending order
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, false, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.1, false, 1.0), 100.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.9, false, 1.0), 100.0);
}

TEST_F(OrderbookTrimPriceTest, OrderbookTrimPriceEdgeCases)
{
    // Test with very small unit
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.123456, true, 0.0001), 100.1235);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.123456, false, 0.0001), 100.1234);

    // Test with very large unit
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, true, 1000.0), 1000.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, false, 1000.0), 0.0);

    // Test with unit equal to price
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, true, 100.0), 100.0);
    // FIXME:
    // EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(100.0, false, 100.0), 0.0);

    // Test with zero price
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(0.0, true, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(0.0, false, 1.0), 0.0);

    // Test with negative price
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(-100.1, true, 1.0), -100.0);
    EXPECT_DOUBLE_EQ(Helper::orderbookTrimPrice(-100.1, false, 1.0), -101.0);

    // Test invalid unit
    EXPECT_THROW(Helper::orderbookTrimPrice(100.0, true, 0.0), std::invalid_argument);
    EXPECT_THROW(Helper::orderbookTrimPrice(100.0, true, -1.0), std::invalid_argument);
}

// Stress tests
TEST_F(OrderbookTrimPriceTest, OrderbookTrimPriceStress)
{
    std::uniform_real_distribution< double > price_dist(0.0, 1000.0);
    std::uniform_real_distribution< double > unit_dist(0.0001, 100.0);

    for (int i = 0; i < 1000; ++i)
    {
        double price   = price_dist(rng);
        double unit    = unit_dist(rng);
        bool ascending = (rng() % 2) == 0;

        double result = Helper::orderbookTrimPrice(price, ascending, unit);
        EXPECT_TRUE(std::isfinite(result));
        EXPECT_GE(result, 0.0);
    }
}

// Test fixture
class CandleUtilsTest : public ::testing::Test
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

// --- Enum-based get_candle_source Tests ---

TEST_F(CandleUtilsTest, GetCandleSourceEnumClose)
{
    auto result = Helper::getCandleSource(candles, Candle::Source::Close);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], 101.0);
    EXPECT_DOUBLE_EQ(result[1], 102.0);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHigh)
{
    auto result = Helper::getCandleSource(candles, Candle::Source::High);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], 102.0);
    EXPECT_DOUBLE_EQ(result[1], 103.0);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHL2)
{
    auto result = Helper::getCandleSource(candles, Candle::Source::HL2);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0) / 2.0);  // 100.5
    EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0) / 2.0); // 101.5
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHLC3)
{
    auto result = Helper::getCandleSource(candles, Candle::Source::HLC3);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0 + 101.0) / 3.0);  // 100.666...
    EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0 + 102.0) / 3.0); // 101.666...
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumOHLC4)
{
    auto result = Helper::getCandleSource(candles, Candle::Source::OHLC4);
    EXPECT_EQ(result.size(), 2UL);
    EXPECT_DOUBLE_EQ(result[0], (100.0 + 102.0 + 99.0 + 101.0) / 4.0);  // 100.5
    EXPECT_DOUBLE_EQ(result[1], (101.0 + 103.0 + 100.0 + 102.0) / 4.0); // 101.5
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumEmptyMatrix)
{
    blaze::DynamicMatrix< double > empty(0UL, 6UL);
    EXPECT_THROW(Helper::getCandleSource(empty, Candle::Source::Close), std::invalid_argument);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumInsufficientColumns)
{
    blaze::DynamicMatrix< double > small(2UL, 3UL);
    EXPECT_THROW(Helper::getCandleSource(small, Candle::Source::Close), std::invalid_argument);
}

// Tests for sliceCandles function
TEST_F(CandleUtilsTest, SliceCandlesBasic)
{
    // Create a test candles matrix
    blaze::DynamicMatrix< double > candles(500, 6, 1.0);

    // Test with sequential = true
    auto result1 = Helper::sliceCandles(candles, true);
    EXPECT_EQ(result1.rows(), candles.rows());
    EXPECT_EQ(result1.columns(), candles.columns());

    // Test with sequential = false
    auto result2 = Helper::sliceCandles(candles, false);
    // Check if it returns last warmupCandlesNum rows
    int warmupCandlesNum = 240; // Default value
    EXPECT_EQ(result2.rows(), warmupCandlesNum);
    EXPECT_EQ(result2.columns(), candles.columns());
}

TEST_F(CandleUtilsTest, SliceCandlesEdgeCases)
{
    // Test with fewer rows than warmup
    blaze::DynamicMatrix< double > small_candles(100, 6, 1.0);
    auto result = Helper::sliceCandles(small_candles, false);

    // Should return original matrix
    EXPECT_EQ(result.rows(), small_candles.rows());
    EXPECT_EQ(result.columns(), small_candles.columns());

    // Test with empty matrix
    blaze::DynamicMatrix< double > empty_candles(0, 6);
    auto result_empty = Helper::sliceCandles(empty_candles, false);

    EXPECT_EQ(result_empty.rows(), 0);
    EXPECT_EQ(result_empty.columns(), 6);
}

class SliceCandlesTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create a test matrix with known values
        testMatrix = blaze::DynamicMatrix< double >(300, 4);
        for (size_t i = 0; i < testMatrix.rows(); ++i)
        {
            for (size_t j = 0; j < testMatrix.columns(); ++j)
            {
                testMatrix(i, j) = i * 100 + j; // Unique values for easy verification
            }
        }
    }

    blaze::DynamicMatrix< double > testMatrix;
};

TEST_F(SliceCandlesTest, SequentialModeReturnsOriginalMatrix)
{
    // Test that sequential mode returns the original matrix unchanged
    auto result = Helper::sliceCandles(testMatrix, true);

    ASSERT_EQ(result.rows(), testMatrix.rows());
    ASSERT_EQ(result.columns(), testMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), testMatrix(i, j));
        }
    }
}

TEST_F(SliceCandlesTest, NonSequentialModeReturnsLastWarmupCandles)
{
    // Test that non-sequential mode returns the last warmup_candles_num rows
    auto result = Helper::sliceCandles(testMatrix, false);

    ASSERT_EQ(result.rows(), 240); // Default warmup_candles_num
    ASSERT_EQ(result.columns(), testMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), testMatrix(testMatrix.rows() - 240 + i, j));
        }
    }
}

TEST_F(SliceCandlesTest, MatrixSmallerThanWarmupCandles)
{
    // Test with a matrix smaller than warmup_candles_num
    blaze::DynamicMatrix< double > smallMatrix(100, 4);
    for (size_t i = 0; i < smallMatrix.rows(); ++i)
    {
        for (size_t j = 0; j < smallMatrix.columns(); ++j)
        {
            smallMatrix(i, j) = i * 100 + j;
        }
    }

    auto result = Helper::sliceCandles(smallMatrix, false);

    ASSERT_EQ(result.rows(), smallMatrix.rows());
    ASSERT_EQ(result.columns(), smallMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), smallMatrix(i, j));
        }
    }
}

TEST_F(SliceCandlesTest, EmptyMatrix)
{
    // Test with an empty matrix
    blaze::DynamicMatrix< double > emptyMatrix(0, 0);

    auto result = Helper::sliceCandles(emptyMatrix, false);

    ASSERT_EQ(result.rows(), 0);
    ASSERT_EQ(result.columns(), 0);
}

TEST_F(SliceCandlesTest, SingleColumnMatrix)
{
    // Test with a single column matrix
    blaze::DynamicMatrix< double > singleColMatrix(300, 1);
    for (size_t i = 0; i < singleColMatrix.rows(); ++i)
    {
        singleColMatrix(i, 0) = i;
    }

    auto result = Helper::sliceCandles(singleColMatrix, false);

    ASSERT_EQ(result.rows(), 240);
    ASSERT_EQ(result.columns(), 1);

    for (size_t i = 0; i < result.rows(); ++i)
    {
        EXPECT_DOUBLE_EQ(result(i, 0), singleColMatrix(singleColMatrix.rows() - 240 + i, 0));
    }
}

TEST_F(SliceCandlesTest, MatrixExactlyWarmupCandlesSize)
{
    // Test with a matrix exactly the size of warmup_candles_num
    blaze::DynamicMatrix< double > exactMatrix(240, 4);
    for (size_t i = 0; i < exactMatrix.rows(); ++i)
    {
        for (size_t j = 0; j < exactMatrix.columns(); ++j)
        {
            exactMatrix(i, j) = i * 100 + j;
        }
    }

    auto result = Helper::sliceCandles(exactMatrix, false);

    ASSERT_EQ(result.rows(), 240);
    ASSERT_EQ(result.columns(), exactMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), exactMatrix(i, j));
        }
    }
}

TEST_F(SliceCandlesTest, StressTest)
{
    // Test with a very large matrix
    blaze::DynamicMatrix< double > largeMatrix(10000, 10);
    for (size_t i = 0; i < largeMatrix.rows(); ++i)
    {
        for (size_t j = 0; j < largeMatrix.columns(); ++j)
        {
            largeMatrix(i, j) = i * 100 + j;
        }
    }

    auto result = Helper::sliceCandles(largeMatrix, false);

    ASSERT_EQ(result.rows(), 240);
    ASSERT_EQ(result.columns(), largeMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_DOUBLE_EQ(result(i, j), largeMatrix(largeMatrix.rows() - 240 + i, j));
        }
    }
}

TEST_F(SliceCandlesTest, NaNValues)
{
    // Test with matrix containing NaN values
    blaze::DynamicMatrix< double > nanMatrix(300, 4);
    for (size_t i = 0; i < nanMatrix.rows(); ++i)
    {
        for (size_t j = 0; j < nanMatrix.columns(); ++j)
        {
            nanMatrix(i, j) = std::numeric_limits< double >::quiet_NaN();
        }
    }

    auto result = Helper::sliceCandles(nanMatrix, false);

    ASSERT_EQ(result.rows(), 240);
    ASSERT_EQ(result.columns(), nanMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_TRUE(std::isnan(result(i, j)));
        }
    }
}

TEST_F(SliceCandlesTest, InfValues)
{
    // Test with matrix containing Inf values
    blaze::DynamicMatrix< double > infMatrix(300, 4);
    for (size_t i = 0; i < infMatrix.rows(); ++i)
    {
        for (size_t j = 0; j < infMatrix.columns(); ++j)
        {
            infMatrix(i, j) = std::numeric_limits< double >::infinity();
        }
    }

    auto result = Helper::sliceCandles(infMatrix, false);

    ASSERT_EQ(result.rows(), 240);
    ASSERT_EQ(result.columns(), infMatrix.columns());

    for (size_t i = 0; i < result.rows(); ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            EXPECT_TRUE(std::isinf(result(i, j)));
        }
    }
}

class GetNextCandleTimestampTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        // Create test candles with known timestamps
        baseCandle    = blaze::DynamicVector< int64_t >(5); // Typical OHLCV format
        baseCandle[0] = 1609459200000;                      // 2021-01-01 00:00:00 UTC in milliseconds
    }

    blaze::DynamicVector< int64_t > baseCandle;
};

TEST_F(GetNextCandleTimestampTest, BasicTimeframes)
{
    // Test basic timeframe calculations
    EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, Enum::Timeframe::MINUTE_1),
              baseCandle[0] + 60'000); // +1 minute

    EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, Enum::Timeframe::HOUR_1),
              baseCandle[0] + 3600'000); // +1 hour

    EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, Enum::Timeframe::DAY_1),
              baseCandle[0] + 86400'000); // +1 day
}

TEST_F(GetNextCandleTimestampTest, EmptyCandle)
{
    // Test with empty candle vector
    blaze::DynamicVector< int64_t > emptyCandle(0);
    EXPECT_THROW(Helper::getNextCandleTimestamp(emptyCandle, Enum::Timeframe::MINUTE_1), std::invalid_argument);
}

TEST_F(GetNextCandleTimestampTest, LargeTimeframes)
{
    // Test with larger timeframes
    EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, Enum::Timeframe::WEEK_1),
              baseCandle[0] + 604800'000); // +1 week

    EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, Enum::Timeframe::MONTH_1),
              baseCandle[0] + 2592000'000); // +30 days
}

TEST_F(GetNextCandleTimestampTest, MaxTimestampBoundary)
{
    // Test near int64_t maximum value to check for overflow
    blaze::DynamicVector< int64_t > maxCandle(5);
    maxCandle[0] = std::numeric_limits< int64_t >::max() - 60'000; // Just below max

    // Should work with 1-minute timeframe
    EXPECT_NO_THROW(Helper::getNextCandleTimestamp(maxCandle, Enum::Timeframe::MINUTE_1));

    // FIXME:
    // Should throw or handle overflow for larger timeframes
    // EXPECT_EQ(Helper::getNextCandleTimestamp(maxCandle, Enum::Timeframe::DAY_1), ???);
}

TEST_F(GetNextCandleTimestampTest, NegativeTimestamp)
{
    // Test with negative timestamp
    blaze::DynamicVector< int64_t > negativeCandle(5);
    negativeCandle[0] = -1000;

    // Should still calculate correctly with negative timestamps
    EXPECT_EQ(Helper::getNextCandleTimestamp(negativeCandle, Enum::Timeframe::MINUTE_1), -1000 + 60'000);
}

TEST_F(GetNextCandleTimestampTest, AllTimeframes)
{
    // Test all available timeframes
    std::vector< std::pair< Enum::Timeframe, int64_t > > timeframes = {{Enum::Timeframe::MINUTE_1, 60'000},
                                                                       {Enum::Timeframe::MINUTE_3, 180'000},
                                                                       {Enum::Timeframe::MINUTE_5, 300'000},
                                                                       {Enum::Timeframe::MINUTE_15, 900'000},
                                                                       {Enum::Timeframe::MINUTE_30, 1800'000},
                                                                       {Enum::Timeframe::MINUTE_45, 2700'000},
                                                                       {Enum::Timeframe::HOUR_1, 3600'000},
                                                                       {Enum::Timeframe::HOUR_2, 7200'000},
                                                                       {Enum::Timeframe::HOUR_3, 10800'000},
                                                                       {Enum::Timeframe::HOUR_4, 14400'000},
                                                                       {Enum::Timeframe::HOUR_6, 21600'000},
                                                                       {Enum::Timeframe::HOUR_8, 28800'000},
                                                                       {Enum::Timeframe::HOUR_12, 43200'000},
                                                                       {Enum::Timeframe::DAY_1, 86400'000},
                                                                       {Enum::Timeframe::DAY_3, 259200'000},
                                                                       {Enum::Timeframe::WEEK_1, 604800'000},
                                                                       {Enum::Timeframe::MONTH_1, 2592000'000}};

    for (const auto &[timeframe, expected_ms] : timeframes)
    {
        EXPECT_EQ(Helper::getNextCandleTimestamp(baseCandle, timeframe), baseCandle[0] + expected_ms)
            << "Failed for timeframe: " << static_cast< int >(timeframe);
    }
}

TEST_F(GetNextCandleTimestampTest, InvalidTimeframe)
{
    // Test with invalid timeframe enum value
    // Note: This assumes Enum::Timeframe has an INVALID or similar value
    Enum::Timeframe invalid_timeframe = static_cast< Enum::Timeframe >(-1);
    EXPECT_THROW(Helper::getNextCandleTimestamp(baseCandle, invalid_timeframe), Exception::InvalidTimeframe);
}

// ... existing code ...
class PrepareQtyBasicTest : public ::testing::Test
{
};

// Tests for prepareQty
TEST_F(PrepareQtyBasicTest, PrepareQtyBasic)
{
    // Test buy/long
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "buy"), 100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "long"), 100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(-100.0, "buy"), 100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(-100.0, "long"), 100.0);

    // Test sell/short
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "sell"), -100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "short"), -100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(-100.0, "sell"), -100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(-100.0, "short"), -100.0);

    // Test close
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "close"), 0.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(-100.0, "close"), 0.0);
}

TEST_F(PrepareQtyBasicTest, PrepareQtyEdgeCases)
{
    // Test case insensitivity
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "BUY"), 100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "SELL"), -100.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(100.0, "CLOSE"), 0.0);

    // Test zero quantity
    EXPECT_DOUBLE_EQ(Helper::prepareQty(0.0, "buy"), 0.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(0.0, "sell"), 0.0);
    EXPECT_DOUBLE_EQ(Helper::prepareQty(0.0, "close"), 0.0);

    // Test invalid side
    EXPECT_THROW(Helper::prepareQty(100.0, "invalid"), std::invalid_argument);
    EXPECT_THROW(Helper::prepareQty(100.0, ""), std::invalid_argument);
}

// FIXME:
// Tests for terminateApp function
// Note: This test is risky as it will exit the program
// TEST_F(HelperUtilsTest, TerminateApp) {
//     // This test would terminate the program, so we'll skip it
//     // EXPECT_EXIT(Helper::terminateApp(), ::testing::ExitedWithCode(1), "");
// }
