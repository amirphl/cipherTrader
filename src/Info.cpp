#include "Info.hpp"
#include <sstream>
#include <vector>

namespace Info {

const std::string CIPHER_TRADER_API_URL{
    "https://api.ciphertrader.trade/api/v1"};

const std::string CIPHER_TRADER_WEBSITE_URL{"https://ciphertrader.trade"};

const std::vector<Timeframe> BYBIT_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_3,  Timeframe::MINUTE_5,
    Timeframe::MINUTE_15, Timeframe::MINUTE_30, Timeframe::HOUR_1,
    Timeframe::HOUR_2,    Timeframe::HOUR_4,    Timeframe::HOUR_6,
    Timeframe::HOUR_12,   Timeframe::DAY_1,
};

const std::vector<Timeframe> BINANCE_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_3,  Timeframe::MINUTE_5,
    Timeframe::MINUTE_15, Timeframe::MINUTE_30, Timeframe::HOUR_1,
    Timeframe::HOUR_2,    Timeframe::HOUR_4,    Timeframe::HOUR_6,
    Timeframe::HOUR_8,    Timeframe::HOUR_12,   Timeframe::DAY_1,
};

const std::vector<Timeframe> COINBASE_TIMEFRAMES{
    Timeframe::MINUTE_1, Timeframe::MINUTE_5, Timeframe::MINUTE_15,
    Timeframe::HOUR_1,   Timeframe::HOUR_6,   Timeframe::DAY_1,
};

const std::vector<Timeframe> APEX_PRO_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_5, Timeframe::MINUTE_15,
    Timeframe::MINUTE_30, Timeframe::HOUR_1,   Timeframe::HOUR_2,
    Timeframe::HOUR_4,    Timeframe::HOUR_6,   Timeframe::HOUR_12,
    Timeframe::DAY_1,
};

const std::vector<Timeframe> GATE_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_5, Timeframe::MINUTE_15,
    Timeframe::MINUTE_30, Timeframe::HOUR_1,   Timeframe::HOUR_2,
    Timeframe::HOUR_4,    Timeframe::HOUR_6,   Timeframe::HOUR_8,
    Timeframe::HOUR_12,   Timeframe::DAY_1,    Timeframe::WEEK_1,
};

const std::vector<Timeframe> FTX_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_3,  Timeframe::MINUTE_5,
    Timeframe::MINUTE_15, Timeframe::MINUTE_30, Timeframe::HOUR_1,
    Timeframe::HOUR_2,    Timeframe::HOUR_4,    Timeframe::HOUR_6,
    Timeframe::HOUR_12,   Timeframe::DAY_1,
};

const std::vector<Timeframe> BITGET_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_5, Timeframe::MINUTE_15,
    Timeframe::MINUTE_30, Timeframe::HOUR_1,   Timeframe::HOUR_4,
    Timeframe::HOUR_12,   Timeframe::DAY_1,
};

const std::vector<Timeframe> DYDX_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_5, Timeframe::MINUTE_15,
    Timeframe::MINUTE_30, Timeframe::HOUR_1,   Timeframe::HOUR_4,
    Timeframe::DAY_1,
};

const std::unordered_map<Exchange,
                         std::unordered_map<std::string, ExchangeInfo>>
    EXCHANGE_INFO{
        {Exchange::BYBIT_USDC_PERPETUAL,
         {
             {"name", toString(Exchange::BYBIT_USDT_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", "futures"},
             {"settlement_currency", "USDT"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BYBIT_USDT_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::BYBIT_USDT_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", "futures"},
             {"settlement_currency", "USDT"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BYBIT_USDC_PERPETUAL,
         {
             {"name", toString(Exchange::BYBIT_USDC_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", "futures"},
             {"settlement_currency", "USDC"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BYBIT_USDC_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::BYBIT_USDC_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"settlement_currency", "USDC"},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BYBIT_SPOT,
         {
             {"name", toString(Exchange::BYBIT_SPOT)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.001},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BYBIT_SPOT_TESTNET,
         {
             {"name", toString(Exchange::BYBIT_SPOT_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.001},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BITFINEX_SPOT,
         {
             {"name", toString(Exchange::BITFINEX_SPOT)},
             {"url", "https://bitfinex.com"},
             {"fee", 0.002},
             {"type", "spot"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes",
              std::vector<Timeframe>{
                  Timeframe::MINUTE_1,
                  Timeframe::MINUTE_5,
                  Timeframe::MINUTE_15,
                  Timeframe::MINUTE_30,
                  Timeframe::HOUR_1,
                  Timeframe::HOUR_3,
                  Timeframe::HOUR_6,
                  Timeframe::HOUR_12,
                  Timeframe::DAY_1,
              }},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BINANCE_SPOT,
         {
             {"name", toString(Exchange::BINANCE_SPOT)},
             {"url", "https://binance.com"},
             {"fee", 0.001},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BINANCE_US_SPOT,
         {
             {"name", toString(Exchange::BINANCE_US_SPOT)},
             {"url", "https://binance.us"},
             {"fee", 0.001},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BINANCE_PERPETUAL_FUTURES,
         {
             {"name", toString(Exchange::BINANCE_PERPETUAL_FUTURES)},
             {"url", "https://binance.com"},
             {"fee", 0.0004},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET,
         {
             {"name", toString(Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET)},
             {"url", "https://binance.com"},
             {"fee", 0.0004},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::COINBASE_SPOT,
         {
             {"name", toString(Exchange::COINBASE_SPOT)},
             {"url", "https://www.coinbase.com/advanced-trade/spot/BTC-USD"},
             {"fee", 0.0003},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", COINBASE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::APEX_PRO_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::APEX_PRO_PERPETUAL_TESTNET)},
             {"url", "https://testnet.pro.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "free"},
         }},
        {Exchange::APEX_PRO_PERPETUAL,
         {
             {"name", toString(Exchange::APEX_PRO_PERPETUAL)},
             {"url", "https://pro.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::APEX_OMNI_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::APEX_OMNI_PERPETUAL_TESTNET)},
             {"url", "https://testnet.omni.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "free"},
         }},
        {Exchange::APEX_OMNI_PERPETUAL,
         {
             {"name", toString(Exchange::APEX_OMNI_PERPETUAL)},
             {"url", "https://omni.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::GATE_USDT_PERPETUAL,
         {
             {"name", toString(Exchange::GATE_USDT_PERPETUAL)},
             {"url", "https://jesse.trade/gate"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", GATE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", true},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::GATE_SPOT,
         {
             {"name", toString(Exchange::GATE_SPOT)},
             {"url", "https://jesse.trade/gate"},
             {"fee", 0.0005},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", GATE_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::FTX_PERPETUAL_FUTURES,
         {
             {"name", toString(Exchange::FTX_PERPETUAL_FUTURES)},
             {"url", "https://ftx.com/markets/future"},
             {"fee", 0.0006},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::FTX_SPOT,
         {
             {"name", toString(Exchange::FTX_SPOT)},
             {"url", "https://ftx.com/markets/spot"},
             {"fee", 0.0007},
             {"type", "spot"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::FTX_US_SPOT,
         {
             {"name", toString(Exchange::FTX_US_SPOT)},
             {"url", "https://ftx.us"},
             {"fee", 0.002},
             {"type", "spot"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BITGET_USDT_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::BITGET_USDT_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BITGET_USDT_PERPETUAL,
         {
             {"name", toString(Exchange::BITGET_USDT_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", "futures"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::BITGET_SPOT,
         {
             {"name", toString(Exchange::BITGET_SPOT)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", "spot"},
             {"supported_leverage_modes",
              std::vector<std::string>{"cross", "isolated"}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::DYDX_PERPETUAL,
         {
             {"name", toString(Exchange::DYDX_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/dydx"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", DYDX_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {Exchange::DYDX_PERPETUAL_TESTNET,
         {
             {"name", toString(Exchange::DYDX_PERPETUAL_TESTNET)},
             {"url", "https://trade.stage.dydx.exchange/trade/ETH-USD"},
             {"fee", 0.0005},
             {"type", "futures"},
             {"supported_leverage_modes", std::vector<std::string>{"cross"}},
             {"supported_timeframes", DYDX_TIMEFRAMES},
             {"modes",
              std::unordered_map<std::string, bool>{{"backtesting", false},
                                                    {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},

    };

template <typename T> std::string vectorToString(const std::vector<T> &vec) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < vec.size(); ++i) {
    if constexpr (std::is_same_v<T, std::string>)
      oss << "\"" << vec[i] << "\"";
    else
      oss << vec[i];
    if (i != vec.size() - 1)
      oss << ", ";
  }
  oss << "]";
  return oss.str();
}

template <typename T>
std::string unorderedMapToString(const std::unordered_map<T, bool> &map) {
  std::ostringstream oss;
  oss << "{";
  bool first = true;
  for (const auto &[key, value] : map) {
    if (!first)
      oss << ", ";

    if constexpr (std::is_same_v<T, std::string>)
      oss << "\"" << key << "\": " << (value ? "true" : "false");
    else
      oss << key << ": " << (value ? "true" : "false");

    first = false;
  }
  oss << "}";
  return oss.str();
}

std::string toString(const ExchangeInfo &var) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::string>)
          return value;
        else if constexpr (std::is_same_v<T, double>)
          return std::to_string(value);
        else if constexpr (std::is_same_v<T, bool>)
          return value ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
          return vectorToString(value);
        else if constexpr (std::is_same_v<T, std::vector<Timeframe>>)
          return "[Timeframe objects]"; // Customize this if needed
        else if constexpr (std::is_same_v<
                               T, std::unordered_map<std::string, bool>>)
          return unorderedMapToString(value);
        else
          return "Unknown Type";
      },
      var);
}

std::vector<std::string> getExchangesByMode(const std::string &mode) {
  std::vector<std::string> exchanges;

  for (const auto &[exchange, info] : EXCHANGE_INFO) {
    auto it = info.find("modes");
    if (it != info.end()) {
      const auto &modes =
          std::get<std::unordered_map<std::string, bool>>(it->second);
      if (modes.at(mode)) {
        exchanges.push_back(toString(exchange));
      }
    }
  }

  std::sort(exchanges.begin(), exchanges.end());
  return exchanges;
}

const std::vector<std::string> BACKTESTING_EXCHANGES =
    getExchangesByMode("backtesting");

const std::vector<std::string> LIVE_TRADING_EXCHANGES =
    getExchangesByMode("live_trading");

const std::vector<Timeframe> CIPHER_TRADER_SUPPORTED_TIMEFRAMES{
    Timeframe::MINUTE_1,  Timeframe::MINUTE_3,  Timeframe::MINUTE_5,
    Timeframe::MINUTE_15, Timeframe::MINUTE_30, Timeframe::MINUTE_45,
    Timeframe::HOUR_1,    Timeframe::HOUR_2,    Timeframe::HOUR_3,
    Timeframe::HOUR_4,    Timeframe::HOUR_6,    Timeframe::HOUR_8,
    Timeframe::HOUR_12,   Timeframe::DAY_1,
};

} // namespace Info
