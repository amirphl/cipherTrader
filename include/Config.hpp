#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <nlohmann/json.hpp>

namespace Config {

// Define the variant type for all possible ConfigData values
using ConfValue =
    std::variant<int, bool, double, std::string,
                 std::map<std::string, std::string>, // For live_drivers
                 std::vector<std::string>            // For app arrays
                 >;

class AnyMap {
public:
  // Store a value of any type
  template <typename T> void store(const std::string &key, T &&value) {
    map_[key] = std::forward<T>(value); // Perfect forwarding to avoid copies
  }

  // Retrieve and cast a value to type T
  template <typename T>
  T get(const std::string &key, const T &defaultValue = T()) const {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return defaultValue;
    }
    try {
      return std::any_cast<T>(it->second);
    } catch (const std::bad_any_cast &) {
      return defaultValue; // Return default on type mismatch
    }
  }

  // Check if a key exists
  bool has(const std::string &key) const {
    return map_.find(key) != map_.end();
  }

  // Clear the map
  void clear() { map_.clear(); }

private:
  std::map<std::string, std::any> map_;
};

struct Conf {
  struct Env {
    struct Caching {
      std::string driver = "yaml";
    } caching;

    struct Logging {
      bool order_submission = true;
      bool order_cancellation = true;
      bool order_execution = true;
      bool position_opened = true;
      bool position_increased = true;
      bool position_reduced = true;
      bool position_closed = true;
      bool shorter_period_candles = false;
      bool trading_candles = true;
      bool balance_update = true;
      bool exchange_ws_reconnection = true;
    } logging;

    struct Exchange {
      int fee = 0;
      std::string type = "futures";
      std::string futures_leverage_mode = "cross"; // "cross" or "isolated"
      int futures_leverage = 1;
      double balance = 10000.0;
    };
    std::map<std::string, Exchange> exchanges = {{"SANDBOX", Exchange{}}};

    struct Optimization {
      std::string ratio = "sharpe"; // sharpe, calmar, sortino, etc.
    } optimization;

    struct Data {
      int warmup_candles_num = 240;
      bool generate_candles_from_1m = false;
      bool persistency = true;
    } data;
  } env;

  struct App {
    std::vector<std::string> considering_symbols;
    std::vector<std::string> trading_symbols;
    std::vector<std::string> considering_timeframes;
    std::vector<std::string> trading_timeframes;
    std::vector<std::string> considering_exchanges;
    std::vector<std::string> trading_exchanges;
    std::vector<std::string>
        considering_candles; // Assuming string for simplicity
    std::map<std::string, std::string>
        live_drivers;         // Placeholder for live trade drivers
    std::string trading_mode; // "backtest", "livetrade", "fitness"
    bool debug_mode = false;
    bool is_unit_testing = false;
  } app;
};

// Singleton Config class
class Config {
public:
  // Singleton access
  static Config &getInstance();

  // Initialize the config (loads defaults or from environment)
  void initConfig();

  // Reload the config (e.g., from environment or file)
  void reloadConfig(bool clearCache = true);

  // Get a config value by dot-separated key string
  // Get config value as a variant
  ConfValue get(const std::string &keys,
                const ConfValue &defaultValue = std::string("")) const;

  bool isCached(const std::string &keys) const;

  // Deleted to enforce Singleton
  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

private:
  Config() = default; // Private constructor for Singleton
  Conf conf_;
  // AnyMap cache_; // Cache for computed values
  mutable std::map<std::string, ConfValue> cache_; // Cache stores variant

  ConfValue fetchValue(const std::string &keys,
                       const ConfValue &defaultValue) const;
  ConfValue getNestedValue(const std::string &keys) const;

  // Helper to convert environment string to ConfValue
  ConfValue fromEnvString(const std::string &value) const;
};

} // namespace Config

#endif
