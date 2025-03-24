#include "Config.hpp"
#include <sstream>

namespace Config {
// Compile-time string hashing (simple FNV-1a variant for this example)
constexpr uint32_t hashString(const char *str, uint32_t hash = 2166136261u) {
  return *str ? hashString(str + 1, (hash ^ *str) * 16777619u) : hash;
}

// Enum for Level 1 keys
enum class Level1Key : uint32_t {
  Env = hashString("env"),
  App = hashString("app"),
  Invalid = 0
};

// Enum for Level 2 keys under "env"
enum class EnvLevel2Key : uint32_t {
  Caching = hashString("caching"),
  Logging = hashString("logging"),
  Exchanges = hashString("exchanges"),
  Optimization = hashString("optimization"),
  Data = hashString("data"),
  Invalid = 0
};

// Enum for Level 2 keys under "app"
enum class AppLevel2Key : uint32_t {
  ConsideringSymbols = hashString("considering_symbols"),
  TradingSymbols = hashString("trading_symbols"),
  ConsideringTimeframes = hashString("considering_timeframes"),
  TradingTimeframes = hashString("trading_timeframes"),
  ConsideringExchanges = hashString("considering_exchanges"),
  TradingExchanges = hashString("trading_exchanges"),
  ConsideringCandles = hashString("considering_candles"),
  LiveDrivers = hashString("live_drivers"),
  TradingMode = hashString("trading_mode"),
  DebugMode = hashString("debug_mode"),
  IsUnitTesting = hashString("is_unit_testing"),
  Invalid = 0
};

// Helper to convert string to enum at runtime
constexpr Level1Key toLevel1Key(const std::string &s) {
  return hashString(s.c_str()) == static_cast<uint32_t>(Level1Key::Env)
             ? Level1Key::Env
         : hashString(s.c_str()) == static_cast<uint32_t>(Level1Key::App)
             ? Level1Key::App
             : Level1Key::Invalid;
}

constexpr EnvLevel2Key toEnvLevel2Key(const std::string &s) {
  uint32_t h = hashString(s.c_str());
  return h == static_cast<uint32_t>(EnvLevel2Key::Caching)
             ? EnvLevel2Key::Caching
         : h == static_cast<uint32_t>(EnvLevel2Key::Logging)
             ? EnvLevel2Key::Logging
         : h == static_cast<uint32_t>(EnvLevel2Key::Exchanges)
             ? EnvLevel2Key::Exchanges
         : h == static_cast<uint32_t>(EnvLevel2Key::Optimization)
             ? EnvLevel2Key::Optimization
         : h == static_cast<uint32_t>(EnvLevel2Key::Data)
             ? EnvLevel2Key::Data
             : EnvLevel2Key::Invalid;
}

constexpr AppLevel2Key toAppLevel2Key(const std::string &s) {
  uint32_t h = hashString(s.c_str());
  return h == static_cast<uint32_t>(AppLevel2Key::ConsideringSymbols)
             ? AppLevel2Key::ConsideringSymbols
         : h == static_cast<uint32_t>(AppLevel2Key::TradingSymbols)
             ? AppLevel2Key::TradingSymbols
         : h == static_cast<uint32_t>(AppLevel2Key::ConsideringTimeframes)
             ? AppLevel2Key::ConsideringTimeframes
         : h == static_cast<uint32_t>(AppLevel2Key::TradingTimeframes)
             ? AppLevel2Key::TradingTimeframes
         : h == static_cast<uint32_t>(AppLevel2Key::ConsideringExchanges)
             ? AppLevel2Key::ConsideringExchanges
         : h == static_cast<uint32_t>(AppLevel2Key::TradingExchanges)
             ? AppLevel2Key::TradingExchanges
         : h == static_cast<uint32_t>(AppLevel2Key::ConsideringCandles)
             ? AppLevel2Key::ConsideringCandles
         : h == static_cast<uint32_t>(AppLevel2Key::LiveDrivers)
             ? AppLevel2Key::LiveDrivers
         : h == static_cast<uint32_t>(AppLevel2Key::TradingMode)
             ? AppLevel2Key::TradingMode
         : h == static_cast<uint32_t>(AppLevel2Key::DebugMode)
             ? AppLevel2Key::DebugMode
         : h == static_cast<uint32_t>(AppLevel2Key::IsUnitTesting)
             ? AppLevel2Key::IsUnitTesting
             : AppLevel2Key::Invalid;
}

Config &Config::getInstance() {
  static Config instance;
  return instance;
}

void Config::initConfig() {
  // Initialize with defaults (already set in Conf struct)
  // Optionally override with environment variables
  reloadConfig();
  // TODO
  // TODO: FIll from Info exchanges.
}

void Config::reloadConfig(bool clearCache) {
  if (clearCache) {
    cache_.clear();
  }
  // Environment variable overrides can be added here if needed
  // TODO
}

ConfValue Config::get(const std::string &keys,
                      const ConfValue &defaultValue) const {
  if (keys.empty()) {
    throw std::invalid_argument("Keys string cannot be empty");
  }

  ConfValue value;
  if (cache_.find(keys) == cache_.end()) {
    value = fetchValue(keys, defaultValue);
    cache_.insert_or_assign(keys, value); // Replace operator[]
  } else {
    value = cache_.at(keys);
  }

  return value;
}

bool Config::isCached(const std::string &keys) const {
  return cache_.find(keys) != cache_.end();
}

ConfValue Config::getNestedValue(const std::string &keys) const {
  if (keys.empty()) {
    return std::string("");
  }

  // Split keys into tokens
  std::vector<std::string> tokens;
  std::istringstream iss(keys);
  std::string token;
  while (std::getline(iss, token, '.')) {
    tokens.push_back(token);
  }

  if (tokens.empty()) {
    return std::string("");
  }

  // Level 1 switch
  switch (toLevel1Key(tokens[0])) {
  case Level1Key::Env:
    if (tokens.size() == 1)
      return std::string("");
    switch (toEnvLevel2Key(tokens[1])) {
    case EnvLevel2Key::Caching:
      if (tokens.size() == 2)
        return std::string("");
      if (tokens[2] == "driver" && tokens.size() == 3)
        return conf_.env.caching.driver;
      break;
    case EnvLevel2Key::Logging:
      if (tokens.size() == 2)
        return std::string("");
      switch (hashString(tokens[2].c_str())) {
      case hashString("order_submission"):
        if (tokens.size() == 3)
          return conf_.env.logging.order_submission;
        break;
      case hashString("order_cancellation"):
        if (tokens.size() == 3)
          return conf_.env.logging.order_cancellation;
        break;
      case hashString("order_execution"):
        if (tokens.size() == 3)
          return conf_.env.logging.order_execution;
        break;
      case hashString("position_opened"):
        if (tokens.size() == 3)
          return conf_.env.logging.position_opened;
        break;
      case hashString("position_increased"):
        if (tokens.size() == 3)
          return conf_.env.logging.position_increased;
        break;
      case hashString("position_reduced"):
        if (tokens.size() == 3)
          return conf_.env.logging.position_reduced;
        break;
      case hashString("position_closed"):
        if (tokens.size() == 3)
          return conf_.env.logging.position_closed;
        break;
      case hashString("shorter_period_candles"):
        if (tokens.size() == 3)
          return conf_.env.logging.shorter_period_candles;
        break;
      case hashString("trading_candles"):
        if (tokens.size() == 3)
          return conf_.env.logging.trading_candles;
        break;
      case hashString("balance_update"):
        if (tokens.size() == 3)
          return conf_.env.logging.balance_update;
        break;
      case hashString("exchange_ws_reconnection"):
        if (tokens.size() == 3)
          return conf_.env.logging.exchange_ws_reconnection;
        break;
      }
      break;
    case EnvLevel2Key::Exchanges:
      if (tokens.size() <= 2)
        return std::string("");
      for (const auto &[key, exch] : conf_.env.exchanges) {
        if (key == tokens[2] && tokens.size() == 4) {
          switch (hashString(tokens[3].c_str())) {
          case hashString("fee"):
            return exch.fee;
          case hashString("type"):
            return exch.type;
          case hashString("futures_leverage_mode"):
            return exch.futures_leverage_mode;
          case hashString("futures_leverage"):
            return exch.futures_leverage;
          case hashString("balance"):
            return exch.balance;
          }
        }
      }
      break;
    case EnvLevel2Key::Optimization:
      if (tokens.size() == 2)
        return std::string("");
      if (tokens[2] == "ratio" && tokens.size() == 3)
        return conf_.env.optimization.ratio;
      break;
    case EnvLevel2Key::Data:
      if (tokens.size() == 2)
        return std::string("");
      switch (hashString(tokens[2].c_str())) {
      case hashString("warmup_candles_num"):
        if (tokens.size() == 3)
          return conf_.env.data.warmup_candles_num;
        break;
      case hashString("generate_candles_from_1m"):
        if (tokens.size() == 3)
          return conf_.env.data.generate_candles_from_1m;
        break;
      case hashString("persistency"):
        if (tokens.size() == 3)
          return conf_.env.data.persistency;
        break;
      }
      break;
    case EnvLevel2Key::Invalid:
      break;
    }
    break;
  case Level1Key::App:
    if (tokens.size() == 1)
      return std::string("");
    switch (toAppLevel2Key(tokens[1])) {
    case AppLevel2Key::ConsideringSymbols:
      if (tokens.size() == 2)
        return conf_.app.considering_symbols;
      break;
    case AppLevel2Key::TradingSymbols:
      if (tokens.size() == 2)
        return conf_.app.trading_symbols;
      break;
    case AppLevel2Key::ConsideringTimeframes:
      if (tokens.size() == 2)
        return conf_.app.considering_timeframes;
      break;
    case AppLevel2Key::TradingTimeframes:
      if (tokens.size() == 2)
        return conf_.app.trading_timeframes;
      break;
    case AppLevel2Key::ConsideringExchanges:
      if (tokens.size() == 2)
        return conf_.app.trading_exchanges;
      break;
    case AppLevel2Key::TradingExchanges:
      if (tokens.size() == 2)
        return conf_.app.trading_exchanges;
      break;
    case AppLevel2Key::ConsideringCandles:
      if (tokens.size() == 2)
        return conf_.app.considering_candles;
      break;
    case AppLevel2Key::LiveDrivers:
      if (tokens.size() == 2)
        return conf_.app.live_drivers;
      break;
    case AppLevel2Key::TradingMode:
      if (tokens.size() == 2)
        return conf_.app.trading_mode;
      break;
    case AppLevel2Key::DebugMode:
      if (tokens.size() == 2)
        return conf_.app.debug_mode;
      break;
    case AppLevel2Key::IsUnitTesting:
      if (tokens.size() == 2)
        return conf_.app.is_unit_testing;
      break;
    case AppLevel2Key::Invalid:
      break;
    }
    break;
  case Level1Key::Invalid:
    break;
  }

  return std::string(""); // Default for invalid or incomplete path
}

ConfValue Config::fetchValue(const std::string &keys,
                             const ConfValue &defaultValue) const {
  std::string envKey = keys;
  std::replace(envKey.begin(), envKey.end(), '.', '_');
  std::transform(envKey.begin(), envKey.end(), envKey.begin(), ::toupper);
  const char *envValue = std::getenv(envKey.c_str());
  if (envValue != nullptr) {
    return fromEnvString(envValue);
  }

  ConfValue nestedValue = getNestedValue(keys);
  if (std::holds_alternative<std::string>(nestedValue) &&
      std::get<std::string>(nestedValue).empty()) {
    return defaultValue;
  }
  return nestedValue;
}

ConfValue Config::fromEnvString(const std::string &value) const {
  // Try to parse as common types; default to string if unsure
  std::istringstream iss(value);
  int i;
  double d;

  if (iss >> i && iss.eof())
    return i;
  iss.clear();
  iss.str(value);
  if (iss >> d && iss.eof())
    return d;
  iss.clear();
  iss.str(value);
  if (value == "true")
    return true;
  if (value == "false")
    return false;
  return value; // Fallback to string
}

} // namespace Config
