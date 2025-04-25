#ifndef CIPHER_INFO_HPP
#define CIPHER_INFO_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include "Enum.hpp"

namespace ct
{
namespace info
{

extern const std::string CIPHER_TRADER_API_URL;
extern const std::string CIPHER_TRADER_WEBSITE_URL;

extern const std::vector< ct::enums::Timeframe > BYBIT_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > BINANCE_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > COINBASE_TIMEFRAMES;

extern const std::vector< ct::enums::Timeframe > APEX_PRO_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > GATE_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > FTX_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > BITGET_TIMEFRAMES;
extern const std::vector< ct::enums::Timeframe > DYDX_TIMEFRAMES;

using ExchangeInfo = std::variant< std::string,
                                   double,
                                   bool,
                                   ct::enums::ExchangeType,
                                   std::vector< ct::enums::LeverageMode >,
                                   std::vector< ct::enums::Timeframe >,
                                   std::unordered_map< std::string, bool > >;

template < typename T >
std::string vectorToString(const std::vector< T > &vec);

template < typename T >
std::string unorderedMapToString(const std::unordered_map< T, bool > &map);

std::string toString(const ExchangeInfo &var);

extern const std::unordered_map< ct::enums::Exchange, std::unordered_map< std::string, ExchangeInfo > > EXCHANGE_INFO;

std::vector< std::string > getExchangesByMode(const std::string &mode);

extern const std::vector< std::string > BACKTESTING_EXCHANGES;
extern const std::vector< std::string > LIVE_TRADING_EXCHANGES;

extern const std::vector< ct::enums::Timeframe > CIPHER_TRADER_SUPPORTED_TIMEFRAMES;

} // namespace info
} // namespace ct

#endif
