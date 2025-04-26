#include "Route.hpp"
#include "Enum.hpp"
#include <nlohmann/json.hpp>

ct::route::Route::Route(const enums::Exchange exchange,
                        const std::string &symbol,
                        std::optional< enums::Timeframe > timeframe,
                        std::optional< std::string > strategy_name,
                        std::optional< std::string > dna)
    : exchange(exchange)
    , symbol(symbol)
    , timeframe(timeframe)
    , strategy_name(strategy_name)
    , strategy(std::nullopt)
    , dna(dna)
{
}

void ct::route::Router::reset()
{
    routes_.clear();
    data_candles_.clear();
    market_data_.clear();
}

std::vector< nlohmann::json > ct::route::Router::formattedRoutes() const
{
    std::vector< json > result;
    for (const auto &r : routes_)
    {
        result.push_back({{"exchange", r.exchange},
                          {"symbol", r.symbol},
                          {"timeframe", r.timeframe.value()},
                          {"strategy", r.strategy.value()}});
    }
    return result;
}

std::vector< nlohmann::json > ct::route::Router::formattedDataRoutes() const
{
    std::vector< json > result;
    for (const auto &r : data_candles_)
    {
        result.push_back({{"exchange", r["exchange"]}, {"symbol", r["symbol"]}, {"timeframe", r["timeframe"]}});
    }
    return result;
}

std::vector< nlohmann::json > ct::route::Router::allFormattedRoutes() const
{
    std::vector< json > result      = formattedRoutes();
    std::vector< json > data_routes = formattedDataRoutes();
    result.insert(result.end(), data_routes.begin(), data_routes.end());
    return result;
}

void ct::route::Router::init(const std::vector< json > &routes, const std::vector< json > &data_routes)
{
    setRoutes(routes);
    setDataCandles(data_routes);
    // TODO: Simulate store reset logic
    // store.reset(force_install_routes = is_unit_testing());
}

void ct::route::Router::setRoutes(const std::vector< json > &routes)
{
    reset();

    for (const auto &r : routes)
    {
        auto exchange = enums::toExchange(r["exchange"].get< std::string >());
        auto symbol   = r["symbol"].get< std::string >();
        auto timeframe =
            r.contains("timeframe")
                ? std::optional< enums::Timeframe >{enums::toTimeframe(r["timeframe"].get< std::string >())}
                : std::nullopt;
        auto strategy_name = r.contains("strategy_name")
                                 ? std::optional< std::string >{r["strategy_name"].get< std::string >()}
                                 : std::nullopt;
        auto dna = r.contains("dna") ? std::optional< std::string >{r["dna"].get< std::string >()} : std::nullopt;

        this->routes_.emplace_back(exchange, symbol, timeframe, strategy_name, dna);

        // TODO: Implement strategy existence check
        // std::string strategy_name = r["strategy"];
        // bool exists = std::filesystem::exists("strategies/" + strategy_name +
        // "/__init__.py");

        // if (!exists) {
        //   throw std::runtime_error("A strategy with the name of \"" +
        //   strategy_name + "\" could not be found.");
        // }
    }
}

void ct::route::Router::setMarketData(const std::vector< json > &routes)
{
    market_data_.clear();
    for (const auto &r : routes)
    {
        market_data_.emplace_back(r["exchange"], r["symbol"], r["timeframe"], r["strategy_name"], r["dna"]);
    }
}

void ct::route::Router::setDataCandles(const std::vector< json > &data_candles)
{
    this->data_candles_ = data_candles;
}

ct::route::Route ct::route::Router::getRoute(size_t index) const
{
    if (index >= routes_.size())
    {
        throw std::invalid_argument("Index out of range");
    }

    return routes_[index];
}
