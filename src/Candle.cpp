#include "Candle.hpp"
#include <ctime>

namespace Candle {

int randint(int min, int max) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(min, max);
  return dis(gen);
}

// 2021-01-01T00:00:00+00:00
int64_t FIRST_TIMESTAMP = 1609459080000;
int OPEN_PRICE = randint(40, 100);
int CLOSE_PRICE =
    randint(0, 1) ? randint(OPEN_PRICE, 110) : randint(30, OPEN_PRICE);
int max_price = std::max(OPEN_PRICE, CLOSE_PRICE);
int HIGH_PRICE =
    randint(0, 1) ? max_price : std::max(max_price, max_price + 10);
int min_price = std::min(OPEN_PRICE, CLOSE_PRICE);
int LOW_PRICE = randint(0, 1) ? min_price : std::min(min_price, min_price + 10);

template <typename T>
blaze::DynamicVector<T> fakeCandle(const blaze::DynamicVector<T> &attrs,
                                   bool reset) {

  if (reset) {
    FIRST_TIMESTAMP = 1609459080000;
    OPEN_PRICE = randint(40, 100);
    CLOSE_PRICE = randint(OPEN_PRICE, 110);
    HIGH_PRICE = std::max(OPEN_PRICE, CLOSE_PRICE);
    LOW_PRICE = std::min(OPEN_PRICE, CLOSE_PRICE);
  }

  FIRST_TIMESTAMP += 60000;
  OPEN_PRICE = CLOSE_PRICE;
  CLOSE_PRICE += randint(1, 8);
  HIGH_PRICE = std::max(OPEN_PRICE, CLOSE_PRICE);
  LOW_PRICE = std::min(OPEN_PRICE - 1, CLOSE_PRICE);
  int volume = randint(1, 100);

  blaze::DynamicVector<T> candle(6);
  candle[0] = (attrs[0] != .0) ? attrs[0] : static_cast<T>(FIRST_TIMESTAMP);
  candle[1] = (attrs[1] != .0) ? attrs[1] : static_cast<T>(OPEN_PRICE);
  candle[2] = (attrs[2] != .0) ? attrs[2] : static_cast<T>(CLOSE_PRICE);
  candle[3] = (attrs[3] != .0) ? attrs[3] : static_cast<T>(HIGH_PRICE);
  candle[4] = (attrs[4] != .0) ? attrs[4] : static_cast<T>(LOW_PRICE);
  candle[5] = (attrs[5] != .0) ? attrs[5] : static_cast<T>(volume);

  return candle;
}

// Generates a range of candles from a list of close prices.
// The first candle has the timestamp of "2021-01-01T00:00:00+00:00"
template <typename T>
blaze::DynamicMatrix<T> candlesFromClosePrices(const std::vector<T> &prices,
                                               bool reset) {
  auto attrs = blaze::DynamicVector<T>(6, T{});
  fakeCandle(attrs, reset);

  blaze::DynamicMatrix<T> candles(prices.size(), 6);

  T prev_p;

  for (size_t i = 0; i < prices.size(); ++i) {
    T p = prices[i];

    if (i == 0) {
      prev_p = p - 1;
    }

    FIRST_TIMESTAMP += 60000;

    T open_p = prev_p;
    T close_p = p;
    T high_p = std::max(open_p, close_p);
    T low_p = std::min(open_p, close_p);
    int vol = randint(0, 200);

    candles(i, 0) = static_cast<T>(FIRST_TIMESTAMP);
    candles(i, 1) = open_p;
    candles(i, 2) = close_p;
    candles(i, 3) = high_p;
    candles(i, 4) = low_p;
    candles(i, 5) = static_cast<T>(vol);

    prev_p = p;
  }

  return candles;
}

// Generates a range of candles with random values.
template <typename T> blaze::DynamicMatrix<T> rangeCandles(size_t count) {
  blaze::DynamicMatrix<T> candles(count, 6);
  auto attrs = blaze::DynamicVector<T>(6, T{});

  for (size_t i = 0; i < count; ++i) {
    auto candle = fakeCandle(attrs, false);

    // blaze::row(candles, i) = candle;
    // TODO: fix performance
    for (size_t j = 0; j < 6; ++j) {
      candles(i, j) = candle[j];
    }
  }

  return candles;
}

} // namespace Candle
