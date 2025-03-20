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

inline std::string toString(Side side) {
  return (side == Side::BUY) ? "buy" : "sell";
}

inline std::string toString(TradeType tradeType) {
  return (tradeType == TradeType::LONG) ? "long" : "short";
}

inline std::string toString(OrderStatus orderStatus) {
  switch (orderStatus) {
  case OrderStatus::ACTIVE:
    return "active";
  case OrderStatus::CANCELED:
    return "canceled";
  case OrderStatus::EXECUTED:
    return "executed";
  case OrderStatus::PARTIALLY_FILLED:
    return "partially_filled";
  case OrderStatus::QUEUED:
    return "queued";
  case OrderStatus::LIQUIDATED:
    return "liquidated";
  case OrderStatus::REJECTED:
    return "rejected";
  default:
    return "unknown";
  }
}

inline std::string toString(Timeframe timeframe) {
  switch (timeframe) {
  case Timeframe::MINUTE_1:
    return "1m";
  case Timeframe::MINUTE_3:
    return "3m";
  case Timeframe::MINUTE_5:
    return "5m";
  case Timeframe::MINUTE_15:
    return "15m";
  case Timeframe::MINUTE_30:
    return "30m";
  case Timeframe::MINUTE_45:
    return "45m";
  case Timeframe::HOUR_1:
    return "1h";
  case Timeframe::HOUR_2:
    return "2h";
  case Timeframe::HOUR_3:
    return "3h";
  case Timeframe::HOUR_4:
    return "4h";
  case Timeframe::HOUR_6:
    return "6h";
  case Timeframe::HOUR_8:
    return "8h";
  case Timeframe::HOUR_12:
    return "12h";
  case Timeframe::DAY_1:
    return "1D";
  case Timeframe::DAY_3:
    return "3D";
  case Timeframe::WEEK_1:
    return "1W";
  case Timeframe::MONTH_1:
    return "1M";
  default:
    return "UNKNOWN";
  }
}

inline std::string toString(Color color) {
  switch (color) {
  case Color::GREEN:
    return "green";
  case Color::YELLOW:
    return "yellow";
  case Color::RED:
    return "red";
  case Color::MAGENTA:
    return "magenta";
  case Color::BLACK:
    return "black";
  default:
    return "UNKNOWN";
  }
}

inline std::string toString(OrderType orderType) {
  switch (orderType) {
  case OrderType::MARKET:
    return "MARKET";
  case OrderType::LIMIT:
    return "LIMIT";
  case OrderType::STOP:
    return "STOP";
  case OrderType::FOK:
    return "FOK";
  case OrderType::STOP_LIMIT:
    return "STOP LIMIT";
  default:
    return "UNKNOWN";
  }
}

inline std::string toString(Exchange exchange) {
  switch (exchange) {
  case Exchange::SANDBOX:
    return "Sandbox";
  case Exchange::COINBASE_SPOT:
    return "Coinbase Spot";
  case Exchange::BITFINEX_SPOT:
    return "Bitfinex Spot";
  case Exchange::BINANCE_SPOT:
    return "Binance Spot";
  case Exchange::BINANCE_US_SPOT:
    return "Binance US Spot";
  case Exchange::BINANCE_PERPETUAL_FUTURES:
    return "Binance Perpetual Futures";
  case Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET:
    return "Binance Perpetual Futures Testnet";
  case Exchange::BYBIT_USDT_PERPETUAL:
    return "Bybit USDT Perpetual";
  case Exchange::BYBIT_USDC_PERPETUAL:
    return "Bybit USDC Perpetual";
  case Exchange::BYBIT_USDT_PERPETUAL_TESTNET:
    return "Bybit USDT Perpetual Testnet";
  case Exchange::BYBIT_USDC_PERPETUAL_TESTNET:
    return "Bybit USDC Perpetual Testnet";
  case Exchange::BYBIT_SPOT:
    return "Bybit Spot";
  case Exchange::BYBIT_SPOT_TESTNET:
    return "Bybit Spot Testnet";
  case Exchange::FTX_PERPETUAL_FUTURES:
    return "FTX Perpetual Futures";
  case Exchange::FTX_SPOT:
    return "FTX Spot";
  case Exchange::FTX_US_SPOT:
    return "FTX US Spot";
  case Exchange::BITGET_SPOT:
    return "Bitget Spot";
  case Exchange::BITGET_USDT_PERPETUAL:
    return "Bitget USDT Perpetual";
  case Exchange::BITGET_USDT_PERPETUAL_TESTNET:
    return "Bitget USDT Perpetual Testnet";
  case Exchange::DYDX_PERPETUAL:
    return "Dydx Perpetual";
  case Exchange::DYDX_PERPETUAL_TESTNET:
    return "Dydx Perpetual Testnet";
  case Exchange::APEX_PRO_PERPETUAL_TESTNET:
    return "Apex Pro Perpetual Testnet";
  case Exchange::APEX_PRO_PERPETUAL:
    return "Apex Pro Perpetual";
  case Exchange::APEX_OMNI_PERPETUAL_TESTNET:
    return "Apex Omni Perpetual Testnet";
  case Exchange::APEX_OMNI_PERPETUAL:
    return "Apex Omni Perpetual";
  case Exchange::GATE_USDT_PERPETUAL:
    return "Gate USDT Perpetual";
  case Exchange::GATE_SPOT:
    return "Gate Spot";
  default:
    return "UNKNOWN";
  }
}

inline std::string toString(MigrationAction action) {
  switch (action) {
  case MigrationAction::ADD:
    return "add";
  case MigrationAction::DROP:
    return "drop";
  case MigrationAction::RENAME:
    return "rename";
  case MigrationAction::MODIFY_TYPE:
    return "modify_type";
  case MigrationAction::ALLOW_NULL:
    return "allow_null";
  case MigrationAction::DENY_NULL:
    return "deny_null";
  case MigrationAction::ADD_INDEX:
    return "add_index";
  case MigrationAction::DROP_INDEX:
    return "drop_index";
  default:
    return "UNKNOWN";
  }
}

inline std::string toString(OrderSubmittedVia method) {
  switch (method) {
  case OrderSubmittedVia::STOP_LOSS:
    return "stop-loss";
  case OrderSubmittedVia::TAKE_PROFIT:
    return "take-profit";
  default:
    return "UNKNOWN";
  }
}

} // namespace Enum

#endif
