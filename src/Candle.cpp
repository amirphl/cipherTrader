#include "Candle.hpp"
#include <algorithm>
#include <random>

// Thread-safe random number generator
class ct::candle::RandomGenerator
{
   public:
    static RandomGenerator &getInstance()
    {
        static RandomGenerator instance;
        return instance;
    }

    int randint(int min, int max)
    {
        std::uniform_int_distribution<> dis(min, max);
        return dis(gen_);
    }

   private:
    RandomGenerator() : gen_(std::random_device{}()) {}
    RandomGenerator(const RandomGenerator &)            = delete;
    RandomGenerator &operator=(const RandomGenerator &) = delete;

    std::mt19937 gen_;
};

// Thread-safe singleton for candle state
class ct::candle::CandleState
{
   public:
    static CandleState &getInstance()
    {
        static CandleState instance;
        return instance;
    }

    void reset()
    {
        first_timestamp_ = 1609459080000; // 2021-01-01T00:00:00+00:00
        open_price_      = RandomGenerator::getInstance().randint(40, 100);
        close_price_     = RandomGenerator::getInstance().randint(0, 1)
                               ? RandomGenerator::getInstance().randint(open_price_, 110)
                               : RandomGenerator::getInstance().randint(30, open_price_);

        const int max_price = std::max(open_price_, close_price_);
        high_price_ = RandomGenerator::getInstance().randint(0, 1) ? max_price : std::max(max_price, max_price + 10);

        const int min_price = std::min(open_price_, close_price_);
        low_price_ = RandomGenerator::getInstance().randint(0, 1) ? min_price : std::min(min_price, min_price + 10);
    }

    void update()
    {
        first_timestamp_ += 60000;
        open_price_ = close_price_;
        close_price_ += RandomGenerator::getInstance().randint(1, 8);
        high_price_ = std::max(open_price_, close_price_);
        low_price_  = std::min(open_price_ - 1, close_price_);
    }

    int64_t getTimestamp() const { return first_timestamp_; }
    int getOpenPrice() const { return open_price_; }
    int getClosePrice() const { return close_price_; }
    int getHighPrice() const { return high_price_; }
    int getLowPrice() const { return low_price_; }

   private:
    CandleState() { reset(); }
    CandleState(const CandleState &)            = delete;
    CandleState &operator=(const CandleState &) = delete;

    int64_t first_timestamp_;
    int open_price_;
    int close_price_;
    int high_price_;
    int low_price_;
};

// Thread-safe random number generation
int ct::candle::randint(int min, int max)
{
    return RandomGenerator::getInstance().randint(min, max);
}

// Generate a single candle with optional attributes
template < typename T >
blaze::DynamicVector< T > ct::candle::generateFakeCandle(const blaze::DynamicVector< T > &attrs, bool reset)
{
    auto &state = CandleState::getInstance();
    if (reset)
    {
        state.reset();
    }
    state.update();

    const int volume = randint(1, 100);

    blaze::DynamicVector< T > candle(6);
    candle[0] = (attrs[0] != T{}) ? attrs[0] : static_cast< T >(state.getTimestamp());
    candle[1] = (attrs[1] != T{}) ? attrs[1] : static_cast< T >(state.getOpenPrice());
    candle[2] = (attrs[2] != T{}) ? attrs[2] : static_cast< T >(state.getClosePrice());
    candle[3] = (attrs[3] != T{}) ? attrs[3] : static_cast< T >(state.getHighPrice());
    candle[4] = (attrs[4] != T{}) ? attrs[4] : static_cast< T >(state.getLowPrice());
    candle[5] = (attrs[5] != T{}) ? attrs[5] : static_cast< T >(volume);

    return candle;
}

// Generate candles from a list of close prices
template < typename T >
blaze::DynamicMatrix< T > ct::candle::generateCandlesFromClosePrices(const std::vector< T > &prices, bool reset)
{
    if (prices.empty())
    {
        return blaze::DynamicMatrix< T >(0, 6);
    }

    auto &state = CandleState::getInstance();
    if (reset)
    {
        state.reset();
    }

    blaze::DynamicMatrix< T > candles(prices.size(), 6);
    T prev_price = prices[0] - 1;

    for (size_t i = 0; i < prices.size(); ++i)
    {
        const T current_price = prices[i];
        state.update();

        const T open_price  = prev_price;
        const T close_price = current_price;
        const T high_price  = std::max(open_price, close_price);
        const T low_price   = std::min(open_price, close_price);
        const T volume      = static_cast< T >(randint(0, 200));

        candles(i, 0) = static_cast< T >(state.getTimestamp());
        candles(i, 1) = open_price;
        candles(i, 2) = close_price;
        candles(i, 3) = high_price;
        candles(i, 4) = low_price;
        candles(i, 5) = volume;

        prev_price = current_price;
    }

    return candles;
}

// Generate a range of random candles
template < typename T >
blaze::DynamicMatrix< T > ct::candle::generateRangeCandles(size_t count)
{
    if (count == 0)
    {
        return blaze::DynamicMatrix< T >(0, 6);
    }

    blaze::DynamicMatrix< T > candles(count, 6);
    blaze::DynamicVector< T > attrs(6, T{});

    auto &state = CandleState::getInstance();
    state.reset();

    for (size_t i = 0; i < count; ++i)
    {
        auto candle = generateFakeCandle(attrs, false);
        // blaze::row(candles, i) =
        //     candle; // Use blaze's row assignment for better performance
        // Use subassignment instead of row assignment
        // blaze::subvector(blaze::row(candles, i), 0, 6) = candle;
        // Use column-wise copy instead of row assignment
        for (size_t j = 0; j < 6; ++j)
        {
            candles(i, j) = candle[j];
        }
    }

    return candles;
}

// Explicit template instantiations for common types
template blaze::DynamicVector< double > ct::candle::generateFakeCandle(const blaze::DynamicVector< double > &, bool);
template blaze::DynamicVector< float > ct::candle::generateFakeCandle(const blaze::DynamicVector< float > &, bool);
template blaze::DynamicMatrix< double > ct::candle::generateCandlesFromClosePrices(const std::vector< double > &, bool);
template blaze::DynamicMatrix< float > ct::candle::generateCandlesFromClosePrices(const std::vector< float > &, bool);
template blaze::DynamicMatrix< double > ct::candle::generateRangeCandles(size_t);
template blaze::DynamicMatrix< float > ct::candle::generateRangeCandles(size_t);
