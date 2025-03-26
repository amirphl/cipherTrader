#ifndef INFO_HPP
#define INFO_HPP

#include "Enum.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Info {

using namespace Enum;

extern const std::string CIPHER_TRADER_API_URL;
extern const std::string CIPHER_TRADER_WEBSITE_URL;

extern const std::vector<Timeframe> BYBIT_TIMEFRAMES;
extern const std::vector<Timeframe> BINANCE_TIMEFRAMES;
extern const std::vector<Timeframe> COINBASE_TIMEFRAMES;

extern const std::vector<Timeframe> APEX_PRO_TIMEFRAMES;
extern const std::vector<Timeframe> GATE_TIMEFRAMES;
extern const std::vector<Timeframe> FTX_TIMEFRAMES;
extern const std::vector<Timeframe> BITGET_TIMEFRAMES;
extern const std::vector<Timeframe> DYDX_TIMEFRAMES;

using ExchangeInfo =
    std::variant<std::string, double, bool, std::vector<std::string>,
                 std::vector<Timeframe>, std::unordered_map<std::string, bool>>;

template <typename T> std::string vectorToString(const std::vector<T> &vec);

template <typename T>
std::string unorderedMapToString(const std::unordered_map<T, bool> &map);

std::string toString(const ExchangeInfo &var);

extern const std::unordered_map<Exchange,
                                std::unordered_map<std::string, ExchangeInfo>>
    EXCHANGE_INFO;

std::vector<std::string> getExchangesByMode(const std::string &mode);

extern const std::vector<std::string> BACKTESTING_EXCHANGES;
extern const std::vector<std::string> LIVE_TRADING_EXCHANGES;

extern const std::vector<Timeframe> CIPHER_TRADER_SUPPORTED_TIMEFRAMES;

} // namespace Info

#endif
