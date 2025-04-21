#include "Enum.hpp"
#include <unordered_map>

const std::string CipherEnum::toString(OrderSide side)
{
    return (side == OrderSide::BUY) ? "buy" : "sell";
}

CipherEnum::OrderSide CipherEnum::toOrderSide(const std::string &order_side_str)
{
    if (order_side_str == "buy")
    {
        return OrderSide::BUY;
    }
    else if (order_side_str == "sell")
    {
        return OrderSide::SELL;
    }
    throw std::invalid_argument("Invalid OrderSide string: " + order_side_str);
}

const std::string CipherEnum::toString(TradeType tradeType)
{
    return (tradeType == TradeType::LONG) ? LONG : SHORT;
}

const std::string CipherEnum::toString(Position position)
{
    return (position == Position::LONG) ? LONG : SHORT;
}

const std::string CipherEnum::toString(OrderStatus orderStatus)
{
    switch (orderStatus)
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
            return "unknown";
    }
}

CipherEnum::OrderStatus CipherEnum::toOrderStatus(const std::string &status_str)
{
    if (status_str == "active")
    {
        return OrderStatus::ACTIVE;
    }
    else if (status_str == "canceled")
    {
        return OrderStatus::CANCELED;
    }
    else if (status_str == "executed")
    {
        return OrderStatus::EXECUTED;
    }
    else if (status_str == "partially_filled")
    {
        return OrderStatus::PARTIALLY_FILLED;
    }
    else if (status_str == "queued")
    {
        return OrderStatus::QUEUED;
    }
    else if (status_str == "liquidated")
    {
        return OrderStatus::LIQUIDATED;
    }
    else if (status_str == "rejected")
    {
        return OrderStatus::REJECTED;
    }
    else
    {
        throw std::invalid_argument("Invalid order status: " + status_str);
    }
}

const std::string CipherEnum::toString(Timeframe timeframe)
{
    switch (timeframe)
    {
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

CipherEnum::Timeframe CipherEnum::toTimeframe(const std::string &timeframe_str)
{
    static const std::unordered_map< std::string, Timeframe > timeframe_map = {{"1m", Timeframe::MINUTE_1},
                                                                               {"3m", Timeframe::MINUTE_3},
                                                                               {"5m", Timeframe::MINUTE_5},
                                                                               {"15m", Timeframe::MINUTE_15},
                                                                               {"30m", Timeframe::MINUTE_30},
                                                                               {"45m", Timeframe::MINUTE_45},
                                                                               {"1h", Timeframe::HOUR_1},
                                                                               {"2h", Timeframe::HOUR_2},
                                                                               {"3h", Timeframe::HOUR_3},
                                                                               {"4h", Timeframe::HOUR_4},
                                                                               {"6h", Timeframe::HOUR_6},
                                                                               {"8h", Timeframe::HOUR_8},
                                                                               {"12h", Timeframe::HOUR_12},
                                                                               {"1D", Timeframe::DAY_1},
                                                                               {"3D", Timeframe::DAY_3},
                                                                               {"1W", Timeframe::WEEK_1},
                                                                               {"1M", Timeframe::MONTH_1}};

    auto it = timeframe_map.find(timeframe_str);
    if (it == timeframe_map.end())
    {
        throw std::invalid_argument("Invalid timeframe: " + timeframe_str);
    }
    return it->second;
}

const std::string CipherEnum::toString(Color color)
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
            return "UNKNOWN";
    }
}

const std::string CipherEnum::toString(OrderType orderType)
{
    switch (orderType)
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
            return "UNKNOWN";
    }
}

CipherEnum::OrderType CipherEnum::toOrderType(const std::string &order_type_str)
{
    if (order_type_str == "MARKET")
    {
        return OrderType::MARKET;
    }
    else if (order_type_str == "LIMIT")
    {
        return OrderType::LIMIT;
    }
    else if (order_type_str == "STOP")
    {
        return OrderType::STOP;
    }
    else if (order_type_str == "FOK")
    {
        return OrderType::FOK;
    }
    else if (order_type_str == "STOP LIMIT")
    {
        return OrderType::STOP_LIMIT;
    }
    throw std::invalid_argument("Invalid OrderType string: " + order_type_str);
}

const std::string CipherEnum::toString(Exchange exchange)
{
    switch (exchange)
    {
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

CipherEnum::Exchange CipherEnum::toExchange(const std::string &exchange_str)
{
    static const std::unordered_map< std::string, Exchange > exchange_map = {
        {"Sandbox", Exchange::SANDBOX},
        {"Coinbase Spot", Exchange::COINBASE_SPOT},
        {"Bitfinex Spot", Exchange::BITFINEX_SPOT},
        {"Binance Spot", Exchange::BINANCE_SPOT},
        {"Binance US Spot", Exchange::BINANCE_US_SPOT},
        {"Binance Perpetual Futures", Exchange::BINANCE_PERPETUAL_FUTURES},
        {"Binance Perpetual Futures Testnet", Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET},
        {"Bybit USDT Perpetual", Exchange::BYBIT_USDT_PERPETUAL},
        {"Bybit USDC Perpetual", Exchange::BYBIT_USDC_PERPETUAL},
        {"Bybit USDT Perpetual Testnet", Exchange::BYBIT_USDT_PERPETUAL_TESTNET},
        {"Bybit USDC Perpetual Testnet", Exchange::BYBIT_USDC_PERPETUAL_TESTNET},
        {"Bybit Spot", Exchange::BYBIT_SPOT},
        {"Bybit Spot Testnet", Exchange::BYBIT_SPOT_TESTNET},
        {"FTX Perpetual Futures", Exchange::FTX_PERPETUAL_FUTURES},
        {"FTX Spot", Exchange::FTX_SPOT},
        {"FTX US Spot", Exchange::FTX_US_SPOT},
        {"Bitget Spot", Exchange::BITGET_SPOT},
        {"Bitget USDT Perpetual", Exchange::BITGET_USDT_PERPETUAL},
        {"Bitget USDT Perpetual Testnet", Exchange::BITGET_USDT_PERPETUAL_TESTNET},
        {"Dydx Perpetual", Exchange::DYDX_PERPETUAL},
        {"Dydx Perpetual Testnet", Exchange::DYDX_PERPETUAL_TESTNET},
        {"Apex Pro Perpetual Testnet", Exchange::APEX_PRO_PERPETUAL_TESTNET},
        {"Apex Pro Perpetual", Exchange::APEX_PRO_PERPETUAL},
        {"Apex Omni Perpetual Testnet", Exchange::APEX_OMNI_PERPETUAL_TESTNET},
        {"Apex Omni Perpetual", Exchange::APEX_OMNI_PERPETUAL},
        {"Gate USDT Perpetual", Exchange::GATE_USDT_PERPETUAL},
        {"Gate Spot", Exchange::GATE_SPOT}};

    auto it = exchange_map.find(exchange_str);
    if (it == exchange_map.end())
    {
        throw std::invalid_argument("Invalid exchange: " + exchange_str);
    }
    return it->second;
}

const std::string CipherEnum::toString(MigrationAction action)
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
            return "UNKNOWN";
    }
}

const std::string CipherEnum::toString(OrderSubmittedVia method)
{
    switch (method)
    {
        case OrderSubmittedVia::STOP_LOSS:
            return "stop-loss";
        case OrderSubmittedVia::TAKE_PROFIT:
            return "take-profit";
        default:
            return "UNKNOWN";
    }
}

CipherEnum::TradeType CipherEnum::toTradeType(const std::string &trade_type_str)
{
    if (trade_type_str == SHORT)
    {
        return TradeType::SHORT;
    }
    else if (trade_type_str == LONG)
    {
        return TradeType::LONG;
    }
    else
    {
        throw std::invalid_argument("Invalid trade type: " + trade_type_str);
    }
}
