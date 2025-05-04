#ifndef CIPHER_TIMEFRAME_HPP
#define CIPHER_TIMEFRAME_HPP

#include "Precompiled.hpp"

#include "Enum.hpp"

namespace ct
{
namespace timeframe
{

extern const std::vector< enums::Timeframe > BYBIT_TIMEFRAMES;
extern const std::vector< enums::Timeframe > BINANCE_TIMEFRAMES;
extern const std::vector< enums::Timeframe > COINBASE_TIMEFRAMES;

extern const std::vector< enums::Timeframe > APEX_PRO_TIMEFRAMES;
extern const std::vector< enums::Timeframe > GATE_TIMEFRAMES;
extern const std::vector< enums::Timeframe > FTX_TIMEFRAMES;
extern const std::vector< enums::Timeframe > BITGET_TIMEFRAMES;
extern const std::vector< enums::Timeframe > DYDX_TIMEFRAMES;

// TODO:Move to utils.
template < typename T >
std::string vectorToString(const std::vector< T > &vec);

// TODO:Move to utils.
template < typename T >
std::string unorderedMapToString(const std::unordered_map< T, bool > &map);

extern const std::vector< enums::Timeframe > CIPHER_TRADER_SUPPORTED_TIMEFRAMES;

} // namespace timeframe
} // namespace ct

#endif
