#include "Enum.hpp"

const std::string ct::enums::toString(OrderSide order_side)
{
    return (order_side == OrderSide::BUY) ? "buy" : "sell";
}

const std::string ct::enums::toString(PositionType position_type)
{
    if (position_type == PositionType::LONG)
    {
        return "long";
    }
    else if (position_type == PositionType::SHORT)
    {
        return "short";
    }
    else if (position_type == PositionType::CLOSE)
    {
        return "close";
    }
    throw std::invalid_argument("Invalid PositionType");
}

const std::string ct::enums::toString(OrderStatus order_status)
{
    switch (order_status)
    {
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
            throw std::invalid_argument("Invalid OrderStatus");
    }
}

const std::string ct::enums::toString(OrderType order_type)
{
    switch (order_type)
    {
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
            throw std::invalid_argument("Invalid OrderType");
    }
}

const std::string ct::enums::toString(Color color)
{
    switch (color)
    {
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
            throw std::invalid_argument("Invalid Color");
    }
}

const std::string ct::enums::toString(ExchangeName exchange_name)
{
    switch (exchange_name)
    {
        case ExchangeName::SANDBOX:
            return "Sandbox";
        case ExchangeName::COINBASE_SPOT:
            return "Coinbase Spot";
        case ExchangeName::BITFINEX_SPOT:
            return "Bitfinex Spot";
        case ExchangeName::BINANCE_SPOT:
            return "Binance Spot";
        case ExchangeName::BINANCE_US_SPOT:
            return "Binance US Spot";
        case ExchangeName::BINANCE_PERPETUAL_FUTURES:
            return "Binance Perpetual Futures";
        case ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET:
            return "Binance Perpetual Futures Testnet";
        case ExchangeName::BYBIT_USDT_PERPETUAL:
            return "Bybit USDT Perpetual";
        case ExchangeName::BYBIT_USDC_PERPETUAL:
            return "Bybit USDC Perpetual";
        case ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET:
            return "Bybit USDT Perpetual Testnet";
        case ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET:
            return "Bybit USDC Perpetual Testnet";
        case ExchangeName::BYBIT_SPOT:
            return "Bybit Spot";
        case ExchangeName::BYBIT_SPOT_TESTNET:
            return "Bybit Spot Testnet";
        case ExchangeName::FTX_PERPETUAL_FUTURES:
            return "FTX Perpetual Futures";
        case ExchangeName::FTX_SPOT:
            return "FTX Spot";
        case ExchangeName::FTX_US_SPOT:
            return "FTX US Spot";
        case ExchangeName::BITGET_SPOT:
            return "Bitget Spot";
        case ExchangeName::BITGET_USDT_PERPETUAL:
            return "Bitget USDT Perpetual";
        case ExchangeName::BITGET_USDT_PERPETUAL_TESTNET:
            return "Bitget USDT Perpetual Testnet";
        case ExchangeName::DYDX_PERPETUAL:
            return "Dydx Perpetual";
        case ExchangeName::DYDX_PERPETUAL_TESTNET:
            return "Dydx Perpetual Testnet";
        case ExchangeName::APEX_PRO_PERPETUAL_TESTNET:
            return "Apex Pro Perpetual Testnet";
        case ExchangeName::APEX_PRO_PERPETUAL:
            return "Apex Pro Perpetual";
        case ExchangeName::APEX_OMNI_PERPETUAL_TESTNET:
            return "Apex Omni Perpetual Testnet";
        case ExchangeName::APEX_OMNI_PERPETUAL:
            return "Apex Omni Perpetual";
        case ExchangeName::GATE_USDT_PERPETUAL:
            return "Gate USDT Perpetual";
        case ExchangeName::GATE_SPOT:
            return "Gate Spot";
        default:
            throw std::invalid_argument("Invalid ExchangeName");
    }
}

const std::string ct::enums::toString(ct::enums::ExchangeType exchange_type)
{
    switch (exchange_type)
    {
        case ExchangeType::SPOT:
            return "SPOT";
        case ExchangeType::FUTURES:
            return "FUTURES";
        default:
            throw std::invalid_argument("Invalid ExchangeType");
    }
}

const std::string ct::enums::toString(ct::enums::LeverageMode leverage_mode)
{
    switch (leverage_mode)
    {
        case LeverageMode::CROSS:
            return "CROSS";
        case LeverageMode::ISOLATED:
            return "ISOLATED";
        default:
            throw std::invalid_argument("Invalid LeverageMode");
    }
}

const std::string ct::enums::toString(MigrationAction action)
{
    switch (action)
    {
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
            throw std::invalid_argument("Invalid MigrationAction");
    }
}

const std::string ct::enums::toString(OrderSubmittedVia method)
{
    switch (method)
    {
        case OrderSubmittedVia::STOP_LOSS:
            return "stop-loss";
        case OrderSubmittedVia::TAKE_PROFIT:
            return "take-profit";
        default:
            throw std::invalid_argument("Invalid OrderSubmittedVia");
    }
}

ct::enums::OrderSide ct::enums::toOrderSide(const std::string &order_side)
{
    if (order_side == "buy")
    {
        return OrderSide::BUY;
    }
    else if (order_side == "sell")
    {
        return OrderSide::SELL;
    }
    throw std::invalid_argument("Invalid OrderSide string: " + order_side);
}

ct::enums::PositionType ct::enums::toPositionType(const std::string &position_type)
{
    if (position_type == "short")
    {
        return PositionType::SHORT;
    }
    else if (position_type == "long")
    {
        return PositionType::LONG;
    }
    else if (position_type == "close")
    {
        return PositionType::CLOSE;
    }
    else
    {
        throw std::invalid_argument("Invalid PositionType string: " + position_type);
    }
}

ct::enums::OrderStatus ct::enums::toOrderStatus(const std::string &order_status)
{
    if (order_status == "active")
    {
        return OrderStatus::ACTIVE;
    }
    else if (order_status == "canceled")
    {
        return OrderStatus::CANCELED;
    }
    else if (order_status == "executed")
    {
        return OrderStatus::EXECUTED;
    }
    else if (order_status == "partially_filled")
    {
        return OrderStatus::PARTIALLY_FILLED;
    }
    else if (order_status == "queued")
    {
        return OrderStatus::QUEUED;
    }
    else if (order_status == "liquidated")
    {
        return OrderStatus::LIQUIDATED;
    }
    else if (order_status == "rejected")
    {
        return OrderStatus::REJECTED;
    }
    else
    {
        throw std::invalid_argument("Invalid OrderStatus string: " + order_status);
    }
}

ct::enums::OrderType ct::enums::toOrderType(const std::string &order_type)
{
    if (order_type == "MARKET")
    {
        return OrderType::MARKET;
    }
    else if (order_type == "LIMIT")
    {
        return OrderType::LIMIT;
    }
    else if (order_type == "STOP")
    {
        return OrderType::STOP;
    }
    else if (order_type == "FOK")
    {
        return OrderType::FOK;
    }
    else if (order_type == "STOP LIMIT")
    {
        return OrderType::STOP_LIMIT;
    }
    throw std::invalid_argument("Invalid OrderType string: " + order_type);
}


ct::enums::ExchangeName ct::enums::toExchangeName(const std::string &exchange_name)
{
    static const std::unordered_map< std::string, ExchangeName > exchange_name_map = {
        {"Sandbox", ExchangeName::SANDBOX},
        {"Coinbase Spot", ExchangeName::COINBASE_SPOT},
        {"Bitfinex Spot", ExchangeName::BITFINEX_SPOT},
        {"Binance Spot", ExchangeName::BINANCE_SPOT},
        {"Binance US Spot", ExchangeName::BINANCE_US_SPOT},
        {"Binance Perpetual Futures", ExchangeName::BINANCE_PERPETUAL_FUTURES},
        {"Binance Perpetual Futures Testnet", ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET},
        {"Bybit USDT Perpetual", ExchangeName::BYBIT_USDT_PERPETUAL},
        {"Bybit USDC Perpetual", ExchangeName::BYBIT_USDC_PERPETUAL},
        {"Bybit USDT Perpetual Testnet", ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET},
        {"Bybit USDC Perpetual Testnet", ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET},
        {"Bybit Spot", ExchangeName::BYBIT_SPOT},
        {"Bybit Spot Testnet", ExchangeName::BYBIT_SPOT_TESTNET},
        {"FTX Perpetual Futures", ExchangeName::FTX_PERPETUAL_FUTURES},
        {"FTX Spot", ExchangeName::FTX_SPOT},
        {"FTX US Spot", ExchangeName::FTX_US_SPOT},
        {"Bitget Spot", ExchangeName::BITGET_SPOT},
        {"Bitget USDT Perpetual", ExchangeName::BITGET_USDT_PERPETUAL},
        {"Bitget USDT Perpetual Testnet", ExchangeName::BITGET_USDT_PERPETUAL_TESTNET},
        {"Dydx Perpetual", ExchangeName::DYDX_PERPETUAL},
        {"Dydx Perpetual Testnet", ExchangeName::DYDX_PERPETUAL_TESTNET},
        {"Apex Pro Perpetual Testnet", ExchangeName::APEX_PRO_PERPETUAL_TESTNET},
        {"Apex Pro Perpetual", ExchangeName::APEX_PRO_PERPETUAL},
        {"Apex Omni Perpetual Testnet", ExchangeName::APEX_OMNI_PERPETUAL_TESTNET},
        {"Apex Omni Perpetual", ExchangeName::APEX_OMNI_PERPETUAL},
        {"Gate USDT Perpetual", ExchangeName::GATE_USDT_PERPETUAL},
        {"Gate Spot", ExchangeName::GATE_SPOT}};

    auto it = exchange_name_map.find(exchange_name);
    if (it == exchange_name_map.end())
    {
        throw std::invalid_argument("Invalid ExchangeName string: " + exchange_name);
    }
    return it->second;
}


ct::enums::ExchangeType ct::enums::toExchangeType(const std::string &exchange_type)
{
    std::string upperStr = exchange_type;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    if (upperStr == "SPOT")
        return ExchangeType::SPOT;
    else if (upperStr == "FUTURES")
        return ExchangeType::FUTURES;
    else
        throw std::invalid_argument("Invalid ExchangeType string: " + exchange_type);
}

ct::enums::LeverageMode ct::enums::toLeverageMode(const std::string &leverage_mode)
{
    std::string upperStr = leverage_mode;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    if (upperStr == "CROSS")
        return LeverageMode::CROSS;
    else if (upperStr == "ISOLATED")
        return LeverageMode::ISOLATED;
    else
        throw std::invalid_argument("Invalid LeverageMode string: " + leverage_mode);
}

