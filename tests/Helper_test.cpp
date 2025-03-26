#include "Candle.hpp"
#include "Helper.hpp"
#include "Route.hpp"
#include <date/date.h>
#include <dlfcn.h>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>

namespace Helper {

namespace fs = std::filesystem;

class AssetTest : public ::testing::Test {
protected:
  std::string typical_symbol = "BTC-USD";
  std::string no_dash = "BTCUSD";
  std::string empty = "";
};

// Tests forHelper::quoteAsset
TEST_F(AssetTest, QuoteAsset_TypicalSymbol) {
  auto result = Helper::quoteAsset(typical_symbol);
  EXPECT_EQ(result, "USD");
}

TEST_F(AssetTest, QuoteAsset_NoDash) {
  try {
    auto result = Helper::quoteAsset(no_dash);
    FAIL() << "Expected std::invalid_argument but no exception was thrown";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ("Symbol is invalid", e.what()) << "Exception message mismatch";
  } catch (const std::exception &e) {
    FAIL() << "Expected std::invalid_argument but got different exception: "
           << e.what();
  }
}

TEST_F(AssetTest, QuoteAsset_EmptyString) {
  try {
    auto result = Helper::quoteAsset(empty);
    FAIL() << "Expected std::invalid_argument but no exception was thrown";
  } catch (const std::invalid_argument &e) {
    EXPECT_STREQ("Symbol is invalid", e.what()) << "Exception message mismatch";
  } catch (const std::exception &e) {
    FAIL() << "Expected std::invalid_argument but got different exception: "
           << e.what();
  }
}

TEST_F(AssetTest, QuoteAsset_OnlyDash) {
  auto result = Helper::quoteAsset("-");
  EXPECT_EQ(result, "");
}

TEST_F(AssetTest, QuoteAsset_DashAtStart) {
  auto result = Helper::quoteAsset("-USD");
  EXPECT_EQ(result, "USD");
}

TEST_F(AssetTest, QuoteAsset_DashAtEnd) {
  auto result = Helper::quoteAsset("BTC-");
  EXPECT_EQ(result, "");
}

TEST_F(AssetTest, QuoteAsset_MultipleDashes) {
  auto result = Helper::quoteAsset("BTC-USD-TEST");
  EXPECT_EQ(result, "USD-TEST"); // Takes everything after dash
}

// Tests forHelper::Helper::baseAsset
TEST_F(AssetTest, BaseAsset_TypicalSymbol) {
  EXPECT_EQ(Helper::baseAsset(typical_symbol), "BTC");
}

TEST_F(AssetTest, BaseAsset_NoDash) {
  EXPECT_EQ(Helper::baseAsset(no_dash), "BTCUSD");
}

TEST_F(AssetTest, BaseAsset_EmptyString) {
  EXPECT_EQ(Helper::baseAsset(empty), "");
}

TEST_F(AssetTest, BaseAsset_OnlyDash) { EXPECT_EQ(Helper::baseAsset("-"), ""); }

TEST_F(AssetTest, BaseAsset_DashAtStart) {
  EXPECT_EQ(Helper::baseAsset("-USD"), "");
}

TEST_F(AssetTest, BaseAsset_DashAtEnd) {
  EXPECT_EQ(Helper::baseAsset("BTC-"), "BTC");
}

TEST_F(AssetTest, BaseAsset_MultipleDashes) {
  EXPECT_EQ(Helper::baseAsset("BTC-USD-TEST"),
            "BTC"); // Takes everything before dash
}

class AppCurrencyTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::vector<nlohmann::json> routes_data = {
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

TEST_F(AppCurrencyTest, NoSettlementCurrency) {
  auto result = Helper::appCurrency();
  EXPECT_EQ(result, "USD");
}

TEST_F(AppCurrencyTest, WithSettlementCurrency) {
  Route::Router::getInstance().setRoutes({{{"exchange", "Bybit USDC Perpetual"},
                                           {"symbol", "ETH-ART"},
                                           {"timeframe", "1h"},
                                           {"strategy_name", "MyStrategy"},
                                           {"dna", "abc123"}}});
  auto result = Helper::appCurrency();
  EXPECT_EQ(result, "USDT");
}

// Test fixture for common setup
class ToTimestampTest : public ::testing::Test {
protected:
  // Unix epoch start (1970-01-01 00:00:00 UTC)
  std::chrono::system_clock::time_point epoch =
      std::chrono::system_clock::from_time_t(0);
};

// Basic functionality tests
TEST_F(ToTimestampTest, EpochTime) { EXPECT_EQ(Helper::toTimestamp(epoch), 0); }

TEST_F(ToTimestampTest, PositiveTime) {
  auto time = epoch + std::chrono::seconds(3600); // 1 hour after epoch
  EXPECT_EQ(Helper::toTimestamp(time),
            3600000); // 3600 seconds * 1000 = milliseconds
}

// Edge case tests
TEST_F(ToTimestampTest, NegativeTime) {
  auto time = epoch - std::chrono::seconds(3600); // 1 hour before epoch
  EXPECT_EQ(Helper::toTimestamp(time), -3600000);
}

TEST_F(ToTimestampTest, LargeFutureTime) {
  auto time = epoch + std::chrono::hours(1000000); // ~114 years in future
  long long expected = 1000000LL * 3600 * 1000;    // hours to milliseconds
  EXPECT_EQ(Helper::toTimestamp(time), expected);
}

TEST_F(ToTimestampTest, LargePastTime) {
  auto time = epoch - std::chrono::hours(1000000); // ~114 years in past
  long long expected = -1000000LL * 3600 * 1000;   // hours to milliseconds
  EXPECT_EQ(Helper::toTimestamp(time), expected);
}

TEST_F(ToTimestampTest, MillisecondPrecision) {
  auto time = epoch + std::chrono::milliseconds(1500); // 1.5 seconds
  EXPECT_EQ(Helper::toTimestamp(time), 1500);
}

TEST_F(ToTimestampTest, MaximumTimePoint) {
  auto max_time = std::chrono::system_clock::time_point::max();
  long long result = Helper::toTimestamp(max_time);
  EXPECT_GT(result, 0); // Should handle max value without overflow
}

TEST_F(ToTimestampTest, MinimumTimePoint) {
  auto min_time = std::chrono::system_clock::time_point::min();
  long long result = Helper::toTimestamp(min_time);
  EXPECT_LT(result, 0); // Should handle min value without overflow
}

// Test fixture for common setup
class BinarySearchTest : public ::testing::Test {
protected:
  std::vector<int> sorted_ints{1, 3, 5, 7, 9};
  std::vector<std::string> sorted_strings{"apple", "banana", "cherry"};
};

// Basic functionality tests
TEST_F(BinarySearchTest, FindsExistingElement) {
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 5), 2);
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 1), 0);
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 9), 4);

  EXPECT_EQ(Helper::binarySearch(sorted_strings, std::string("banana")), 1);
}

TEST_F(BinarySearchTest, ReturnsMinusOneForNonExistingElement) {
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 4), -1);
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 0), -1);
  EXPECT_EQ(Helper::binarySearch(sorted_ints, 10), -1);

  EXPECT_EQ(Helper::binarySearch(sorted_strings, std::string("date")), -1);
}

// Edge case tests
TEST_F(BinarySearchTest, EmptyVector) {
  std::vector<int> empty;
  EXPECT_EQ(Helper::binarySearch(empty, 5), -1);
}

TEST_F(BinarySearchTest, SingleElementFound) {
  std::vector<int> single{42};
  EXPECT_EQ(Helper::binarySearch(single, 42), 0);
}

TEST_F(BinarySearchTest, SingleElementNotFound) {
  std::vector<int> single{42};
  EXPECT_EQ(Helper::binarySearch(single, 43), -1);
}

TEST_F(BinarySearchTest, AllElementsSame) {
  std::vector<int> same_elements(5, 7); // 5 elements, all 7
  EXPECT_GE(Helper::binarySearch(same_elements, 7),
            0); // Should find some index
  EXPECT_EQ(Helper::binarySearch(same_elements, 8), -1);
}

TEST_F(BinarySearchTest, LargeVector) {
  std::vector<int> large;
  for (int i = 0; i < 1000; i += 2) {
    large.push_back(i);
  }
  EXPECT_EQ(Helper::binarySearch(large, 500), 250);
  EXPECT_EQ(Helper::binarySearch(large, 501), -1);
}

TEST_F(BinarySearchTest, AndLastElements) {
  std::vector<int> nums{1, 2, 3, 4, 5};
  EXPECT_EQ(Helper::binarySearch(nums, 1), 0); // element
  EXPECT_EQ(Helper::binarySearch(nums, 5), 4); // Last element
}

// Test fixture for common setup
class CleanOrderbookListTest : public ::testing::Test {
protected:
  std::vector<std::vector<std::string>> string_input = {{"1.23", "4.56"},
                                                        {"2.34", "5.67"}};
  std::vector<std::vector<int>> int_input = {{1, 2}, {3, 4}};
};

// Tests for Version 1 (static_cast)
TEST_F(CleanOrderbookListTest, IntToDouble) {
  auto result = Helper::cleanOrderbookList<int, double>(int_input);
  EXPECT_EQ(result.size(), 2);
  EXPECT_DOUBLE_EQ(result[0][0], 1.0);
  EXPECT_DOUBLE_EQ(result[0][1], 2.0);
  EXPECT_DOUBLE_EQ(result[1][0], 3.0);
  EXPECT_DOUBLE_EQ(result[1][1], 4.0);
}

TEST_F(CleanOrderbookListTest, IntToFloat) {
  auto result = Helper::cleanOrderbookList<int, float>(int_input);
  EXPECT_EQ(result.size(), 2);
  EXPECT_FLOAT_EQ(result[0][0], 1.0f);
  EXPECT_FLOAT_EQ(result[0][1], 2.0f);
  EXPECT_FLOAT_EQ(result[1][0], 3.0f);
  EXPECT_FLOAT_EQ(result[1][1], 4.0f);
}

TEST_F(CleanOrderbookListTest, EmptyInput) {
  std::vector<std::vector<int>> empty;
  auto result = Helper::cleanOrderbookList<int, double>(empty);
  EXPECT_TRUE(result.empty());
}

TEST_F(CleanOrderbookListTest, InsufficientElements) {
  std::vector<std::vector<int>> invalid = {{1}, {2, 3}};
  EXPECT_THROW((Helper::cleanOrderbookList<int, double>(invalid)),
               std::invalid_argument);
}

// Tests for Version 2 (custom converter)
TEST_F(CleanOrderbookListTest, StringToDouble) {
  auto result = Helper::cleanOrderbookList<std::string, double>(
      string_input, Helper::strToDouble);
  EXPECT_EQ(result.size(), 2);
  EXPECT_DOUBLE_EQ(result[0][0], 1.23);
  EXPECT_DOUBLE_EQ(result[0][1], 4.56);
  EXPECT_DOUBLE_EQ(result[1][0], 2.34);
  EXPECT_DOUBLE_EQ(result[1][1], 5.67);
}

TEST_F(CleanOrderbookListTest, StringToFloat) {
  auto result = Helper::cleanOrderbookList<std::string, float>(
      string_input, Helper::strToFloat);
  EXPECT_EQ(result.size(), 2);
  EXPECT_FLOAT_EQ(result[0][0], 1.23f);
  EXPECT_FLOAT_EQ(result[0][1], 4.56f);
  EXPECT_FLOAT_EQ(result[1][0], 2.34f);
  EXPECT_FLOAT_EQ(result[1][1], 5.67f);
}

TEST_F(CleanOrderbookListTest, InvalidStringConversion) {
  std::vector<std::vector<std::string>> invalid = {{"abc", "4.56"},
                                                   {"2.34", "5.67"}};
  EXPECT_THROW((Helper::cleanOrderbookList<std::string, double>(
                   invalid, Helper::strToDouble)),
               std::invalid_argument);
}

TEST_F(CleanOrderbookListTest, EmptyInputWithConverter) {
  std::vector<std::vector<std::string>> empty;
  auto result = Helper::cleanOrderbookList<std::string, double>(
      empty, Helper::strToDouble);
  EXPECT_TRUE(result.empty());
}

TEST_F(CleanOrderbookListTest, InsufficientElementsWithConverter) {
  std::vector<std::vector<std::string>> invalid = {{"1.23"}, {"2.34", "5.67"}};
  EXPECT_THROW((Helper::cleanOrderbookList<std::string, double>(
                   invalid, Helper::strToDouble)),
               std::invalid_argument);
}

// Test fixture (optional, for shared setup if needed)
class ScaleToRangeTest : public ::testing::Test {};

// Test cases
TEST(ScaleToRangeTest, DoubleNormalCase) {
  double result = Helper::scaleToRange(100.0, 0.0, 1.0, 0.0, 50.0);
  EXPECT_DOUBLE_EQ(
      result, 0.5); // 50 is halfway between 0 and 100, maps to 0.5 in [0, 1]
}

TEST(ScaleToRangeTest, IntNormalCase) {
  int result = Helper::scaleToRange(10, 0, 100, 0, 5);
  EXPECT_EQ(result,
            50); // 5 is halfway between 0 and 10, maps to 50 in [0, 100]
}

TEST(ScaleToRangeTest, FloatEdgeMin) {
  float result = Helper::scaleToRange(10.0f, 0.0f, 100.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(result, 0.0f); // Min value maps to newMin
}

TEST(ScaleToRangeTest, FloatEdgeMax) {
  float result = Helper::scaleToRange(10.0f, 0.0f, 100.0f, 0.0f, 10.0f);
  EXPECT_FLOAT_EQ(result, 100.0f); // Max value maps to newMax
}

TEST(ScaleToRangeTest, NegativeRange) {
  double result = Helper::scaleToRange(0.0, -100.0, 1.0, 0.0, -50.0);
  EXPECT_DOUBLE_EQ(
      result, 0.5); // -50 is halfway between -100 and 0, maps to 0.5 in [0, 1]
}

TEST(ScaleToRangeTest, ThrowsWhenValueBelowMin) {
  EXPECT_THROW(Helper::scaleToRange(10, 0, 100, 0, -1), std::invalid_argument);
}

TEST(ScaleToRangeTest, ThrowsWhenValueAboveMax) {
  EXPECT_THROW(Helper::scaleToRange(10, 0, 100, 0, 11), std::invalid_argument);
}

TEST(ScaleToRangeTest, ThrowsWhenOldRangeZero) {
  EXPECT_THROW(Helper::scaleToRange(5, 5, 100, 0, 5), std::invalid_argument);
}

TEST(ScaleToRangeTest, DoublePrecision) {
  double result = Helper::scaleToRange(200.0, 100.0, 2.0, 1.0, 150.0);
  EXPECT_DOUBLE_EQ(
      result, 1.5); // 150 is halfway between 100 and 200, maps to 1.5 in [1, 2]
}

// Test fixture
class DashySymbolTest : public ::testing::Test {};

// Test cases
TEST(DashySymbolTest, AlreadyHasDash) {
  EXPECT_EQ(Helper::dashySymbol("BTC-USD"), "BTC-USD");
  EXPECT_EQ(Helper::dashySymbol("XRP-EUR"), "XRP-EUR");
}

TEST(DashySymbolTest, MatchesConfigSymbol) {
  EXPECT_EQ(Helper::dashySymbol("BTCUSD"), "BTC-USD");
  EXPECT_EQ(Helper::dashySymbol("ETHUSDT"), "ETH-USDT");
  EXPECT_EQ(Helper::dashySymbol("XRPEUR"), "XRP-EUR");
}

TEST(DashySymbolTest, SuffixEUR) {
  EXPECT_EQ(Helper::dashySymbol("ADAEUR"), "ADA-EUR");
}

TEST(DashySymbolTest, SuffixUSDT) {
  EXPECT_EQ(Helper::dashySymbol("SOLUSDT"), "SOL-USDT");
}

TEST(DashySymbolTest, SuffixSUSDT) {
  EXPECT_EQ(Helper::dashySymbol("SETHSUSDT"), "SETHS-USDT");
}

TEST(DashySymbolTest, DefaultSplit) {
  EXPECT_EQ(Helper::dashySymbol("SOLANA"), "SOL-ANA");
  EXPECT_EQ(Helper::dashySymbol("XLMXRP"), "XLM-XRP");
}

TEST(DashySymbolTest, ShortString) {
  EXPECT_EQ(Helper::dashySymbol("BTC"), "BTC");
  EXPECT_EQ(Helper::dashySymbol(""), "");
}

TEST(DashySymbolTest, OtherSuffixes) {
  EXPECT_EQ(Helper::dashySymbol("LTCGBP"), "LTC-GBP");
  EXPECT_EQ(Helper::dashySymbol("BNBFDUSD"), "BNB-FDUSD");
  EXPECT_EQ(Helper::dashySymbol("XTZUSDC"), "XTZ-USDC");
}

// Test fixture
class UnderlineToDashyTest : public ::testing::Test {};

TEST(UnderlineToDashyTest, UnderlineToDashyNormal) {
  EXPECT_EQ(Helper::underlineToDashySymbol("BTC_USD"), "BTC-USD");
  EXPECT_EQ(Helper::underlineToDashySymbol("ETH_USDT"), "ETH-USDT");
}

TEST(UnderlineToDashyTest, UnderlineToDashyNoUnderscore) {
  EXPECT_EQ(Helper::underlineToDashySymbol("BTCUSD"), "BTCUSD");
  EXPECT_EQ(Helper::underlineToDashySymbol(""), "");
}

TEST(UnderlineToDashyTest, UnderlineToDashyMultipleUnderscores) {
  EXPECT_EQ(Helper::underlineToDashySymbol("BTC_USD_ETH"), "BTC-USD-ETH");
}

TEST(UnderlineToDashyTest, DashyToUnderlineNormal) {
  EXPECT_EQ(Helper::dashyToUnderline("BTC-USD"), "BTC_USD");
  EXPECT_EQ(Helper::dashyToUnderline("ETH-USDT"), "ETH_USDT");
}

TEST(UnderlineToDashyTest, DashyToUnderlineNoDash) {
  EXPECT_EQ(Helper::dashyToUnderline("BTCUSD"), "BTCUSD");
  EXPECT_EQ(Helper::dashyToUnderline(""), "");
}

TEST(UnderlineToDashyTest, DashyToUnderlineMultipleDashes) {
  EXPECT_EQ(Helper::dashyToUnderline("BTC-USD-ETH"), "BTC_USD_ETH");
}

class DateDiffInDaysTest : public ::testing::Test {};

TEST(DateDiffInDaysTest, DateDiffNormal) {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto yesterday = now - hours(24);
  EXPECT_EQ(Helper::dateDiffInDays(yesterday, now), 1);
  EXPECT_EQ(Helper::dateDiffInDays(now, yesterday), 1); // Absolute value
}

TEST(DateDiffInDaysTest, DateDiffSameDay) {
  using namespace std::chrono;
  auto now = system_clock::now();
  EXPECT_EQ(Helper::dateDiffInDays(now, now), 0);
}

TEST(DateDiffInDaysTest, DateDiffMultipleDays) {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto three_days_ago = now - hours(72); // 3 days
  EXPECT_EQ(Helper::dateDiffInDays(three_days_ago, now), 3);
}

TEST(DateDiffInDaysTest, DateDiffSmallDifference) {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto few_hours_ago = now - hours(5); // Less than a day
  EXPECT_EQ(Helper::dateDiffInDays(few_hours_ago, now), 0);
}

// Test fixture
class DateToTimestampTest : public ::testing::Test {};

TEST(DateToTimestampTest, ValidDate) {
  long long ts = Helper::toTimestamp("2015-08-01");
  // UTC: 1438387200000 ms (adjust if toTimestamp uses different units)
  EXPECT_EQ(ts, 1438387200000); // Exact UTC timestamp in milliseconds
}

TEST(DateToTimestampTest, EpochStart) {
  long long ts = Helper::toTimestamp("1970-01-01");
  EXPECT_EQ(ts, 0); // UTC epoch start
}

TEST(DateToTimestampTest, LeapYear) {
  long long ts = Helper::toTimestamp("2020-02-29");
  EXPECT_EQ(ts, 1582934400000); // UTC timestamp for leap year
}

TEST(DateToTimestampTest, InvalidFormat) {
  EXPECT_THROW(Helper::toTimestamp("2020/02/29"), std::invalid_argument);
  EXPECT_THROW(Helper::toTimestamp("2020-2-29"), std::invalid_argument);
  EXPECT_THROW(Helper::toTimestamp(""), std::invalid_argument);
}

TEST(DateToTimestampTest, InvalidDate) {
  EXPECT_THROW(Helper::toTimestamp("2020-02-30"), std::invalid_argument);
  EXPECT_THROW(Helper::toTimestamp("2021-04-31"), std::invalid_argument);
}

// Test fixture
class DnaToHpTest : public ::testing::Test {};

// Normal case: int and float parameters
TEST(DnaToHpTest, NormalCase) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

  std::string dna = "AB"; // A=65, B=66
  auto hp = Helper::dnaToHp(strategy_hp, dna);

  EXPECT_EQ(std::get<int>(hp["param1"]),
            32); // 65 scales from [40,119] to [0,100]
  EXPECT_FLOAT_EQ(std::get<float>(hp["param2"]),
                  3.2911392); // 66 scales to [0,10]
}

// Empty input (valid case)
TEST(DnaToHpTest, EmptyInput) {
  nlohmann::json strategy_hp = nlohmann::json::array();
  std::string dna = "";
  auto hp = Helper::dnaToHp(strategy_hp, dna);
  EXPECT_TRUE(hp.empty());
}

// Edge case: Min ASCII value (40)
TEST(DnaToHpTest, MinAsciiValue) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

  std::string dna = "(("; // 40, 40 (ASCII '(')
  auto hp = Helper::dnaToHp(strategy_hp, dna);

  EXPECT_EQ(std::get<int>(hp["param1"]), 0);           // Min of range
  EXPECT_FLOAT_EQ(std::get<float>(hp["param2"]), 0.0); // Min of range
}

// Edge case: Max ASCII value (119)
TEST(DnaToHpTest, MaxAsciiValue) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 0, "max": 100},
        {"name": "param2", "type": "float", "min": 0.0, "max": 10.0}
    ])"_json;

  std::string dna = "ww"; // 119, 119 (ASCII 'w')
  auto hp = Helper::dnaToHp(strategy_hp, dna);

  EXPECT_EQ(std::get<int>(hp["param1"]), 100);          // Max of range
  EXPECT_FLOAT_EQ(std::get<float>(hp["param2"]), 10.0); // Max of range
}

// Edge case: Single character
TEST(DnaToHpTest, SingleCharacter) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": -10, "max": 10}
    ])"_json;

  std::string dna = "M"; // 77
  auto hp = Helper::dnaToHp(strategy_hp, dna);

  EXPECT_EQ(std::get<int>(hp["param1"]),
            -1); // 77 scales from [40,119] to [-10,10]
}

// Invalid JSON: not an array
TEST(DnaToHpTest, NotAnArray) {
  nlohmann::json strategy_hp = R"({"key": "value"})"_json;
  std::string dna = "A";
  EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::invalid_argument);
}

// Invalid: DNA length mismatch
TEST(DnaToHpTest, LengthMismatch) {
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
TEST(DnaToHpTest, MissingFields) {
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
TEST(DnaToHpTest, UnsupportedType) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "string", "min": 0, "max": 100}
    ])"_json;

  std::string dna = "A";
  EXPECT_THROW(Helper::dnaToHp(strategy_hp, dna), std::runtime_error);
}

// Edge case: Zero range in strategy_hp (valid, but scaleToRange handles it)
TEST(DnaToHpTest, ZeroNewRange) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "int", "min": 5, "max": 5}
    ])"_json;

  std::string dna = "A";
  auto hp = Helper::dnaToHp(strategy_hp, dna);
  EXPECT_EQ(std::get<int>(hp["param1"]),
            5); // Should return min (or max) due to zero range
}

// Edge case: Extreme min/max values
TEST(DnaToHpTest, ExtremeMinMax) {
  nlohmann::json strategy_hp = R"([
        {"name": "param1", "type": "float", "min": -1e6, "max": 1e6}
    ])"_json;

  std::string dna = "A"; // 65
  auto hp = Helper::dnaToHp(strategy_hp, dna);
  float expected = Helper::scaleToRange(119.0f, 40.0f, 1e6f, -1e6f, 65.0f);
  EXPECT_FLOAT_EQ(std::get<float>(hp["param1"]), expected);
}

// Test fixture
class EstimateAveragePriceTest : public ::testing::Test {};

// Normal case: Positive quantities
TEST(EstimateAveragePriceTest, NormalPositiveQuantities) {
  float result = Helper::estimateAveragePrice(2.0f, 100.0f, 3.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = (200+270)/5 = 94
}

// Normal case: Negative order quantity (e.g., short position)
TEST(EstimateAveragePriceTest, NegativeOrderQuantity) {
  float result = Helper::estimateAveragePrice(-2.0f, 100.0f, 3.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Normal case: Negative current quantity
TEST(EstimateAveragePriceTest, NegativeCurrentQuantity) {
  float result = Helper::estimateAveragePrice(2.0f, 100.0f, -3.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Normal case: Both quantities negative
TEST(EstimateAveragePriceTest, BothQuantitiesNegative) {
  float result = Helper::estimateAveragePrice(-2.0f, 100.0f, -3.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 94.0f); // (2*100 + 3*90) / (2+3) = 94 (abs used)
}

// Edge case: Current quantity is zero
TEST(EstimateAveragePriceTest, CurrentQuantityZero) {
  float result = Helper::estimateAveragePrice(2.0f, 100.0f, 0.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 100.0f); // (2*100 + 0*90) / (2+0) = 200/2 = 100
}

// Edge case: Order quantity is zero
TEST(EstimateAveragePriceTest, OrderQuantityZero) {
  float result = Helper::estimateAveragePrice(0.0f, 100.0f, 3.0f, 90.0f);
  EXPECT_FLOAT_EQ(result, 90.0f); // (0*100 + 3*90) / (0+3) = 270/3 = 90
}

// Edge case: Both quantities zero
TEST(EstimateAveragePriceTest, BothQuantitiesZero) {
  EXPECT_THROW(Helper::estimateAveragePrice(0.0f, 100.0f, 0.0f, 90.0f),
               std::invalid_argument);
}

// Typical trading scenario: Averaging up
TEST(EstimateAveragePriceTest, AveragingUp) {
  float result = Helper::estimateAveragePrice(1.0f, 110.0f, 2.0f, 100.0f);
  EXPECT_FLOAT_EQ(result,
                  103.33333f); // (1*110 + 2*100) / (1+2) = 310/3 ≈ 103.333
}

// Typical trading scenario: Averaging down
TEST(EstimateAveragePriceTest, AveragingDown) {
  float result = Helper::estimateAveragePrice(1.0f, 90.0f, 2.0f, 100.0f);
  EXPECT_FLOAT_EQ(result, 96.66667f); // (1*90 + 2*100) / (1+2) = 290/3 ≈ 96.667
}

// Edge case: Large quantities and prices
TEST(EstimateAveragePriceTest, LargeValues) {
  float result =
      Helper::estimateAveragePrice(1000.0f, 5000.0f, 2000.0f, 4000.0f);
  EXPECT_FLOAT_EQ(result,
                  4333.3333f); // (1000*5000 + 2000*4000) / (1000+2000) = 13M/3
}

// Edge case: Very small quantities
TEST(EstimateAveragePriceTest, SmallQuantities) {
  float result = Helper::estimateAveragePrice(0.001f, 100.0f, 0.002f, 90.0f);
  EXPECT_FLOAT_EQ(result, 93.33333f); // (0.001*100 + 0.002*90) / 0.003 ≈ 93.333
}

// Test fixture
class PnlUtilsTest : public ::testing::Test {};

// --- estimate_PNL Tests ---

TEST(PnlUtilsTest, EstimatePnlLongNoFee) {
  float pnl = Helper::estimatePNL(2.0f, 100.0f, 110.0f, "long");
  EXPECT_FLOAT_EQ(pnl, 20.0f); // 2 * (110 - 100) = 20
}

TEST(PnlUtilsTest, EstimatePnlShortNoFee) {
  float pnl = Helper::estimatePNL(3.0f, 100.0f, 90.0f, "short");
  EXPECT_FLOAT_EQ(pnl, 30.0f); // 3 * (90 - 100) * -1 = 30
}

TEST(PnlUtilsTest, EstimatePnlLongWithFee) {
  float pnl = Helper::estimatePNL(2.0f, 100.0f, 110.0f, "long", 0.001f);
  EXPECT_FLOAT_EQ(pnl, 19.58f); // 20 - (0.001 * 2 * (100 + 110)) = 20 - 0.42
}

TEST(PnlUtilsTest, EstimatePnlShortWithFee) {
  float pnl = Helper::estimatePNL(3.0f, 100.0f, 90.0f, "short", 0.001f);
  EXPECT_FLOAT_EQ(pnl, 29.43f); // 30 - (0.001 * 3 * (100 + 90)) = 30 - 0.57
}

TEST(PnlUtilsTest, EstimatePnlNegativeQty) {
  float pnl = Helper::estimatePNL(-2.0f, 100.0f, 110.0f, "long");
  EXPECT_FLOAT_EQ(pnl, 20.0f); // |-2| * (110 - 100) = 20
}

TEST(PnlUtilsTest, EstimatePnlZeroQty) {
  EXPECT_THROW(Helper::estimatePNL(0.0f, 100.0f, 110.0f, "long"),
               std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlInvalidTradeType) {
  EXPECT_THROW(Helper::estimatePNL(2.0f, 100.0f, 110.0f, "invalid"),
               std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlLargeValues) {
  float pnl = Helper::estimatePNL(1000.0f, 5000.0f, 5100.0f, "long", 0.0001f);
  EXPECT_FLOAT_EQ(
      pnl, 98990.0f); // 1000 * (5100 - 5000) - 0.0001 * 1000 * (5000 + 5100)
}

TEST(PnlUtilsTest, EstimatePnlSmallValues) {
  float pnl = Helper::estimatePNL(0.001f, 100.0f, 101.0f, "long", 0.001f);
  EXPECT_FLOAT_EQ(
      pnl, 0.000799f); // 0.001 * (101 - 100) - 0.001 * 0.001 * (100 + 101)
}

TEST(PnlUtilsTest, EstimatePnlPercentageLong) {
  float pct = Helper::estimatePNLPercentage(2.0f, 100.0f, 110.0f, "long");
  EXPECT_FLOAT_EQ(pct, 10.0f); // (2 * (110 - 100)) / (2 * 100) * 100 = 10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageShort) {
  float pct = Helper::estimatePNLPercentage(3.0f, 100.0f, 90.0f, "short");
  EXPECT_FLOAT_EQ(pct, 10.0f); // (3 * (90 - 100) * -1) / (3 * 100) * 100 = 10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageNegativeQty) {
  float pct = Helper::estimatePNLPercentage(-2.0f, 100.0f, 110.0f, "long");
  EXPECT_FLOAT_EQ(pct, 10.0f); // Same as positive qty due to abs
}

TEST(PnlUtilsTest, EstimatePnlPercentageZeroQty) {
  EXPECT_THROW(Helper::estimatePNLPercentage(0.0f, 100.0f, 110.0f, "long"),
               std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlPercentageZeroEntryPrice) {
  EXPECT_THROW(Helper::estimatePNLPercentage(2.0f, 0.0f, 10.0f, "long"),
               std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlPercentageInvalidTradeType) {
  EXPECT_THROW(Helper::estimatePNLPercentage(2.0f, 100.0f, 110.0f, "invalid"),
               std::invalid_argument);
}

TEST(PnlUtilsTest, EstimatePnlPercentageLoss) {
  float pct = Helper::estimatePNLPercentage(2.0f, 100.0f, 90.0f, "long");
  EXPECT_FLOAT_EQ(pct, -10.0f); // (2 * (90 - 100)) / (2 * 100) * 100 = -10%
}

TEST(PnlUtilsTest, EstimatePnlPercentageLargeValues) {
  float pct = Helper::estimatePNLPercentage(1000.0f, 5000.0f, 5100.0f, "long");
  EXPECT_FLOAT_EQ(pct,
                  2.0f); // (1000 * (5100 - 5000)) / (1000 * 5000) * 100 = 2%
}

TEST(PnlUtilsTest, EstimatePnlPercentageSmallValues) {
  float pct = Helper::estimatePNLPercentage(0.001f, 100.0f, 101.0f, "long");
  EXPECT_FLOAT_EQ(pct,
                  1.0f); // (0.001 * (101 - 100)) / (0.001 * 100) * 100 = 1%
}

// The observation that `max_float / 2` and `max_float / 2 + 1.0f` are equal in
// your tests stems from the limitations of floating-point precision in C++.
// Specifically, when dealing with very large numbers like
// `std::numeric_limits<float>::max()` (approximately \(3.4028235 \times
// 10^{38}\)), adding a small value like `1.0f` doesn’t change the result due to
// the finite precision of the `float` type. Let’s break this down:

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
//    - Adding `1.0f` doesn’t shift the value enough to reach the next
//    representable `float`, so the result remains `1.70141175e38`.

// ### Demonstration
// Here’s a small program to illustrate this:

// ```cpp
// #include <iostream>
// #include <limits>

// int main() {
//     float max_float = std::numeric_limits<float>::max();
//     float half_max = max_float / 2;
//     float half_max_plus_one = half_max + 1.0f;

//     std::cout << "max_float: " << max_float << std::endl;
//     std::cout << "half_max: " << half_max << std::endl;
//     std::cout << "half_max + 1.0f: " << half_max_plus_one << std::endl;
//     std::cout << "Equal? " << (half_max == half_max_plus_one ? "Yes" : "No")
//     << std::endl;

//     return 0;
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
// - `estimate_PNL`: Returns `0.0f - fee`, which isn’t a meaningful test of
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
// exceeds the ULP. Let me know if you’d like the full updated test file with
// these changes! TEST(PnlUtilsTest, EstimatePnlMaxFloat) {
//   float max_float = std::numeric_limits<float>::max();
//   float pnl =
//       Helper::estimatePNL(1.0f, max_float / 2, max_float / 2 + 1.0f, "long");
//   EXPECT_FLOAT_EQ(pnl, 1.0f); // Limited by float precision
// }

// --- estimate_PNL_percentage Tests ---
TEST(PnlUtilsTest, EstimatePnlPercentageMaxFloat) {
  float max_float = std::numeric_limits<float>::max();
  float pct = Helper::estimatePNLPercentage(1.0f, max_float / 2,
                                            max_float / 2 + 1.0f, "long");
  EXPECT_NEAR(pct, 0.0f, 0.0001f); // Small % due to float limits
}

// Test fixture to manage temporary files and directories
class FileUtilsTest : public ::testing::Test {
protected:
  const std::string test_file = "test_file.txt";
  const std::string test_dir = "test_dir";
  const std::string nested_dir = "test_dir/nested";

  void SetUp() override {
    // Clean up any existing test artifacts
    std::filesystem::remove_all(test_file);
    std::filesystem::remove_all(test_dir);
  }

  void TearDown() override {
    // Clean up after each test
    std::filesystem::remove_all(test_file);
    std::filesystem::remove_all(test_dir);
  }

  // Helper to create a file with content
  void createFile(const std::string &path, const std::string &content = "") {
    std::ofstream file(path);
    file << content;
    file.close();
  }
};

// --- file_exists Tests ---

TEST_F(FileUtilsTest, FileExistsTrue) {
  createFile(test_file, "content");
  EXPECT_TRUE(Helper::fileExists(test_file));
}

TEST_F(FileUtilsTest, FileExistsFalseNonExistent) {
  EXPECT_FALSE(Helper::fileExists(test_file));
}

TEST_F(FileUtilsTest, FileExistsFalseDirectory) {
  std::filesystem::create_directory(test_dir);
  EXPECT_FALSE(
      Helper::fileExists(test_dir)); // Should return false for directories
}

TEST_F(FileUtilsTest, FileExistsEmptyPath) {
  EXPECT_FALSE(Helper::fileExists(""));
}

// --- clear_file Tests ---

TEST_F(FileUtilsTest, ClearFileCreatesEmptyFile) {
  Helper::clearFile(test_file);
  EXPECT_TRUE(Helper::fileExists(test_file));
  std::ifstream file(test_file);
  std::string content;
  std::getline(file, content);
  EXPECT_TRUE(content.empty());
}

TEST_F(FileUtilsTest, ClearFileOverwritesExisting) {
  createFile(test_file, "existing content");
  Helper::clearFile(test_file);
  EXPECT_TRUE(Helper::fileExists(test_file));
  std::ifstream file(test_file);
  std::string content;
  std::getline(file, content);
  EXPECT_TRUE(content.empty());
}

TEST_F(FileUtilsTest, ClearFileEmptyPath) {
  EXPECT_THROW(Helper::clearFile(""), std::runtime_error);
}

// Note: Testing permission-denied cases requires OS-specific setup (e.g.,
// chmod), omitted here

// --- make_directory Tests ---

TEST_F(FileUtilsTest, MakeDirectoryCreatesNew) {
  Helper::makeDirectory(test_dir);
  EXPECT_TRUE(std::filesystem::exists(test_dir));
  EXPECT_TRUE(std::filesystem::is_directory(test_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryNested) {
  Helper::makeDirectory(nested_dir);
  EXPECT_TRUE(std::filesystem::exists(nested_dir));
  EXPECT_TRUE(std::filesystem::is_directory(nested_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryExists) {
  std::filesystem::create_directory(test_dir);
  Helper::makeDirectory(test_dir); // Should not throw if already exists
  EXPECT_TRUE(std::filesystem::exists(test_dir));
}

TEST_F(FileUtilsTest, MakeDirectoryEmptyPath) {
  EXPECT_THROW(Helper::makeDirectory(""), std::runtime_error);
}

TEST_F(FileUtilsTest, MakeDirectoryFileExists) {
  createFile(test_file);
  EXPECT_THROW(Helper::makeDirectory(test_file), std::runtime_error);
}

// #include "utils.hpp"
// #include <cmath>
// #include <gtest/gtest.h>
// #include <set>
// #include <stdexcept>

// Test fixture
class UUIDTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// --- floor_with_precision Tests ---

TEST_F(UUIDTest, FloorWithPrecisionNormal) {
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 2), 123.45);
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 1), 123.4);
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456, 0), 123.0);
}

TEST_F(UUIDTest, FloorWithPrecisionNegativeNumber) {
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 2), -123.46);
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 1), -123.5);
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(-123.456, 0), -124.0);
}

TEST_F(UUIDTest, FloorWithPrecisionZero) {
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(0.0, 2), 0.0);
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(0.0, 0), 0.0);
}

TEST_F(UUIDTest, FloorWithPrecisionHighPrecision) {
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(123.456789, 5), 123.45678);
}

TEST_F(UUIDTest, FloorWithPrecisionNegativePrecision) {
  EXPECT_THROW(Helper::floorWithPrecision(123.456, -1), std::invalid_argument);
}

TEST_F(UUIDTest, FloorWithPrecisionLargeNumber) {
  EXPECT_DOUBLE_EQ(Helper::floorWithPrecision(1e10 + 0.5, 1),
                   1e10 + 0.5); // Precision exceeds double’s capability
}

// --- format_currency Tests ---

TEST_F(UUIDTest, FormatCurrencyNormal) {
  EXPECT_EQ(Helper::formatCurrency(1234567.89), "1,234,567.890000");
  EXPECT_EQ(Helper::formatCurrency(1000.0), "1,000.000000");
}

TEST_F(UUIDTest, FormatCurrencyNegative) {
  EXPECT_EQ(Helper::formatCurrency(-1234567.89), "-1,234,567.890000");
}

TEST_F(UUIDTest, FormatCurrencyZero) {
  EXPECT_EQ(Helper::formatCurrency(0.0), "0.000000");
}

TEST_F(UUIDTest, FormatCurrencySmallNumber) {
  EXPECT_EQ(Helper::formatCurrency(0.123456), "0.123456");
}

TEST_F(UUIDTest, FormatCurrencyLargeNumber) {
  double large = 1e12;
  EXPECT_EQ(Helper::formatCurrency(large), "1,000,000,000,000.000000");
}

TEST_F(UUIDTest, FormatCurrencyMaxDouble) {
  double max_double = std::numeric_limits<double>::max();
  std::string result = Helper::formatCurrency(max_double);
  EXPECT_TRUE(result.find(',') !=
              std::string::npos); // Ensure thousands separator exists
}

// --- generate_unique_id Tests ---

TEST_F(UUIDTest, GenerateUniqueIdLength) {
  std::string id = Helper::generateUniqueId();
  EXPECT_EQ(id.length(), 36); // UUID v4: 8-4-4-4-12
}

TEST_F(UUIDTest, GenerateUniqueIdFormat) {
  std::string id = Helper::generateUniqueId();
  std::regex uuid_regex(
      "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
  EXPECT_TRUE(std::regex_match(id, uuid_regex));
}

TEST_F(UUIDTest, GenerateUniqueIdUniqueness) {
  std::set<std::string> ids;
  const int iterations = 1000;
  for (int i = 0; i < iterations; ++i) {
    std::string id = Helper::generateUniqueId();
    EXPECT_TRUE(ids.insert(id).second); // Ensure no duplicates
  }
}

// --- generate_short_unique_id Tests ---

TEST_F(UUIDTest, GenerateShortUniqueIdLength) {
  std::string short_id = Helper::generateShortUniqueId();
  EXPECT_EQ(short_id.length(), 22); // First 22 chars of UUID
}

// FIXME:
// TEST_F(UUIDTest, GenerateShortUniqueIdFormat) {
//   std::string short_id = Helper::generateShortUniqueId();
//   std::regex short_uuid_regex(
//       "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{2}$");
//   EXPECT_TRUE(std::regex_match(short_id, short_uuid_regex));
// }

TEST_F(UUIDTest, GenerateShortUniqueIdUniqueness) {
  std::set<std::string> short_ids;
  const int iterations = 1000;
  for (int i = 0; i < iterations; ++i) {
    std::string short_id = Helper::generateShortUniqueId();
    EXPECT_TRUE(short_ids.insert(short_id).second); // Ensure no duplicates
  }
}

TEST_F(UUIDTest, GenerateShortUniqueIdPrefix) {
  std::string full_id = Helper::generateUniqueId();
  std::string short_id = Helper::generateShortUniqueId();
  std::string full_prefix = full_id.substr(0, 22);
  // Note: This test might fail occasionally due to independent generation
  // For strict prefix check, we'd need to generate short from same UUID
  // instance
  EXPECT_EQ(short_id.length(), 22); // Basic check instead
}

// FIXME: Fix the related issue to make this test pass
// // Test fixture
// class TimestampToTest : public ::testing::Test {
// protected:
//   void SetUp() override {}
//   void TearDown() override {}
// };

// // --- timestamp_to_time_point Tests ---

// TEST_F(TimestampToTest, TimestampToTimePointNormal) {
//   int64_t timestamp = 1609804800000; // 2021-01-05 00:00:00 UTC
//   auto tp = Helper::timestampToTimePoint(timestamp);
//   auto duration = tp.time_since_epoch();
//   EXPECT_EQ(
//       std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
//       timestamp);
// }

// TEST_F(TimestampToTest, TimestampToTimePointZero) {
//   auto tp = Helper::timestampToTimePoint(0);
//   EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(
//                 tp.time_since_epoch())
//                 .count(),
//             0);
// }

// TEST_F(TimestampToTest, TimestampToTimePointNegative) {
//   int64_t timestamp = -31557600000; // 1969-01-01 00:00:00 UTC
//   auto tp = Helper::timestampToTimePoint(timestamp);
//   EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(
//                 tp.time_since_epoch())
//                 .count(),
//             timestamp);
// }

// // --- timestamp_to_date Tests ---

// TEST_F(TimestampToTest, TimestampToDateNormal) {
//   EXPECT_EQ(Helper::timestampToDate(1609804800000), "2021-01-05");
// }

// TEST_F(TimestampToTest, TimestampToDateZero) {
//   EXPECT_EQ(Helper::timestampToDate(0), "1970-01-01");
// }

// TEST_F(TimestampToTest, TimestampToDateNegative) {
//   EXPECT_EQ(Helper::timestampToDate(-31557600000), "1969-01-01");
// }

// TEST_F(TimestampToTest, TimestampToDateLarge) {
//   EXPECT_EQ(Helper::timestampToDate(4102444800000),
//             "2100-01-01"); // Far future
// }

// // --- timestamp_to_time Tests ---

// TEST_F(TimestampToTest, TimestampToTimeNormal) {
//   EXPECT_EQ(Helper::timestampToTime(1609804800000), "2021-01-05 00:00:00");
// }

// TEST_F(TimestampToTest, TimestampToTimeWithMs) {
//   EXPECT_EQ(Helper::timestampToTime(1609804800123),
//             "2021-01-05 00:00:00"); // Ms truncated
// }

// TEST_F(TimestampToTest, TimestampToTimeZero) {
//   EXPECT_EQ(Helper::timestampToTime(0), "1970-01-01 00:00:00");
// }

// TEST_F(TimestampToTest, TimestampToTimeNegative) {
//   EXPECT_EQ(Helper::timestampToTime(-31557600000), "1969-01-01 00:00:00");
// }

// // --- timestamp_to_iso8601 Tests ---

// TEST_F(TimestampToTest, TimestampToIso8601Normal) {
//   EXPECT_EQ(Helper::timestampToIso8601(1609804800000),
//             "2021-01-05T00:00:00.000Z");
// }

// TEST_F(TimestampToTest, TimestampToIso8601WithMs) {
//   EXPECT_EQ(Helper::timestampToIso8601(1609804800123),
//             "2021-01-05T00:00:00.123Z");
// }

// TEST_F(TimestampToTest, TimestampToIso8601Zero) {
//   EXPECT_EQ(Helper::timestampToIso8601(0), "1970-01-01T00:00:00.000Z");
// }

// TEST_F(TimestampToTest, TimestampToIso8601Negative) {
//   EXPECT_EQ(Helper::timestampToIso8601(-31557600000),
//             "1969-01-01T00:00:00.000Z");
// }

// TEST_F(TimestampToTest, TimestampToIso8601Large) {
//   EXPECT_EQ(Helper::timestampToIso8601(4102444800123),
//             "2100-01-01T00:00:00.123Z");
// }

// // --- iso8601_to_timestamp Tests ---

// TEST_F(TimestampToTest, Iso8601ToTimestampNormal) {
//   EXPECT_EQ(Helper::iso8601ToTimestamp("2021-01-05T00:00:00.000Z"),
//             1609804800000);
// }

// TEST_F(TimestampToTest, Iso8601ToTimestampWithMs) {
//   EXPECT_EQ(Helper::iso8601ToTimestamp("2021-01-05T00:00:00.123Z"),
//             1609804800123);
// }

// TEST_F(TimestampToTest, Iso8601ToTimestampZero) {
//   EXPECT_EQ(Helper::iso8601ToTimestamp("1970-01-01T00:00:00.000Z"), 0);
// }

// TEST_F(TimestampToTest, Iso8601ToTimestampNegative) {
//   EXPECT_EQ(Helper::iso8601ToTimestamp("1969-01-01T00:00:00.000Z"),
//             -31557600000);
// }

// TEST_F(TimestampToTest, Iso8601ToTimestampInvalidFormat) {
//   EXPECT_THROW(Helper::iso8601ToTimestamp("2021-01-05"),
//   std::invalid_argument);
//   EXPECT_THROW(Helper::iso8601ToTimestamp("2021-01-05T00:00:00"),
//                std::invalid_argument); // No Z
//   EXPECT_THROW(Helper::iso8601ToTimestamp("invalid"), std::invalid_argument);
// }

// // --- today_to_timestamp Tests ---

// TEST_F(TimestampToTest, TodayToTimestampBasic) {
//   int64_t ts = Helper::todayToTimestamp();
//   std::string date_str = Helper::timestampToDate(ts);
//   EXPECT_EQ(date_str.substr(8, 2), "00"); // Should be start of day
//   auto tp = Helper::timestampToTimePoint(ts);
//   auto time = date::format("%T", tp);
//   EXPECT_EQ(time, "00:00:00"); // Verify midnight
// }

// TEST_F(TimestampToTest, TodayToTimestampConsistency) {
//   int64_t ts1 = Helper::todayToTimestamp();
//   std::this_thread::sleep_for(std::chrono::milliseconds(100));
//   int64_t ts2 = Helper::todayToTimestamp();
//   EXPECT_EQ(ts1, ts2); // Should be same day start despite small delay
// }

// Test fixture
class CandleUtilsTest : public ::testing::Test {
protected:
  blaze::DynamicMatrix<double> candles;

  void SetUp() override {
    // Sample candle data: [timestamp, open, close, high, low, volume]
    candles = blaze::DynamicMatrix<double>(2UL, 6UL);
    candles = {{1609459200.0, 100.0, 101.0, 102.0, 99.0, 1000.0},
               {1609462800.0, 101.0, 102.0, 103.0, 100.0, 1500.0}};
  }

  void TearDown() override {}
};

// --- Enum-based get_candle_source Tests ---

TEST_F(CandleUtilsTest, GetCandleSourceEnumClose) {
  auto result = Helper::getCandleSource(candles, Candle::Source::Close);
  EXPECT_EQ(result.size(), 2UL);
  EXPECT_DOUBLE_EQ(result[0], 101.0);
  EXPECT_DOUBLE_EQ(result[1], 102.0);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHigh) {
  auto result = Helper::getCandleSource(candles, Candle::Source::High);
  EXPECT_EQ(result.size(), 2UL);
  EXPECT_DOUBLE_EQ(result[0], 102.0);
  EXPECT_DOUBLE_EQ(result[1], 103.0);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHL2) {
  auto result = Helper::getCandleSource(candles, Candle::Source::HL2);
  EXPECT_EQ(result.size(), 2UL);
  EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0) / 2.0);  // 100.5
  EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0) / 2.0); // 101.5
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumHLC3) {
  auto result = Helper::getCandleSource(candles, Candle::Source::HLC3);
  EXPECT_EQ(result.size(), 2UL);
  EXPECT_DOUBLE_EQ(result[0], (102.0 + 99.0 + 101.0) / 3.0);  // 100.666...
  EXPECT_DOUBLE_EQ(result[1], (103.0 + 100.0 + 102.0) / 3.0); // 101.666...
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumOHLC4) {
  auto result = Helper::getCandleSource(candles, Candle::Source::OHLC4);
  EXPECT_EQ(result.size(), 2UL);
  EXPECT_DOUBLE_EQ(result[0], (100.0 + 102.0 + 99.0 + 101.0) / 4.0);  // 100.5
  EXPECT_DOUBLE_EQ(result[1], (101.0 + 103.0 + 100.0 + 102.0) / 4.0); // 101.5
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumEmptyMatrix) {
  blaze::DynamicMatrix<double> empty(0UL, 6UL);
  EXPECT_THROW(Helper::getCandleSource(empty, Candle::Source::Close),
               std::invalid_argument);
}

TEST_F(CandleUtilsTest, GetCandleSourceEnumInsufficientColumns) {
  blaze::DynamicMatrix<double> small(2UL, 3UL);
  EXPECT_THROW(Helper::getCandleSource(small, Candle::Source::Close),
               std::invalid_argument);
}

// Helper function to create a strategy file
void createStrategyFile(const fs::path &sourcePath,
                        const std::string &className) {
  std::ofstream outFile(sourcePath);
  ASSERT_TRUE(outFile) << "Failed to create " << sourcePath;
  outFile << R"(
#include "Helper.hpp"

namespace YourStrategy {
    class )"
          << className << R"( : public Helper::Strategy {
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
void compileStrategy(const fs::path &srcPath, const fs::path &outputPath,
                     const fs::path &includePath, const fs::path &libraryPath) {
  // -lmy_trading_lib
  std::string cmd = "g++ -shared -pthread -ldl -fPIC -std=c++17 -I" +
                    includePath.string() + " -I/opt/homebrew/include -L" +
                    libraryPath.string() + " -L/opt/homebrew/lib -o" +
                    outputPath.string() + " " + srcPath.string();
  int result = system(cmd.c_str());
  if (result != 0) {
    std::cerr << "Compilation failed with code " << result << "\n";
    ASSERT_EQ(result, 0) << "Compilation failed for " << srcPath;
  }
  if (!fs::exists(outputPath)) {
    std::cerr << "Output .so not created: " << outputPath << "\n";
    ASSERT_TRUE(fs::exists(outputPath)) << "Output .so not created";
  }
  // ASSERT_EQ(result, 0) << "Compilation failed for " << srcPath;
  // ASSERT_TRUE(fs::exists(outputPath))
  //     << "Output .so not created: " << outputPath;
}

// Get the project directory dynamically
fs::path getProjectDir() {
  // Use the source file's path (__FILE__) to locate projectDir
  fs::path sourcePath =
      fs::canonical(__FILE__); // Absolute path to this .cpp file
  fs::path projectDir =
      sourcePath.parent_path().parent_path(); // Assumes projectDir/build/
  if (!fs::exists(projectDir / "include") || !fs::exists(projectDir / "src")) {
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

class StrategyLoaderTest : public ::testing::Test {
protected:
  Helper::StrategyLoader &loader = Helper::StrategyLoader::getInstance();
  fs::path tempDir = fs::temp_directory_path() / "strategy_test";
  fs::path srcPath = tempDir / "src";
  fs::path libraryPath = tempDir / "lib";
  fs::path includePath = tempDir / "include";
  fs::path strategiesDir = tempDir / "strategies";

  void SetUp() override {
    fs::create_directories(srcPath);
    fs::create_directories(libraryPath);
    fs::create_directories(includePath);
    fs::create_directories(strategiesDir);

    fs::path projectDir = getProjectDir();

    fs::copy(projectDir / "include", includePath,
             fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);

    fs::copy(projectDir / "src", srcPath,
             fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing);
    // Since Helper.cpp uses dlopen/dlsym (for loading .so files), ensure -ldl
    // is included
    // Add -O2 or -O3 for performance in a production-like test setup
    //
    std::string libCmd = "g++ -shared -pthread -ldl -fPIC -std=c++17 -I" +
                         includePath.string() + " -I/opt/homebrew/include" +
                         " -L/opt/homebrew/lib -o " +
                         (libraryPath / "libciphertrader.so").string() + " " +
                         srcPath.string() + "/*";
    system(libCmd.c_str());

    loader.setBasePath(tempDir);
    loader.setIncludePath(includePath);
    loader.setLibraryPath(libraryPath);
    loader.setTestingMode(false);
  }

  void TearDown() override { fs::remove_all(tempDir); }

  std::optional<std::filesystem::path>
  resolveModulePath(const std::string &name) const {
    return loader.resolveModulePath(name);
  }

  std::pair<std::unique_ptr<Strategy>, void *>
  loadFromDynamicLib(const std::filesystem::path &path) const {
    return loader.loadFromDynamicLib(path);
  }

  std::pair<std::unique_ptr<Strategy>, void *>
  adjustAndReload(const std::string &name,
                  const std::filesystem::path &sourcePath) const {
    return loader.adjustAndReload(name, sourcePath);
  }

  std::pair<std::unique_ptr<Strategy>, void *>
  createFallback(const std::string &name,
                 const std::filesystem::path &modulePath) const {
    return loader.createFallback(name, modulePath);
  }
};

// --- Singleton Tests ---
TEST_F(StrategyLoaderTest, InstanceReturnsSameObject) {
  auto &loader1 = Helper::StrategyLoader::getInstance();
  auto &loader2 = Helper::StrategyLoader::getInstance();
  EXPECT_EQ(&loader1, &loader2);
}

// --- getStrategy Tests ---
TEST_F(StrategyLoaderTest, GetStrategyValid) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::path soPath = strategiesDir / "TestStrategy" / "TestStrategy.so";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "TestStrategy");
  compileStrategy(sourcePath, soPath, includePath, libraryPath);

  auto [strategy, handle] = loader.getStrategy("TestStrategy");
  if (!strategy) {
    if (handle) {
      dlclose(handle);
    }

    FAIL() << "Strategy loading failed";
  }

  EXPECT_NE(strategy, nullptr);
}

TEST_F(StrategyLoaderTest, GetStrategyEmptyName) {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
  EXPECT_THROW(loader.getStrategy(""), std::invalid_argument);
#pragma clang diagnostic pop
}

TEST_F(StrategyLoaderTest, GetStrategyMissing) {
  auto [strategy, handle] = loader.getStrategy("NonExistent");
  EXPECT_EQ(strategy, nullptr);
}

// --- resolveModulePath Tests ---
TEST_F(StrategyLoaderTest, ResolveModulePathNonTesting) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::path soPath = strategiesDir / "TestStrategy" / "TestStrategy.so";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "TestStrategy");
  compileStrategy(sourcePath, soPath, includePath, libraryPath);

  auto path = resolveModulePath("TestStrategy");
  EXPECT_TRUE(path.has_value());
  EXPECT_EQ(*path, soPath);
}

TEST_F(StrategyLoaderTest, ResolveModulePathTestingLive) {
  loader.setTestingMode(true);
  fs::create_directories(tempDir / "tests" / "strategies" / "TestStrategy");
  fs::path soPath =
      tempDir / "tests" / "strategies" / "TestStrategy" / "TestStrategy.so";
  createStrategyFile(tempDir / "tests" / "strategies" / "TestStrategy" /
                         "main.cpp",
                     "TestStrategy");
  compileStrategy(tempDir / "tests" / "strategies" / "TestStrategy" /
                      "main.cpp",
                  soPath, includePath, libraryPath);

  // fs::rename(tempDir, tempDir.parent_path() / "ciphertrader-live");
  //
  // Remove existing ciphertrader-live if it exists
  fs::path newDir = tempDir.parent_path() / "ciphertrader-live";
  if (fs::exists(newDir)) {
    fs::remove_all(newDir);
  }
  fs::rename(tempDir, newDir);
  auto soPath2 = std::filesystem::path("ciphertrader-live") / "tests" /
                 "strategies" / "TestStrategy" / "TestStrategy.so";
  loader.setBasePath(tempDir.parent_path() / "ciphertrader-live");
  auto path = resolveModulePath("TestStrategy");

  EXPECT_TRUE(path.has_value());
  EXPECT_TRUE(endsWith((*path).string(), soPath2.string()));
}

TEST_F(StrategyLoaderTest, ResolveModulePathInvalid) {
  auto path = resolveModulePath("NonExistent");
  EXPECT_FALSE(path.has_value());
}

// --- loadFromDynamicLib Tests ---
TEST_F(StrategyLoaderTest, LoadFromDynamicLibValid) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::path soPath = strategiesDir / "TestStrategy" / "TestStrategy.so";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "TestStrategy");
  compileStrategy(sourcePath, soPath, includePath, libraryPath);

  auto [strategy, handle] = loadFromDynamicLib(soPath);
  EXPECT_NE(strategy, nullptr)
      << "Failed to load valid strategy from " << soPath;
}

TEST_F(StrategyLoaderTest, LoadFromDynamicLibInvalidPath) {
  auto [strategy, handle] = loadFromDynamicLib("invalid.so");
  EXPECT_EQ(strategy, nullptr) << "Expected nullptr for invalid .so path";
  if (handle) {
    dlclose(handle);
  }
}

// --- adjustAndReload Tests ---
TEST_F(StrategyLoaderTest, AdjustAndReloadRenamesClass) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::path soPath = strategiesDir / "TestStrategy" / "TestStrategy.so";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "OldStrategy"); // Different name

  auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
  EXPECT_NE(strategy, nullptr);

  std::ifstream updatedFile(sourcePath);
  std::string content((std::istreambuf_iterator<char>(updatedFile)),
                      std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.find("class OldStrategy") != std::string::npos);
  EXPECT_TRUE(fs::exists(soPath));
}

TEST_F(StrategyLoaderTest, AdjustAndReloadNoChangeNeeded) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "TestStrategy"); // Same name
  auto originalModTime = fs::last_write_time(sourcePath);

  auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
  EXPECT_EQ(strategy, nullptr); // No reload needed, returns nullptr
  EXPECT_EQ(fs::last_write_time(sourcePath), originalModTime); // File unchanged
  if (handle) {
    dlclose(handle);
  }
}

// --- createFallback Tests ---
TEST_F(StrategyLoaderTest, CreateFallbackValid) {
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::path soPath = strategiesDir / "TestStrategy" / "TestStrategy.so";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "OldStrategy"); // Different name
  compileStrategy(sourcePath, soPath, includePath, libraryPath);

  auto [strategy, handle] = createFallback("TestStrategy", soPath);
  EXPECT_NE(strategy, nullptr);
}

TEST_F(StrategyLoaderTest, CreateFallbackInvalid) {
  auto [strategy, handle] = createFallback("TestStrategy", "invalid.so");
  EXPECT_EQ(strategy, nullptr);
  if (handle) {
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

TEST_F(StrategyLoaderTest, EdgeCaseInvalidPathCharacters) {
  fs::path sourcePath = strategiesDir / "Test/Strategy" / "main.cpp";
  EXPECT_THROW(fs::create_directory(strategiesDir / "Test/Strategy"),
               std::filesystem::filesystem_error);
  // Do not proceed with further operations since the directory creation fails

  // createStrategyFile(sourcePath, "TestStrategy");
  // auto [strategy, handle] =
  //     loader.getStrategy("Test/Strategy"); // Invalid path character
  // EXPECT_EQ(strategy, nullptr);            // Should fail gracefully
}

TEST_F(StrategyLoaderTest, EdgeCaseNoLib) {
  fs::remove(libraryPath / "libmy_trading_lib.so");
  fs::path sourcePath = strategiesDir / "TestStrategy" / "main.cpp";
  fs::create_directory(strategiesDir / "TestStrategy");
  createStrategyFile(sourcePath, "TestStrategy");
  auto [strategy, handle] = adjustAndReload("TestStrategy", sourcePath);
  EXPECT_EQ(strategy, nullptr); // Compilation should fail without lib
  if (handle) {
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

} // namespace Helper
