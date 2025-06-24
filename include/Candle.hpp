#ifndef CIPHER_CANDLE_HPP
#define CIPHER_CANDLE_HPP

#include "Precompiled.hpp"

#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Timeframe.hpp"
#include <blaze/math/TransposeFlag.h>
#include <blaze/math/dense/DynamicVector.h>

namespace ct
{
namespace candle
{

const size_t _COLUMNS_   = 6;
const size_t _TIMESTAMP_ = 0;
const size_t _OPEN_      = 1;
const size_t _CLOSE_     = 2;
const size_t _HIGH_      = 3;
const size_t _LOW_       = 4;
const size_t _VOLUME_    = 5;

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

class RandomGenerator
{
   public:
    static ct::candle::RandomGenerator& getInstance()
    {
        static ct::candle::RandomGenerator instance;
        return instance;
    }

    int randint(int min, int max);

   private:
    RandomGenerator() : gen_(std::random_device{}()) {}
    RandomGenerator(const RandomGenerator&)            = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;

    std::mt19937 gen_;
};

class CandleGenState;

// Generate a single candle with optional attributes
template < typename T >
blaze::DynamicVector< T, blaze::rowVector > generateFakeCandle(const blaze::DynamicVector< T, blaze::rowVector >& attrs,
                                                               bool reset);

// Generate candles from a list of close prices
// The first candle has the timestamp of "2021-01-01T00:00:00+00:00"
template < typename T >
blaze::DynamicMatrix< T > generateCandlesFromClosePrices(const std::vector< T >& prices, bool reset);

// Generate a range of random candles
template < typename T >
blaze::DynamicMatrix< T > generateRangeCandles(size_t count, bool reset);

template < typename T >
blaze::DynamicVector< T, blaze::rowVector > generateCandleFromOneMinutes(const timeframe::Timeframe& timeframe,
                                                                         const blaze::DynamicMatrix< T >& candles,
                                                                         const bool accept_forming_candles = false);
template < typename T >
int64_t getNextCandleTimestamp(const blaze::DynamicVector< T, blaze::rowVector >& candle,
                               const timeframe::Timeframe& timeframe);

template < typename T >
auto getCandleSource(const blaze::DynamicMatrix< T >& candles, Source source_type = Source::Close)
    -> blaze::DynamicVector< T, blaze::rowVector >;

/**
 * @brief Singleton class for managing candle data
 *
 * This class is responsible for storing and managing candle data for different
 * exchange/symbol/timeframe combinations. It provides methods for adding, retrieving,
 * and generating candles.
 */
class CandlesState
{
   public:
    /**
     * @brief Get the singleton instance of CandlesState
     *
     * @return CandlesState& Reference to the singleton instance
     */
    static CandlesState& getInstance();

    /**
     * @brief Initialize the candles state
     */
    void init(size_t bucket_size);

    /**
     * @brief Reset the candles state
     */
    void reset();

    /**
     * @brief Add a candle to storage
     *
     * @param exchange_name The exchange name
     * @param symbol The trading symbol
     * @param timeframe The candle timeframe
     * @param candle The candle data to add
     */
    void addCandle(const enums::ExchangeName& exchange_name,
                   const std::string& symbol,
                   const timeframe::Timeframe& timeframe,
                   const blaze::DynamicVector< double, blaze::rowVector >& candle,
                   const bool with_execution = true,
                   bool with_generation      = true,
                   const bool with_skip      = true);

    void addCandles(const enums::ExchangeName& exchange_name,
                    const std::string& symbol,
                    const timeframe::Timeframe& timeframe,
                    const blaze::DynamicMatrix< double >& candles,
                    const bool with_generation = true);

    // In few exchanges, there's no candle stream over the WS, for those we have to use cases the trades stream
    void addCandleFromTrade(const double price,
                            const double volume,
                            const enums::ExchangeName& exchange_name,
                            const std::string& symbol);

    static void updatePosition(const enums::ExchangeName& exchange_name, const std::string& symbol, const double price);

    void generateHigherTimeframes(const blaze::DynamicVector< double, blaze::rowVector >& candle,
                                  const enums::ExchangeName& exchange_name,
                                  const std::string& symbol,
                                  const bool with_execution);

    void simulateOrderExecution(const enums::ExchangeName& exchange_name,
                                const std::string& symbol,
                                const timeframe::Timeframe& timeframe,
                                const blaze::DynamicVector< double, blaze::rowVector >& new_candle);

    int formingEstimation(const enums::ExchangeName& exchange_name,
                          const std::string& symbol,
                          const timeframe::Timeframe& timeframe) const;
    /**
     * @brief Get candles for a specific exchange, symbol, and timeframe
     *
     * @param exchange_name The exchange name
     * @param symbol The trading symbol
     * @param timeframe The candle timeframe
     * @return blaze::DynamicMatrix<double> The candles data
     */
    blaze::DynamicMatrix< double > getCandles(const enums::ExchangeName& exchange_name,
                                              const std::string& symbol,
                                              const timeframe::Timeframe& timeframe) const;

    /**
     * @brief Get the current candle for a specific exchange, symbol, and timeframe
     *
     * @param exchange_name The exchange name
     * @param symbol The trading symbol
     * @param timeframe The candle timeframe
     * @return blaze::DynamicVector<double, blaze::rowVector> The current candle data
     */
    blaze::DynamicVector< double, blaze::rowVector > getCurrentCandle(const enums::ExchangeName& exchange_name,
                                                                      const std::string& symbol,
                                                                      const timeframe::Timeframe& timeframe) const;

    void addMultiple1MinCandles(const blaze::DynamicMatrix< double >& candles,
                                const enums::ExchangeName& exchange_name,
                                const std::string& symbol);

    /**
     * @brief Check if all candles are initiated
     *
     * @return bool True if all candles are initiated, false otherwise
     */
    bool areAllInitiated() const;

    /**
     * @brief Set the initiated status for a specific exchange and symbol
     *
     * @param exchange_name The exchange name
     * @param symbol The trading symbol
     * @param status The initiated status
     */
    void setInitiatedPair(const enums::ExchangeName& exchange_name, const std::string& symbol, bool status);

    /**
     * @brief Start the candle generation loop
     *
     * This method starts a background thread that generates new candles
     * at regular intervals to prevent missing candles when no volume is traded.
     */
    void generateNewCandlesLoop();

    void markAllAsInitiated();

   private:
    // Private constructor to enforce singleton pattern
    CandlesState();
    ~CandlesState();

    // Deleted to enforce singleton
    CandlesState(const CandlesState&)            = delete;
    CandlesState& operator=(const CandlesState&) = delete;

    /**
     * @brief Generate an empty candle from a previous candle
     *
     * @param previous_candle The previous candle
     * @param timeframe
     * @return blaze::DynamicVector<double> The generated empty candle
     */
    blaze::DynamicVector< double, blaze::rowVector > generateEmptyCandleFromPreviousCandle(
        const blaze::DynamicVector< double, blaze::rowVector >& previous_candle,
        const timeframe::Timeframe& timeframe) const;

    /**
     * @brief Get the pair key
     *
     * @param exchange_name The exchange name
     * @param symbol The trading symbol
     * @return std::string The pair key
     */
    std::string getPairKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    // Storage for candles data
    std::unordered_map< std::string, std::unique_ptr< datastructure::DynamicBlazeArray< double > > > storage_;

    // Flag to track if all candles are initiated
    bool are_all_initiated_;

    // Map to track initiated pairs
    std::unordered_map< std::string, bool > initiated_pairs_;

    // Thread for candle generation loop
    std::unique_ptr< std::thread > candle_generation_thread_;

    // Flag to control thread execution
    std::atomic< bool > running_;
};

} // namespace candle
} // namespace ct

#endif // CIPHER_CANDLE_HPP
