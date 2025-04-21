#ifndef CIPHER_CANDLE_HPP
#define CIPHER_CANDLE_HPP

#include <vector>
#include <blaze/Math.h>

namespace CipherCandle
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
blaze::DynamicVector< T > generateFakeCandle(const blaze::DynamicVector< T > &attrs, bool reset);

// Generate candles from a list of close prices
template < typename T >
blaze::DynamicMatrix< T > generateCandlesFromClosePrices(const std::vector< T > &prices, bool reset);

// Generate a range of random candles
template < typename T >
blaze::DynamicMatrix< T > generateRangeCandles(size_t count);

} // namespace CipherCandle

#endif // CIPHER_CANDLE_HPP
