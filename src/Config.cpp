#include "Config.hpp"
#include "Helper.hpp"

namespace Config {

Config &Config::getInstance() {
  static Config instance;
  return instance;
}

void Config::initConfig() {
  // Initialize with defaults (already set in Conf struct)
  // Optionally override with environment variables
  reloadConfig();
  // TODO
}

void Config::reloadConfig() {
  // Clear cache if not unit testing
  if (!Helper::isUnitTesting()) {
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
  if (Helper::isUnitTesting() || cache_.find(keys) == cache_.end()) {
    value = fetchValue(keys, defaultValue);
    cache_.insert_or_assign(keys, value); // Replace operator[]
  } else {
    value = cache_.at(keys);
  }

  return value;
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

ConfValue Config::getNestedValue(const std::string &keys) const {
  std::istringstream iss(keys);
  std::string token;
  const void *current = &configData_;
  bool isValid = true;

  while (std::getline(iss, token, '.') && isValid) {
    if (current == &configData_) {
      if (token == "env")
        current = &configData_.env;
      else if (token == "app")
        current = &configData_.app;
      else
        isValid = false;
    } else if (current == &configData_.env) {
      if (token == "caching")
        current = &configData_.env.caching;
      else if (token == "logging")
        current = &configData_.env.logging;
      else if (token == "exchanges")
        current = &configData_.env.exchanges;
      else if (token == "optimization")
        current = &configData_.env.optimization;
      else if (token == "data")
        current = &configData_.env.data;
      else
        isValid = false;
    } else if (current == &configData_.env.caching) {
      if (token == "driver")
        return configData_.env.caching.driver;
      else
        isValid = false;
    } else if (current == &configData_.env.logging) {
      if (token == "order_submission")
        return configData_.env.logging.order_submission;
      else if (token == "order_cancellation")
        return configData_.env.logging.order_cancellation;
      else if (token == "order_execution")
        return configData_.env.logging.order_execution;
      else if (token == "position_opened")
        return configData_.env.logging.position_opened;
      else if (token == "position_increased")
        return configData_.env.logging.position_increased;
      else if (token == "position_reduced")
        return configData_.env.logging.position_reduced;
      else if (token == "position_closed")
        return configData_.env.logging.position_closed;
      else if (token == "shorter_period_candles")
        return configData_.env.logging.shorter_period_candles;
      else if (token == "trading_candles")
        return configData_.env.logging.trading_candles;
      else if (token == "balance_update")
        return configData_.env.logging.balance_update;
      else if (token == "exchange_ws_reconnection")
        return configData_.env.logging.exchange_ws_reconnection;
      else
        isValid = false;
    } else if (current == &configData_.env.exchanges) {
      auto it = configData_.env.exchanges.find(token);
      if (it != configData_.env.exchanges.end())
        current = &it->second;
      else
        isValid = false;
    } else if (const auto *exch =
                   static_cast<const Conf::Env::Exchange *>(current)) {
      if (token == "fee")
        return exch->fee;
      else if (token == "type")
        return exch->type;
      else if (token == "futures_leverage_mode")
        return exch->futures_leverage_mode;
      else if (token == "futures_leverage")
        return exch->futures_leverage;
      else if (token == "balance")
        return exch->balance;
      else
        isValid = false;
    } else if (current == &configData_.env.optimization) {
      if (token == "ratio")
        return configData_.env.optimization.ratio;
      else
        isValid = false;
    } else if (current == &configData_.env.data) {
      if (token == "warmup_candles_num")
        return configData_.env.data.warmup_candles_num;
      else if (token == "generate_candles_from_1m")
        return configData_.env.data.generate_candles_from_1m;
      else if (token == "persistency")
        return configData_.env.data.persistency;
      else
        isValid = false;
    } else if (current == &configData_.app) {
      if (token == "considering_symbols")
        return configData_.app.considering_symbols;
      else if (token == "trading_symbols")
        return configData_.app.trading_symbols;
      else if (token == "considering_timeframes")
        return configData_.app.considering_timeframes;
      else if (token == "trading_timeframes")
        return configData_.app.trading_timeframes;
      else if (token == "considering_exchanges")
        return configData_.app.trading_exchanges;
      else if (token == "trading_exchanges")
        return configData_.app.trading_exchanges;
      else if (token == "considering_candles")
        return configData_.app.considering_candles;
      else if (token == "live_drivers")
        return configData_.app.live_drivers;
      else if (token == "trading_mode")
        return configData_.app.trading_mode;
      else if (token == "debug_mode")
        return configData_.app.debug_mode;
      else if (token == "is_unit_testing")
        return configData_.app.is_unit_testing;
      else
        isValid = false;
    } else {
      isValid = false;
    }
  }
  return std::string(""); // Default return for invalid/incomplete path
}

} // namespace Config
