#include "Helper.hpp"
#include "Route.hpp"
#include <functional>
#include <gtest/gtest.h>

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
  EXPECT_EQ(Helper::toTimestamp(time), 1000); // Should truncate to 1 second
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
