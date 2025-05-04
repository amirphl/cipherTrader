#ifndef CIPHER_ENUM_HPP
#define CIPHER_ENUM_HPP

#include "Precompiled.hpp"

namespace ct
{
namespace enums
{

enum class OrderSide
{
    BUY,
    SELL,
};

enum class PositionType
{
    LONG,
    SHORT,
    CLOSE,
};

enum class OrderStatus
{
    ACTIVE,
    CANCELED,
    EXECUTED,
    PARTIALLY_FILLED,
    QUEUED,
    LIQUIDATED,
    REJECTED
};

enum class Timeframe
{
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

enum class Color
{
    GREEN,
    YELLOW,
    RED,
    MAGENTA,
    BLACK
};

enum class OrderType
{
    MARKET,
    LIMIT,
    STOP,
    FOK,
    STOP_LIMIT
};

enum class ExchangeName
{
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

enum class ExchangeType
{
    SPOT,
    FUTURES,
};

enum class LeverageMode
{
    CROSS,
    ISOLATED
};

enum class MigrationAction
{
    ADD,
    DROP,
    RENAME,
    MODIFY_TYPE,
    ALLOW_NULL,
    DENY_NULL,
    ADD_INDEX,
    DROP_INDEX
};

enum class OrderSubmittedVia
{
    STOP_LOSS,
    TAKE_PROFIT
};

const std::string toString(OrderSide side);
OrderSide toOrderSide(const std::string &orderSideStr);
const std::string toString(PositionType positionType);
PositionType toPositionType(const std::string &positionTypeStr);
const std::string toString(OrderStatus orderStatus);
OrderStatus toOrderStatus(const std::string &statusStr);
const std::string toString(Timeframe timeframe);
Timeframe toTimeframe(const std::string &timeframeStr);
const std::string toString(Color color);
const std::string toString(OrderType orderType);
OrderType toOrderType(const std::string &orderTypeStr);
const std::string toString(ExchangeName exchangeName);
ExchangeName toExchangeName(const std::string &exchangeNameStr);
const std::string toString(ExchangeType exchangeType);
ExchangeType toExchangeType(const std::string &exchangeTypeStr);
const std::string toString(LeverageMode leverageMode);
LeverageMode toLeverageMode(const std::string &leverageModeStr);
const std::string toString(MigrationAction action);
const std::string toString(OrderSubmittedVia method);

inline std::ostream &operator<<(std::ostream &os, const ct::enums::OrderSide &order_side)
{
    return os << toString(order_side);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::PositionType &position_type)
{
    return os << toString(position_type);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::OrderStatus &order_status)
{
    return os << toString(order_status);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::Timeframe &timeframe)
{
    return os << toString(timeframe);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::OrderType &order_type)
{
    return os << toString(order_type);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::ExchangeName &exchange_name)
{
    return os << toString(exchange_name);
}

inline std::ostream &operator<<(std::ostream &os, const ct::enums::OrderSubmittedVia &order_submitted_via)
{
    return os << toString(order_submitted_via);
}

} // namespace enums
} // namespace ct

#endif
