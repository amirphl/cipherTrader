#ifndef CANDLE_HPP
#define CANDLE_HPP

#include <blaze/Math.h>
#include <ctime>
#include <vector>

namespace Candle {

int randint(int min, int max);

extern int64_t FIRST_TIMESTAMP;
extern int OPEN_PRICE;
extern int CLOSE_PRICE;
extern int HIGH_PRICE;
extern int LOW_PRICE;

template <typename T>
blaze::DynamicVector<T> fakeCandle(const blaze::DynamicVector<T> &attrs,
                                   bool reset = false);

template <typename T>
blaze::DynamicMatrix<T> candlesFromClosePrices(const std::vector<T> &prices,
                                               bool reset = true);

template <typename T> blaze::DynamicMatrix<T> rangeCandles(size_t count);

} // namespace Candle

#endif // CANDLE_HPP
