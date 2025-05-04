#include "Enum.hpp"

const std::string ct::enums::toString(OrderSide side)
{
    return (side == OrderSide::BUY) ? "buy" : "sell";
}

ct::enums::OrderSide ct::enums::toOrderSide(const std::string &orderSideStr)
{
    if (orderSideStr == "buy")
    {
        return OrderSide::BUY;
    }
    else if (orderSideStr == "sell")
    {
        return OrderSide::SELL;
    }
    throw std::invalid_argument("Invalid OrderSide string: " + orderSideStr);
}

const std::string ct::enums::toString(PositionType positionType)
{
    if (positionType == PositionType::LONG)
    {
        return "long";
    }
    else if (positionType == PositionType::SHORT)
    {
        return "short";
    }
    else if (positionType == PositionType::CLOSE)
    {
        return "close";
    }
    throw std::invalid_argument("Invalid Position");
}

const std::string ct::enums::toString(OrderStatus orderStatus)
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

ct::enums::OrderStatus ct::enums::toOrderStatus(const std::string &statusStr)
{
    if (statusStr == "active")
    {
        return OrderStatus::ACTIVE;
    }
    else if (statusStr == "canceled")
    {
        return OrderStatus::CANCELED;
    }
    else if (statusStr == "executed")
    {
        return OrderStatus::EXECUTED;
    }
    else if (statusStr == "partially_filled")
    {
        return OrderStatus::PARTIALLY_FILLED;
    }
    else if (statusStr == "queued")
    {
        return OrderStatus::QUEUED;
    }
    else if (statusStr == "liquidated")
    {
        return OrderStatus::LIQUIDATED;
    }
    else if (statusStr == "rejected")
    {
        return OrderStatus::REJECTED;
    }
    else
    {
        throw std::invalid_argument("Invalid order status: " + statusStr);
    }
}

const std::string ct::enums::toString(Timeframe timeframe)
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

ct::enums::Timeframe ct::enums::toTimeframe(const std::string &timeframeStr)
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

    auto it = timeframe_map.find(timeframeStr);
    if (it == timeframe_map.end())
    {
        throw std::invalid_argument("Invalid timeframe: " + timeframeStr);
    }
    return it->second;
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
            return "UNKNOWN";
    }
}

const std::string ct::enums::toString(OrderType orderType)
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

ct::enums::OrderType ct::enums::toOrderType(const std::string &orderTypeStr)
{
    if (orderTypeStr == "MARKET")
    {
        return OrderType::MARKET;
    }
    else if (orderTypeStr == "LIMIT")
    {
        return OrderType::LIMIT;
    }
    else if (orderTypeStr == "STOP")
    {
        return OrderType::STOP;
    }
    else if (orderTypeStr == "FOK")
    {
        return OrderType::FOK;
    }
    else if (orderTypeStr == "STOP LIMIT")
    {
        return OrderType::STOP_LIMIT;
    }
    throw std::invalid_argument("Invalid OrderType string: " + orderTypeStr);
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
            return "UNKNOWN";
    }
}

ct::enums::ExchangeName ct::enums::toExchangeName(const std::string &exchange_name_str)
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

    auto it = exchange_name_map.find(exchange_name_str);
    if (it == exchange_name_map.end())
    {
        throw std::invalid_argument("Invalid exchange: " + exchange_name_str);
    }
    return it->second;
}

const std::string ct::enums::toString(ct::enums::ExchangeType exchangeType)
{
    switch (exchangeType)
    {
        case ExchangeType::SPOT:
            return "SPOT";
        case ExchangeType::FUTURES:
            return "FUTURES";
        default:
            throw std::invalid_argument("Unknown ExchangeType");
    }
}

ct::enums::ExchangeType ct::enums::toExchangeType(const std::string &exchangeTypeStr)
{
    std::string upperStr = exchangeTypeStr;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    if (upperStr == "SPOT")
        return ExchangeType::SPOT;
    else if (upperStr == "FUTURES")
        return ExchangeType::FUTURES;
    else
        throw std::invalid_argument("Invalid exchange type string: " + exchangeTypeStr);
}

const std::string ct::enums::toString(ct::enums::LeverageMode leverageMode)
{
    switch (leverageMode)
    {
        case LeverageMode::CROSS:
            return "CROSS";
        case LeverageMode::ISOLATED:
            return "ISOLATED";
        default:
            throw std::invalid_argument("Unknown LeverageMode");
    }
}

ct::enums::LeverageMode ct::enums::toLeverageMode(const std::string &leverageModeStr)
{
    std::string upperStr = leverageModeStr;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    if (upperStr == "CROSS")
        return LeverageMode::CROSS;
    else if (upperStr == "ISOLATED")
        return LeverageMode::ISOLATED;
    else
        throw std::invalid_argument("Invalid leverage mode string: " + leverageModeStr);
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
            return "UNKNOWN";
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
            return "UNKNOWN";
    }
}

ct::enums::PositionType ct::enums::toPositionType(const std::string &positionTypeStr)
{
    if (positionTypeStr == "short")
    {
        return PositionType::SHORT;
    }
    else if (positionTypeStr == "long")
    {
        return PositionType::LONG;
    }
    else if (positionTypeStr == "close")
    {
        return PositionType::CLOSE;
    }
    else
    {
        throw std::invalid_argument("Invalid position type: " + positionTypeStr);
    }
}
