#ifndef CIPHER_CANDLE_HPP
#define CIPHER_CANDLE_HPP

#include "Cache.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Timeframe.hpp"

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
    static ct::candle::RandomGenerator& getInstance();

    int randint(int min, int max);

   private:
    RandomGenerator() : gen_(std::random_device{}()) {}
    RandomGenerator(const RandomGenerator&)            = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;

    std::mt19937 gen_;
};

class CandleGenState
{
   public:
    static CandleGenState& getInstance();

    void reset();

    void update();

    int64_t getTimestamp() const { return first_timestamp_; }
    int getOpenPrice() const { return open_price_; }
    int getClosePrice() const { return close_price_; }
    int getHighPrice() const { return high_price_; }
    int getLowPrice() const { return low_price_; }

   private:
    CandleGenState() { reset(); }
    CandleGenState(const CandleGenState&)            = delete;
    CandleGenState& operator=(const CandleGenState&) = delete;

    int64_t first_timestamp_;
    int open_price_;
    int close_price_;
    int high_price_;
    int low_price_;
};

struct ExchangeSymbolCandleTimeSpec
{
    enums::ExchangeName exchange_name_;
    std::string symbol_;
    std::string start_date_;
    std::string end_date_;
};

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
 * @brief Print candle information to log
 *
 * @param candle Candle data as Blaze vector
 * @param is_partial Whether the candle is partial
 * @param symbol Trading symbol
 */
void printCandle(const blaze::DynamicVector< double, blaze::rowVector >& candle,
                 bool is_partial,
                 const std::string& symbol);

/**
 * @brief Check if candle is bullish (close >= open)
 *
 * @param candle Candle data as Blaze vector
 * @return bool True if bullish, false otherwise
 */
bool isBullish(const blaze::DynamicVector< double, blaze::rowVector >& candle);

/**
 * @brief Check if candle is bearish (close < open)
 *
 * @param candle Candle data as Blaze vector
 * @return bool True if bearish, false otherwise
 */
bool isBearish(const blaze::DynamicVector< double, blaze::rowVector >& candle);

/**
 * @brief Check if candle includes a specific price
 *
 * @param candle Candle data as Blaze vector
 * @param price Price to check
 * @return bool True if price is within candle range
 */
bool candleIncludesPrice(const blaze::DynamicVector< double, blaze::rowVector >& candle, double price);

/**
 * @brief Split a candle at a specific price
 *
 * @param candle Original candle data
 * @param price Price to split at
 * @return std::pair<blaze::DynamicVector<double, blaze::rowVector>, blaze::DynamicVector<double, blaze::rowVector>>
 *         Pair of earlier and later candles
 */
std::pair< blaze::DynamicVector< double, blaze::rowVector >, blaze::DynamicVector< double, blaze::rowVector > >
splitCandle(const blaze::DynamicVector< double, blaze::rowVector >& candle, double price);

/**
 * @brief Inject warmup candles to state
 *
 * @param candles Matrix of candles to inject
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 */
void injectWarmupCandlesToState(const blaze::DynamicMatrix< double >& candles,
                                const enums::ExchangeName& exchange_name,
                                const std::string& symbol);

// TODO: Doc
blaze::DynamicMatrix< double > getCandlesFromDB(const ct::enums::ExchangeName& exchange_name,
                                                const std::string& symbol,
                                                int64_t start_date_timestamp,
                                                int64_t finish_date_timestamp,
                                                cache::Cache cache,
                                                bool caching = false);

// TODO: Doc
blaze::DynamicMatrix< double > generateCandles(const timeframe::Timeframe& timeframe,
                                               const blaze::DynamicMatrix< double >& trading_candles);

/**
 * @brief Get candles from database with optional caching
 *
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 * @param timeframe Candle timeframe
 * @param start_date_timestamp Start timestamp in milliseconds
 * @param finish_date_timestamp Finish timestamp in milliseconds
 * @param warmup_candles_num Number of warmup candles
 * @param caching Whether to use caching
 * @param b Whether this is for TODO:
 * @return std::pair<blaze::DynamicMatrix<double>, blaze::DynamicMatrix<double>>
 *         Pair of warmup and trading candles
 */
std::pair< blaze::DynamicMatrix< double >, blaze::DynamicMatrix< double > > getCandles(
    const enums::ExchangeName& exchange_name,
    const std::string& symbol,
    const timeframe::Timeframe& timeframe,
    int64_t start_date_timestamp,
    int64_t finish_date_timestamp,
    size_t warmup_candles_num,
    cache::Cache cache,
    bool caching = false,
    bool b       = false); // TODO:

/**
 * @brief Get existing candles grouped by exchange and symbol
 *
 * @return std::vector<ExchangeSymbolCandleTimeSpec> List of candle information
 */
std::vector< ExchangeSymbolCandleTimeSpec > getExistingCandles();

/**
 * @brief Delete all candles for a specific exchange and symbol
 *
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 */
void deleteCandles(const enums::ExchangeName& exchange_name, const std::string& symbol);

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
