#include "Route.hpp"
#include "Enum.hpp"

namespace Route {

Route::Route(const Enum::Exchange exchange, const std::string &symbol,
             std::optional<Enum::Timeframe> timeframe,
             std::optional<std::string> strategy_name,
             std::optional<std::string> dna)
    : exchange(exchange), symbol(symbol), timeframe(timeframe),
      strategy_name(strategy_name), strategy(std::nullopt), dna(dna) {}

void Router::reset() {
  routes.clear();
  data_candles.clear();
  market_data.clear();
}

std::vector<json> Router::formattedRoutes() const {
  std::vector<json> result;
  for (const auto &r : routes) {
    result.push_back({{"exchange", r.exchange},
                      {"symbol", r.symbol},
                      {"timeframe", r.timeframe.value()},
                      {"strategy", r.strategy.value()}});
  }
  return result;
}

std::vector<json> Router::formattedDataRoutes() const {
  std::vector<json> result;
  for (const auto &r : data_candles) {
    result.push_back({{"exchange", r["exchange"]},
                      {"symbol", r["symbol"]},
                      {"timeframe", r["timeframe"]}});
  }
  return result;
}

std::vector<json> Router::allFormattedRoutes() const {
  std::vector<json> result = formattedRoutes();
  std::vector<json> data_routes = formattedDataRoutes();
  result.insert(result.end(), data_routes.begin(), data_routes.end());
  return result;
}

void Router::initiate(const std::vector<json> &routes,
                      const std::vector<json> &data_routes) {
  setRoutes(routes);
  setDataCandles(data_routes);
  // TODO: Simulate store reset logic
  // store.reset(force_install_routes = is_unit_testing());
}

void Router::setRoutes(const std::vector<json> &routes) {
  reset();
  for (const auto &r : routes) {
    // TODO: Implement strategy existence check
    // std::string strategy_name = r["strategy"];
    // bool exists = std::filesystem::exists("strategies/" + strategy_name +
    // "/__init__.py");

    // if (!exists) {
    //   throw std::runtime_error("A strategy with the name of \"" +
    //   strategy_name + "\" could not be found.");
    // }

    // this->routes.push_back({{"exchange", r["exchange"]},
    //                         {"symbol", r["symbol"]},
    //                         {"timeframe", r["timeframe"]},
    //                         {"strategy", r["strategy"]}});
  }
}

void Router::setMarketData(const std::vector<json> &routes) {
  market_data.clear();
  for (const auto &r : routes) {
    market_data.emplace_back(r["exchange"], r["symbol"], r["timeframe"],
                             r["strategy_name"], r["dna"]);
  }
}

void Router::setDataCandles(const std::vector<json> &data_candles) {
  this->data_candles = data_candles;
}

std::pair<Route, bool> Router::getRoute(size_t index) const {
  if (index >= routes.size()) {
    return {{}, false};
  }
  return {routes[index], true};
}

} // namespace Route
