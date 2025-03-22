#ifndef ROUTE_HPP
#define ROUTE_HPP

#include "Enum.hpp"
#include <nlohmann/json.hpp>

namespace Route {

using json = nlohmann::json;

class Route {
public:
  Enum::Exchange exchange;
  std::string symbol;
  std::optional<Enum::Timeframe> timeframe;
  std::optional<std::string> strategy_name;
  std::optional<std::string> strategy;
  std::optional<std::string> dna;

  Route()
      : exchange(), symbol(), timeframe(std::nullopt),
        strategy_name(std::nullopt), strategy(std::nullopt), dna(std::nullopt) {
  }

  Route(Enum::Exchange exchange, const std::string &symbol,
        std::optional<Enum::Timeframe> timeframe = std::nullopt,
        std::optional<std::string> strategy_name = std::nullopt,
        std::optional<std::string> dna = std::nullopt);
};

class Router {
private:
  std::vector<Route> routes;
  std::vector<json> data_candles;
  std::vector<Route> market_data;

  // Private constructor to enforce singleton pattern
  Router() = default;

  // Delete copy constructor & assignment operator to prevent copies
  Router(const Router &) = delete;
  Router &operator=(const Router &) = delete;

public:
  static Router &getInstance() {
    static Router instance;
    return instance;
  }

  std::vector<json> formattedRoutes() const;
  std::vector<json> formattedDataRoutes() const;
  std::vector<json> allFormattedRoutes() const;

  void initiate(const std::vector<json> &routes,
                const std::vector<json> &data_routes = {});
  void setRoutes(const std::vector<json> &routes);
  void setMarketData(const std::vector<json> &routes);
  void setDataCandles(const std::vector<json> &data_candles);

  Route getRoute(size_t index) const;

  void reset();
};

} // namespace Route

#endif // ROUTE_HPP
