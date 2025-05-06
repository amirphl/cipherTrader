#ifndef CIPHER_ENUM_HPP
#define CIPHER_ENUM_HPP

#include "Precompiled.hpp"

// TODO:: Move each enum to related header file.

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

enum class OrderType
{
    MARKET,
    LIMIT,
    STOP,
    FOK,
    STOP_LIMIT
};

enum class Color
{
    GREEN,
    YELLOW,
    RED,
    MAGENTA,
    BLACK
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
OrderSide toOrderSide(const std::string &order_side);
const std::string toString(PositionType position_type);
PositionType toPositionType(const std::string &position_type);
const std::string toString(OrderStatus order_status);
OrderStatus toOrderStatus(const std::string &order_status);
const std::string toString(OrderType order_type);
OrderType toOrderType(const std::string &order_type);
const std::string toString(Color color);
const std::string toString(ExchangeName exchange_name);
ExchangeName toExchangeName(const std::string &exchange_name);
const std::string toString(ExchangeType exchange_type);
ExchangeType toExchangeType(const std::string &exchange_type);
const std::string toString(LeverageMode leverage_move);
LeverageMode toLeverageMode(const std::string &leverage_move);
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
