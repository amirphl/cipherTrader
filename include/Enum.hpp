#ifndef ENUM_HPP
#define ENUM_HPP

#include <string>

namespace Enum {

enum class Side {
  BUY,
  SELL,
};

enum class TradeType {
  LONG,
  SHORT,
};

enum class OrderStatus {
  ACTIVE,
  CANCELED,
  EXECUTED,
  PARTIALLY_FILLED,
  QUEUED,
  LIQUIDATED,
  REJECTED
};

enum class Timeframe {
  MINUTE_1,
  MINUTE_3,
  MINUTE_5,
  MINUTE_15,
  MINUTE_30,
  MINUTE_45,
  HOUR_1,
  HOUR_2,
  HOUR_3,
  HOUR_4,
  HOUR_6,
  HOUR_8,
  HOUR_12,
  DAY_1,
  DAY_3,
  WEEK_1,
  MONTH_1
};

enum class Color { GREEN, YELLOW, RED, MAGENTA, BLACK };

enum class OrderType { MARKET, LIMIT, STOP, FOK, STOP_LIMIT };

enum class Exchange {
  SANDBOX,
  COINBASE_SPOT,
  BITFINEX_SPOT,
  BINANCE_SPOT,
  BINANCE_US_SPOT,
  BINANCE_PERPETUAL_FUTURES,
  BINANCE_PERPETUAL_FUTURES_TESTNET,
  BYBIT_USDT_PERPETUAL,
  BYBIT_USDC_PERPETUAL,
  BYBIT_USDT_PERPETUAL_TESTNET,
  BYBIT_USDC_PERPETUAL_TESTNET,
  BYBIT_SPOT,
  BYBIT_SPOT_TESTNET,
  FTX_PERPETUAL_FUTURES,
  FTX_SPOT,
  FTX_US_SPOT,
  BITGET_SPOT,
  BITGET_USDT_PERPETUAL,
  BITGET_USDT_PERPETUAL_TESTNET,
  DYDX_PERPETUAL,
  DYDX_PERPETUAL_TESTNET,
  APEX_PRO_PERPETUAL_TESTNET,
  APEX_PRO_PERPETUAL,
  APEX_OMNI_PERPETUAL_TESTNET,
  APEX_OMNI_PERPETUAL,
  GATE_USDT_PERPETUAL,
  GATE_SPOT
};

enum class MigrationAction {
  ADD,
  DROP,
  RENAME,
  MODIFY_TYPE,
  ALLOW_NULL,
  DENY_NULL,
  ADD_INDEX,
  DROP_INDEX
};

enum class OrderSubmittedVia { STOP_LOSS, TAKE_PROFIT };

std::string toString(Side side);
std::string toString(TradeType tradeType);
std::string toString(OrderStatus orderStatus);
std::string toString(Timeframe timeframe);
Timeframe toTimeframe(const std::string &timeframe_str);
std::string toString(Color color);
std::string toString(OrderType orderType);
std::string toString(Exchange exchange);
Exchange toExchange(const std::string &exchange_str);
std::string toString(MigrationAction action);
std::string toString(OrderSubmittedVia method);

} // namespace Enum

#endif
