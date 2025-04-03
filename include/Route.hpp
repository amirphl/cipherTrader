#ifndef CIPHER_ROUTE_HPP
#define CIPHER_ROUTE_HPP

#include <optional>
#include <string>
#include <vector>
#include "Enum.hpp"
#include <nlohmann/json.hpp>

namespace CipherRoute
{

using json = nlohmann::json;

class Route
{
   public:
    CipherEnum::Exchange exchange;
    std::string symbol;
    std::optional< CipherEnum::Timeframe > timeframe;
    std::optional< std::string > strategy_name;
    std::optional< std::string > strategy;
    std::optional< std::string > dna;

    Route()
        : exchange()
        , symbol()
        , timeframe(std::nullopt)
        , strategy_name(std::nullopt)
        , strategy(std::nullopt)
        , dna(std::nullopt)
    {
    }

    Route(CipherEnum::Exchange exchange,
          const std::string &symbol,
          std::optional< CipherEnum::Timeframe > timeframe = std::nullopt,
          std::optional< std::string > strategy_name       = std::nullopt,
          std::optional< std::string > dna                 = std::nullopt);
};

class Router
{
   private:
    std::vector< Route > routes_;
    std::vector< json > data_candles_;
    std::vector< Route > market_data_;

    // Private constructor to enforce singleton pattern
    Router() = default;

    // Delete copy constructor & assignment operator to prevent copies
    Router(const Router &)            = delete;
    Router &operator=(const Router &) = delete;

   public:
    static Router &getInstance()
    {
        static Router instance;
        return instance;
    }

    std::vector< json > formattedRoutes() const;
    std::vector< json > formattedDataRoutes() const;
    std::vector< json > allFormattedRoutes() const;

    void init(const std::vector< json > &routes, const std::vector< json > &data_routes = {});
    void setRoutes(const std::vector< json > &routes);
    void setMarketData(const std::vector< json > &routes);
    void setDataCandles(const std::vector< json > &data_candles);

    Route getRoute(size_t index) const;

    void reset();
};

} // namespace CipherRoute

#endif // CIPHER_ROUTE_HPP
