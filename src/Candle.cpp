#include "Precompiled.hpp"

#include "Candle.hpp"
#include "Config.hpp"
#include "DB.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Exchange.hpp"
#include "Helper.hpp"
#include "Logger.hpp"
#include "Order.hpp"
#include "Position.hpp"
#include "Route.hpp"
#include "Timeframe.hpp"

int ct::candle::RandomGenerator::randint(int min, int max)
{
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen_);
}

// Thread-safe singleton for candle state
class ct::candle::CandleGenState
{
   public:
    static CandleGenState& getInstance()
    {
        static CandleGenState instance;
        return instance;
    }

    void reset()
    {
        auto& rg         = RandomGenerator::getInstance();
        first_timestamp_ = 1609459080000; // 2021-01-01T00:00:00+00:00
        open_price_      = rg.randint(40, 100);
        close_price_     = rg.randint(0, 1) ? rg.randint(open_price_, 110) : rg.randint(30, open_price_);

        const auto max_price = std::max(open_price_, close_price_);
        high_price_          = rg.randint(0, 1) ? max_price : std::max(max_price, max_price + 10);

        const auto min_price = std::min(open_price_, close_price_);
        low_price_           = rg.randint(0, 1) ? min_price : std::min(min_price, min_price + 10);
    }

    void update()
    {
        first_timestamp_ += 60'000;
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
    CandleGenState() { reset(); }
    CandleGenState(const CandleGenState&)            = delete;
    CandleGenState& operator=(const CandleGenState&) = delete;

    int64_t first_timestamp_;
    int open_price_;
    int close_price_;
    int high_price_;
    int low_price_;
};

// Generate a single candle with optional attributes
template < typename T >
blaze::DynamicVector< T, blaze::rowVector > ct::candle::generateFakeCandle(
    const blaze::DynamicVector< T, blaze::rowVector >& attrs, bool reset)
{
    auto& state = CandleGenState::getInstance();
    if (reset)
    {
        state.reset();
    }
    state.update();

    const auto volume = RandomGenerator::getInstance().randint(1, 100);

    blaze::DynamicVector< T, blaze::rowVector > candle(_COLUMNS_);
    candle[_TIMESTAMP_] = (attrs[_TIMESTAMP_] != T{}) ? attrs[_TIMESTAMP_] : static_cast< T >(state.getTimestamp());
    candle[_OPEN_]      = (attrs[_OPEN_] != T{}) ? attrs[_OPEN_] : static_cast< T >(state.getOpenPrice());
    candle[_CLOSE_]     = (attrs[_CLOSE_] != T{}) ? attrs[_CLOSE_] : static_cast< T >(state.getClosePrice());
    candle[_HIGH_]      = (attrs[_HIGH_] != T{}) ? attrs[_HIGH_] : static_cast< T >(state.getHighPrice());
    candle[_LOW_]       = (attrs[_LOW_] != T{}) ? attrs[_LOW_] : static_cast< T >(state.getLowPrice());
    candle[_VOLUME_]    = (attrs[_VOLUME_] != T{}) ? attrs[_VOLUME_] : static_cast< T >(volume);

    return candle;
}

// Generate candles from a list of close prices
template < typename T >
blaze::DynamicMatrix< T > ct::candle::generateCandlesFromClosePrices(const std::vector< T >& prices, bool reset)
{
    if (prices.empty())
    {
        return blaze::DynamicMatrix< T >(0, _COLUMNS_);
    }

    auto& state = CandleGenState::getInstance();
    if (reset)
    {
        state.reset();
    }

    blaze::DynamicMatrix< T > candles(prices.size(), _COLUMNS_);
    T prev_price = prices[0] - 1;

    for (size_t i = 0; i < prices.size(); ++i)
    {
        const T current_price = prices[i];
        state.update();

        const T open_price  = prev_price;
        const T close_price = current_price;
        const T high_price  = std::max(open_price, close_price);
        const T low_price   = std::min(open_price, close_price);
        const T volume      = static_cast< T >(RandomGenerator::getInstance().randint(0, 200));

        candles(i, _TIMESTAMP_) = static_cast< T >(state.getTimestamp());
        candles(i, _OPEN_)      = open_price;
        candles(i, _CLOSE_)     = close_price;
        candles(i, _HIGH_)      = high_price;
        candles(i, _LOW_)       = low_price;
        candles(i, _VOLUME_)    = volume;

        prev_price = current_price;
    }

    return candles;
}

// Generate a range of random candles
template < typename T >
blaze::DynamicMatrix< T > ct::candle::generateRangeCandles(size_t count, bool reset)
{
    if (count == 0)
    {
        return blaze::DynamicMatrix< T >(0, _COLUMNS_);
    }

    blaze::DynamicMatrix< T > candles(count, _COLUMNS_);
    blaze::DynamicVector< T, blaze::rowVector > attrs(_COLUMNS_, T{});

    auto& state = CandleGenState::getInstance();
    if (reset)
    {
        state.reset();
    }

    for (size_t i = 0; i < count; ++i)
    {
        blaze::row(candles, i) = generateFakeCandle(attrs, false);
    }

    return candles;
}

template < typename T >
blaze::DynamicVector< T, blaze::rowVector > ct::candle::generateCandleFromOneMinutes(
    const timeframe::Timeframe& timeframe, const blaze::DynamicMatrix< T >& candles, const bool accept_forming_candles)
{
    auto numCandles = static_cast< int64_t >(candles.rows());
    auto minutes    = timeframe::convertTimeframeToOneMinutes(timeframe);

    if (!accept_forming_candles && numCandles != minutes)
    {
        std::ostringstream oss;
        oss << "Sent only " << numCandles << " candles but " << minutes << " is required to create a "
            << timeframe::toString(timeframe) << " candle.";

        throw std::invalid_argument(oss.str());
    }

    return blaze::DynamicVector< double, blaze::rowVector >{
        candles(0, _TIMESTAMP_),
        candles(0, _OPEN_),
        candles(numCandles - 1, _CLOSE_),
        static_cast< double >(blaze::max(blaze::column(candles, _HIGH_))),
        static_cast< double >(blaze::min(blaze::column(candles, _LOW_))),
        static_cast< double >(blaze::sum(blaze::column(candles, _VOLUME_))),
    };
}

template < typename T >
int64_t ct::candle::getNextCandleTimestamp(const blaze::DynamicVector< T, blaze::rowVector >& candle,
                                           const timeframe::Timeframe& timeframe)
{
    if (candle.size() == 0)
    {
        throw std::invalid_argument("Input candle is empty");
    }

    return static_cast< int64_t >(candle[_TIMESTAMP_] + timeframe::convertTimeframeToOneMinutes(timeframe) * 60'000);
}

template < typename T >
auto ct::candle::getCandleSource(const blaze::DynamicMatrix< T >& candles, Source source_type)
    -> blaze::DynamicVector< T, blaze::rowVector >
{
    // Check matrix dimensions (expect at least 6 columns: timestamp, open, close,
    // high, low, volume)
    if (candles.columns() < _COLUMNS_)
    {
        throw std::invalid_argument("Candles matrix must have at least 6 columns");
    }
    if (candles.rows() == 0)
    {
        throw std::invalid_argument("Candles matrix must have at least one row");
    }

    // auto toConstColumn = [](const blaze::DynamicVector< T >& col) -> blaze::Column< const blaze::DynamicMatrix< T > >
    // {
    //     blaze::DynamicMatrix< T > mat(col.size(), 1);
    //     blaze::column(mat, 0)                = col;
    //     const blaze::DynamicMatrix< T > mat2 = mat;
    //     return blaze::column(mat2, 0);
    // };

    switch (source_type)
    {
        case Source::Close:
            return blaze::trans(blaze::column(candles, _CLOSE_)); // Close prices
        case Source::High:
            return blaze::trans(blaze::column(candles, _HIGH_)); // High prices
        case Source::Low:
            return blaze::trans(blaze::column(candles, _LOW_)); // Low prices
        case Source::Open:
            return blaze::trans(blaze::column(candles, _OPEN_)); // Open prices
        case Source::Volume:
            return blaze::trans(blaze::column(candles, _VOLUME_)); // Volume
        case Source::HL2:
        {
            auto hl2 = (blaze::column(candles, _HIGH_) + blaze::column(candles, _LOW_)) / 2.0;

            return blaze::trans(hl2);
        }
        case Source::HLC3:
        {
            auto hlc3 =
                (blaze::column(candles, _HIGH_) + blaze::column(candles, _LOW_) + blaze::column(candles, _CLOSE_)) /
                3.0;

            return blaze::trans(hlc3);
        }
        case Source::OHLC4:
        {
            auto ohlc4 = (blaze::column(candles, _OPEN_) + blaze::column(candles, _HIGH_) +
                          blaze::column(candles, _LOW_) + blaze::column(candles, _CLOSE_)) /
                         4.0;

            return blaze::trans(ohlc4);
        }
        default:
            throw std::invalid_argument("Unknown candle source type");
    }
}

ct::candle::CandlesState& ct::candle::CandlesState::getInstance()
{
    static CandlesState instance;
    return instance;
}

ct::candle::CandlesState::CandlesState() : are_all_initiated_(false), running_(false) {}

ct::candle::CandlesState::~CandlesState()
{
    // Stop the candle generation thread if running
    if (running_.load())
    {
        running_.store(false);
        if (candle_generation_thread_ && candle_generation_thread_->joinable())
        {
            candle_generation_thread_->join();
        }
    }
}

void ct::candle::CandlesState::init(size_t bucket_size)
{
    reset();

    auto routes = route::Router::getInstance().formattedRoutes();

    for (const auto& route : routes)
    {
        // Get current candle
        auto exchangeName = route["exchange_name"].get< enums::ExchangeName >();
        auto symbol       = route["symbol"].get< std::string >();

        std::string key = helper::generateCompositeKey(exchangeName, symbol, timeframe::Timeframe::MINUTE_1);

        std::array< size_t, 2 > shape{bucket_size, _COLUMNS_};
        storage_.at(key) = std::make_unique< datastructure::DynamicBlazeArray< double > >(shape);

        auto& config               = config::Config::getInstance();
        auto consideringTimeframes = config.getValue< std::vector< std::string > >("app_considering_timeframes");

        for (const auto& t : consideringTimeframes)
        {
            auto timeframe = timeframe::toTimeframe(t);

            // Skip 1-minute timeframe as it's the base
            if (timeframe == timeframe::Timeframe::MINUTE_1)
            {
                continue;
            }

            auto key = helper::generateCompositeKey(exchangeName, symbol, timeframe);

            // ex: 1440 / 60 + 1 (reserve one for forming candle)
            auto size = static_cast< size_t >((bucket_size / timeframe::convertTimeframeToOneMinutes(timeframe)) + 1);

            std::array< size_t, 2 > shape{size, 6};
            storage_.at(key) = std::make_unique< datastructure::DynamicBlazeArray< double > >(shape);
        }
    }
}

void ct::candle::CandlesState::reset()
{
    storage_.clear();
    are_all_initiated_ = false;
    initiated_pairs_.clear();

    // Stop the candle generation thread if running
    if (running_.load())
    {
        running_.store(false);
        if (candle_generation_thread_ && candle_generation_thread_->joinable())
        {
            candle_generation_thread_->join();
        }
    }
}

void ct::candle::CandlesState::addCandle(const enums::ExchangeName& exchange_name,
                                         const std::string& symbol,
                                         const timeframe::Timeframe& timeframe,
                                         const blaze::DynamicVector< double, blaze::rowVector >& candle,
                                         const bool with_execution,
                                         bool with_generation,
                                         const bool with_skip)
{
    auto& config                 = config::Config::getInstance();
    bool generateCandlesFrom1Min = config.getValue< bool >("env.data.generate_candles_from_1m");

    // overwrite with_generation based on the config value for live sessions
    if (helper::isLive() && !generateCandlesFrom1Min)
    {
        with_generation = false;
    }

    if (candle[_TIMESTAMP_] == 0)
    {
        if (helper::isDebugging())
        {
            std::ostringstream oss;
            oss << "Candle { " << ", open: " << candle[_OPEN_] << ", close: " << candle[_CLOSE_]
                << ", high: " << candle[_HIGH_] << ", low: " << candle[_LOW_] << ", volume: " << candle[_VOLUME_]
                << ", exchange_name: " << enums::toString(exchange_name) << ", symbol: " << symbol
                << ", timeframe: " << timeframe::toString(timeframe) << " }";
            logger::LOG.error("candle timestamp is zero. Full candle: " + oss.str());
        }

        // ISSUE: Throw?
        return;
    }

    if (helper::isLive())
    {
        std::string key = helper::generateCompositeKey(exchange_name, symbol);
        auto initiated  = initiated_pairs_.find(key) != initiated_pairs_.end();

        // Ignore if candle is still being initially imported
        if (with_skip && !initiated)
        {
            return;
        }

        // If it's not an old candle, update the related position's current_price
        if (getNextCandleTimestamp(candle, timeframe) > helper::nowToTimestamp())
        {
            updatePosition(exchange_name, symbol, candle[_CLOSE_]);
        }

        // Ignore new candle at the time of execution because it messes the count of candles without actually having an
        // impact
        if (candle[_TIMESTAMP_] > helper::nowToTimestamp())
        {
            return;
        }

        if (initiated)
        {
            // if it's not an initial candle, add it to the storage, if already exists, update it
            auto c = db::Candle(candle[_TIMESTAMP_],
                                candle[_OPEN_],
                                candle[_CLOSE_],
                                candle[_HIGH_],
                                candle[_LOW_],
                                candle[_VOLUME_],
                                exchange_name,
                                symbol,
                                timeframe);

            auto update_on_conflict = true;
            c.save(nullptr, update_on_conflict);
        }
    }

    std::string key = helper::generateCompositeKey(exchange_name, symbol, timeframe);

    const auto& candles = storage_.at(key);

    if (candles->size() == 0)
    {
        candles->append(candle);
    }

    const auto lastCandle = candles->row(-1);

    if (candle[_TIMESTAMP_] >= lastCandle[_TIMESTAMP_])
    {
        // in paper mode, check to see if the new candle causes any active orders to be executed
        if (with_execution && helper::isPaperTrading())
        {
            simulateOrderExecution(exchange_name, symbol, timeframe, candle);
        }

        if (candle[_TIMESTAMP_] > lastCandle[_TIMESTAMP_])
        {
            candles->append(candle);
        }
        else // ==
        {
            // ISSUE: Overwrite?
            candles->row(-1) = candle;
        }

        // Generate other timeframes
        if (with_generation && timeframe == timeframe::Timeframe::MINUTE_1)
        {
            generateHigherTimeframes(candle, exchange_name, symbol, with_execution);
        }
    }
    else
    {
        // Allow updating of the previous candle.
        // Loop through the last 20 items in arr to find it. If so, update it.
        // ISSUE: Is that -1 necessary?
        for (size_t i = 0; i < std::max(candles->size() - 1, size_t(20)); ++i)
        {
            const auto oldCandle = candles->row(-i);

            if (candle[_TIMESTAMP_] == oldCandle[_TIMESTAMP_])
            {
                candles->row(-i) = candle;
                break;
            }
        }
    }
}

void ct::candle::CandlesState::addCandles(const enums::ExchangeName& exchange_name,
                                          const std::string& symbol,
                                          const timeframe::Timeframe& timeframe,
                                          const blaze::DynamicMatrix< double >& candles,
                                          const bool with_generation)
{
    for (size_t i = 0UL; i < candles.rows(); ++i)
    {
        auto withExecution = false;
        auto withSkip      = false;

        addCandle(exchange_name, symbol, timeframe, blaze::row(candles, i), withExecution, with_generation, withSkip);
    }
}

void ct::candle::CandlesState::addCandleFromTrade(const double price,
                                                  const double volume,
                                                  const enums::ExchangeName& exchange_name,
                                                  const std::string& symbol)
{
    if (!helper::isLive())
    {
        throw std::runtime_error("addCandleFromTrade is for live modes only");
    }

    auto key       = helper::generateCompositeKey(exchange_name, symbol);
    auto initiated = initiated_pairs_.find(key) != initiated_pairs_.end();

    // Ignore if candle is still being initially imported
    if (!initiated)
    {
        return;
    }

    // update position's current price
    updatePosition(exchange_name, symbol, price);

    auto func = [&](const timeframe::Timeframe& timeframe)
    {
        // In some cases we might be missing the current forming candle like it is on FTX, hence if that is the case,
        // generate the current forming candle (it won't be super accurate)
        auto currentCandle = getCurrentCandle(exchange_name, symbol, timeframe);

        if (getNextCandleTimestamp(currentCandle, timeframe) < helper::nowToTimestamp())
        {
            auto nxt_candle = generateEmptyCandleFromPreviousCandle(currentCandle, timeframe);
            addCandle(exchange_name, symbol, timeframe, nxt_candle);
        }

        currentCandle = getCurrentCandle(exchange_name, symbol, timeframe);

        currentCandle[_CLOSE_] = price;
        currentCandle[_HIGH_]  = std::max(currentCandle[_HIGH_], price);
        currentCandle[_LOW_]   = std::min(currentCandle[_LOW_], price);
        currentCandle[_VOLUME_] += volume;

        addCandle(exchange_name, symbol, timeframe, currentCandle);
    };

    auto& config                 = config::Config::getInstance();
    bool generateCandlesFrom1Min = config.getValue< bool >("env.data.generate_candles_from_1m");

    // to support both candle generation and ...
    if (generateCandlesFrom1Min)
    {
        func(timeframe::Timeframe::MINUTE_1);
    }
    else
    {
        auto routes = route::Router::getInstance().formattedRoutes();

        for (const auto& route : routes)
        {
            auto timeframe = route["timeframe"].get< timeframe::Timeframe >();
            func(timeframe);
        }
    }
}

void ct::candle::CandlesState::updatePosition(const enums::ExchangeName& exchange_name,
                                              const std::string& symbol,
                                              const double price)
{
    auto position = position::PositionsState::getInstance().getPosition(exchange_name, symbol);

    if (!position)
    {
        return;
    }

    if (helper::isLive())
    {
        auto exchange  = exchange::ExchangesState::getInstance().getExchange(exchange_name);
        auto vars      = exchange->getVars();
        auto precision = vars["precisions"][symbol]["price_precision"].get< int >();

        position->setCurrentPrice(helper::round(price, precision));
    }
    else
    {
        position->setCurrentPrice(price);
    }
}

void ct::candle::CandlesState::generateHigherTimeframes(const blaze::DynamicVector< double, blaze::rowVector >& candle,
                                                        const enums::ExchangeName& exchange_name,
                                                        const std::string& symbol,
                                                        const bool with_execution)
{
    if (!helper::isLive())
    {
        return;
    }

    auto& config               = config::Config::getInstance();
    auto consideringTimeframes = config.getValue< std::vector< std::string > >("app_considering_timeframes");

    for (const auto& t : consideringTimeframes)
    {
        auto timeframe = timeframe::toTimeframe(t);

        // Skip 1-minute timeframe as it's the base
        if (timeframe == timeframe::Timeframe::MINUTE_1)
        {
            continue;
        }

        auto currentCandle = getCurrentCandle(exchange_name, symbol, timeframe);
        auto minutes       = static_cast< int >((candle[_TIMESTAMP_] - currentCandle[_TIMESTAMP_]) / 60'000);

        if (minutes < 0)
        {
            // it's receiving an slightly older candle than the last one. Ignore it
            // TODO:Throw.
            return;
        }

        auto temp = getCandles(exchange_name, symbol, timeframe::Timeframe::MINUTE_1);

        blaze::DynamicMatrix< double > shortCandles(
            blaze::submatrix(temp, temp.rows() - minutes, 0, minutes, temp.columns())); // ISSUE: Is index write?

        if (shortCandles.rows() == 0)
        {
            std::ostringstream oss;
            oss << "No candles were passed. More info: exchange: " << enums::toString(exchange_name)
                << " symbol: " << symbol << " timeframe: " << timeframe::toString(timeframe) << " minutes: " << minutes
                << " last candle's timestamp: " << currentCandle[_TIMESTAMP_]
                << " current timestamp: " << helper::nowToTimestamp();
            throw std::runtime_error(oss.str());
        }

        bool acceptFormingCandles = true;
        auto generatedCandle      = generateCandleFromOneMinutes(timeframe, shortCandles, acceptFormingCandles);

        auto withGeneration = true;
        addCandle(exchange_name, symbol, timeframe, generatedCandle, with_execution, withGeneration);
    }
}

void ct::candle::CandlesState::simulateOrderExecution(
    const enums::ExchangeName& exchange_name,
    const std::string& symbol,
    const timeframe::Timeframe& timeframe,
    const blaze::DynamicVector< double, blaze::rowVector >& new_candle)
{
    auto previousCandle = getCurrentCandle(exchange_name, symbol, timeframe);

    if (previousCandle[_CLOSE_] == new_candle[_CLOSE_])
    {
        return;
    }

    auto orders = order::OrdersState::getInstance().getOrders(exchange_name, symbol);

    for (const auto& order : orders)
    {
        if (!order.isActive())
        {
            continue;
        }
        if (((order.getPrice() >= previousCandle[_CLOSE_]) && (order.getPrice() <= new_candle[_CLOSE_])) ||
            ((order.getPrice() <= previousCandle[_CLOSE_]) && (order.getPrice() >= new_candle[_CLOSE_])))
        {
            // TODO:
            // order.execute();
        }
    }
}

int ct::candle::CandlesState::formingEstimation(const enums::ExchangeName& exchange_name,
                                                const std::string& symbol,
                                                const timeframe::Timeframe& timeframe) const
{
    auto longKey  = helper::generateCompositeKey(exchange_name, symbol, timeframe);
    auto shortKey = helper::generateCompositeKey(exchange_name, symbol, timeframe::Timeframe::MINUTE_1);
    auto required1MinToCompleteCount = timeframe::convertTimeframeToOneMinutes(timeframe);
    auto current1MinCount            = storage_.at(shortKey)->size();
    auto diff                        = current1MinCount % required1MinToCompleteCount;

    return diff;
}

blaze::DynamicMatrix< double > ct::candle::CandlesState::getCandles(const enums::ExchangeName& exchange_name,
                                                                    const std::string& symbol,
                                                                    const timeframe::Timeframe& timeframe) const
{
    auto shortKey = helper::generateCompositeKey(exchange_name, symbol, timeframe::Timeframe::MINUTE_1);
    auto longKey  = helper::generateCompositeKey(exchange_name, symbol, timeframe);

    const auto& shortCandles = storage_.at(shortKey);
    const auto& longCandles  = storage_.at(longKey);

    // no need to worry for forming candles when timeframe == 1m
    if (timeframe == timeframe::Timeframe::MINUTE_1)
    {
        if (shortCandles->size() == 0)
        {
            return blaze::DynamicMatrix< double >(0, _COLUMNS_);
        }
        else
        {
            const auto n = shortCandles->size();
            return shortCandles->rows(0, n);
        }
    }

    auto diff = formingEstimation(exchange_name, symbol, timeframe);

    auto longCount  = shortCandles->size();
    auto shortCount = longCandles->size();

    if (diff == 0 && longCount == 0)
    {
        return blaze::DynamicMatrix< double >(0, _COLUMNS_);
    }

    if (diff == 0 || longCandles->row(longCount - 1)[_TIMESTAMP_] == shortCandles->row(shortCount - diff)[_TIMESTAMP_])
    {
        return longCandles->rows(0, longCount);
    }
    else if (!helper::isLive())
    {
        // generate forming candle only if NOT in live mode
        blaze::DynamicMatrix< double > temp(longCandles->rows(0, longCount));
        blaze::DynamicMatrix< double > cop(shortCandles->rows(shortCount - diff, shortCount));

        auto generatedCandle = generateCandleFromOneMinutes(timeframe, cop, true);

        // append
        temp.resize(temp.rows() + 1, temp.columns());
        blaze::row(temp, temp.rows() - 1) = generatedCandle;

        return temp;
    }
    else
    {
        return longCandles->rows(0, longCount);
    }
}

blaze::DynamicVector< double, blaze::rowVector > ct::candle::CandlesState::getCurrentCandle(
    const enums::ExchangeName& exchange_name, const std::string& symbol, const timeframe::Timeframe& timeframe) const
{
    auto shortKey = helper::generateCompositeKey(exchange_name, symbol, timeframe::Timeframe::MINUTE_1);
    auto longKey  = helper::generateCompositeKey(exchange_name, symbol, timeframe);

    const auto& shortCandles = storage_.at(shortKey);
    const auto& longCandles  = storage_.at(longKey);

    // no need to worry for forming candles when timeframe == 1m
    if (timeframe == timeframe::Timeframe::MINUTE_1)
    {
        if (shortCandles->size() == 0)
        {
            return blaze::DynamicVector< double, blaze::rowVector >(0, _COLUMNS_);
        }
        else
        {
            return shortCandles->row(-1);
        }
    }

    auto diff = formingEstimation(exchange_name, symbol, timeframe);

    auto longCount  = shortCandles->size();
    auto shortCount = longCandles->size();

    if (diff != 0)
    {
        blaze::DynamicMatrix< double > c(shortCandles->rows(shortCount - diff, shortCount));

        return generateCandleFromOneMinutes(timeframe, c, true);
    }
    if (longCount == 0)
    {
        return blaze::DynamicVector< double, blaze::rowVector >(0, _COLUMNS_);
    }
    else
    {
        return longCandles->row(-1);
    }
}

void ct::candle::CandlesState::addMultiple1MinCandles(const blaze::DynamicMatrix< double >& candles,
                                                      const enums::ExchangeName& exchange_name,
                                                      const std::string& symbol)
{
    if (!(helper::isBacktesting() || helper::isOptimizing()))
    {
        throw std::runtime_error("addMultiple1MinCandles() is for backtesting or optimizing only");
    }

    auto key = helper::generateCompositeKey(exchange_name, symbol, timeframe::Timeframe::MINUTE_1);

    auto& shortCandles = storage_.at(key);

    const auto n = candles.rows();

    if (shortCandles->size() == 0)
    {
        shortCandles->appendMultiple(candles);
    }
    else if (candles(0, _TIMESTAMP_) > shortCandles->row(-1)[_TIMESTAMP_])
    {
        shortCandles->appendMultiple(candles);
    }
    else if (candles(0, _TIMESTAMP_) >= shortCandles->row(-n)[_TIMESTAMP_] &&
             candles(n - 1, _TIMESTAMP_) >= shortCandles->row(-1)[_TIMESTAMP_])
    {
        auto overrideCandles =
            static_cast< int >(n - ((candles(n - 1, _TIMESTAMP_) - shortCandles->row(-1)[_TIMESTAMP_]) / 60'000));

        shortCandles->rows(-overrideCandles, shortCandles->size()) = candles;
    }
    else
    {
        std::ostringstream oss;
        oss << "Could not find the candle with timestamp " << helper::timestampToTime(candles(0, _TIMESTAMP_))
            << " in the storage. Last candle's timestamp: "
            << helper::timestampToTime(shortCandles->row(-1)[_TIMESTAMP_])
            << ". exchange: " << enums::toString(exchange_name) << " symbol: " << symbol;
        throw std::runtime_error(oss.str());
    }
}

bool ct::candle::CandlesState::areAllInitiated() const
{
    return are_all_initiated_;
}

void ct::candle::CandlesState::generateNewCandlesLoop()
{
    // Stop existing thread if running
    if (running_.load())
    {
        running_.store(false);
        if (candle_generation_thread_ && candle_generation_thread_->joinable())
        {
            candle_generation_thread_->join();
        }
    }

    // Start new thread
    running_.store(true);
    candle_generation_thread_ = std::make_unique< std::thread >(
        [this]()
        {
            while (running_.load())
            {
                // Make sure all candles are already initiated
                if (!are_all_initiated_)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                // Only at first second on each minute
                int64_t now = helper::nowToTimestamp();
                if (now % 60'000 != 1000) // ISSUE:
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                // Get all routes
                auto routes = route::Router::getInstance().formattedRoutes();

                for (const auto& route : routes)
                {
                    // Get current candle
                    auto exchangeName  = route["exchange_name"].get< enums::ExchangeName >();
                    auto symbol        = route["symbol"].get< std::string >();
                    auto timeframe     = route["timeframe"].get< timeframe::Timeframe >();
                    auto currentCandle = getCurrentCandle(exchangeName, symbol, timeframe);

                    // Fix for a bug
                    if (currentCandle[_TIMESTAMP_] <= 60'000)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    if (getNextCandleTimestamp(currentCandle, timeframe) <= helper::nowToTimestamp())
                    {
                        // Generate empty candle from previous candle
                        auto nxtCandle = generateEmptyCandleFromPreviousCandle(currentCandle, timeframe);

                        // Add new candle
                        addCandle(exchangeName, symbol, timeframe, nxtCandle);
                    }
                }

                // Sleep for 1 second
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
}

blaze::DynamicVector< double, blaze::rowVector > ct::candle::CandlesState::generateEmptyCandleFromPreviousCandle(
    const blaze::DynamicVector< double, blaze::rowVector >& previous_candle,
    const timeframe::Timeframe& timeframe) const
{
    auto newCandle = previous_candle;

    auto offset = timeframe::convertTimeframeToOneMinutes(timeframe) * 60'000;

    // Set timestamp
    newCandle[_TIMESTAMP_] = previous_candle[_TIMESTAMP_] + offset;

    // Set OHLC values (all equal to previous close)
    newCandle[_OPEN_]  = previous_candle[_CLOSE_]; // open = previous close
    newCandle[_CLOSE_] = previous_candle[_CLOSE_]; // close = previous close
    newCandle[_HIGH_]  = previous_candle[_CLOSE_]; // high = previous close
    newCandle[_LOW_]   = previous_candle[_CLOSE_]; // low = previous close

    // Set volume to 0
    newCandle[_VOLUME_] = 0.0;

    return newCandle;
}

void ct::candle::CandlesState::markAllAsInitiated()
{
    for (auto& pair : initiated_pairs_)
    {
        pair.second = true;
    }

    are_all_initiated_ = true;
}

template blaze::DynamicVector< double, blaze::rowVector > ct::candle::generateFakeCandle(
    const blaze::DynamicVector< double, blaze::rowVector >&, bool);

template blaze::DynamicVector< float, blaze::rowVector > ct::candle::generateFakeCandle(
    const blaze::DynamicVector< float, blaze::rowVector >&, bool);

template blaze::DynamicMatrix< double > ct::candle::generateCandlesFromClosePrices(const std::vector< double >&, bool);

template blaze::DynamicMatrix< float > ct::candle::generateCandlesFromClosePrices(const std::vector< float >&, bool);

template blaze::DynamicMatrix< double > ct::candle::generateRangeCandles(size_t, bool);

template blaze::DynamicMatrix< float > ct::candle::generateRangeCandles(size_t, bool);

template int64_t ct::candle::getNextCandleTimestamp(const blaze::DynamicVector< int64_t, blaze::rowVector >& candle,
                                                    const timeframe::Timeframe& timeframe);

template int64_t ct::candle::getNextCandleTimestamp(const blaze::DynamicVector< double, blaze::rowVector >& candle,
                                                    const timeframe::Timeframe& timeframe);

template auto ct::candle::getCandleSource(const blaze::DynamicMatrix< int64_t >& candles, Source source_type)
    -> blaze::DynamicVector< int64_t, blaze::rowVector >;

template auto ct::candle::getCandleSource(const blaze::DynamicMatrix< double >& candles, Source source_type)
    -> blaze::DynamicVector< double, blaze::rowVector >;
