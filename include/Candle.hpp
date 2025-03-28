#ifndef CANDLE_HPP
#define CANDLE_HPP

#include <vector>
#include <blaze/Math.h>

namespace Candle
{

enum class Source
{
    Close,
    High,
    Low,
    Open,
    Volume,
    HL2,
    HLC3,
    OHLC4
};

// Forward declarations of internal classes
class RandomGenerator;
class CandleState;

// Thread-safe random number generation
int randint(int min, int max);

// Generate a single candle with optional attributes
template < typename T >
blaze::DynamicVector< T > fakeCandle(const blaze::DynamicVector< T > &attrs, bool reset);

// Generate candles from a list of close prices
template < typename T >
blaze::DynamicMatrix< T > candlesFromClosePrices(const std::vector< T > &prices, bool reset);

// Generate a range of random candles
template < typename T >
blaze::DynamicMatrix< T > rangeCandles(size_t count);

// Explicit template instantiations for common types
extern template blaze::DynamicVector< double > fakeCandle(const blaze::DynamicVector< double > &, bool);
extern template blaze::DynamicVector< float > fakeCandle(const blaze::DynamicVector< float > &, bool);
extern template blaze::DynamicMatrix< double > candlesFromClosePrices(const std::vector< double > &, bool);
extern template blaze::DynamicMatrix< float > candlesFromClosePrices(const std::vector< float > &, bool);
extern template blaze::DynamicMatrix< double > rangeCandles(size_t);
extern template blaze::DynamicMatrix< float > rangeCandles(size_t);

} // namespace Candle

#endif // CANDLE_HPP
