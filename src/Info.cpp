#include "Info.hpp"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Enum.hpp"
#include "Exchange.hpp"

const std::string ct::info::CIPHER_TRADER_API_URL{"https://api.ciphertrader.trade/api/v1"};

const std::string ct::info::CIPHER_TRADER_WEBSITE_URL{"https://ciphertrader.trade"};

const std::vector< ct::enums::Timeframe > ct::info::BYBIT_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_3,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::BINANCE_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_3,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_8,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::COINBASE_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::APEX_PRO_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::GATE_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_8,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
    enums::Timeframe::WEEK_1,
};

const std::vector< ct::enums::Timeframe > ct::info::FTX_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_3,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::BITGET_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::DYDX_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::DAY_1,
};

const std::unordered_map< ct::enums::ExchangeName, ct::exchange::ExchangeData > ct::info::EXCHANGES_DATA{
    {enums::ExchangeName::BYBIT_USDT_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_USDT_PERPETUAL),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.00055,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium",
                            "USDT")},
    {enums::ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.00055,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium",
                            "USDT")},
    {enums::ExchangeName::BYBIT_USDC_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_USDC_PERPETUAL),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.00055,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium",
                            "USDC")},
    {enums::ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.00055,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium",
                            "USDC")},
    {enums::ExchangeName::BYBIT_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_SPOT),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.001,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::BYBIT_SPOT_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::BYBIT_SPOT_TESTNET),
                            CIPHER_TRADER_WEBSITE_URL + "/bybit",
                            0.001,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BYBIT_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::BITFINEX_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::BITFINEX_SPOT),
                            "https://bitfinex.com",
                            0.002,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS},
                            {
                                enums::Timeframe::MINUTE_1,
                                enums::Timeframe::MINUTE_5,
                                enums::Timeframe::MINUTE_15,
                                enums::Timeframe::MINUTE_30,
                                enums::Timeframe::HOUR_1,
                                enums::Timeframe::HOUR_3,
                                enums::Timeframe::HOUR_6,
                                enums::Timeframe::HOUR_12,
                                enums::Timeframe::DAY_1,
                            },
                            {{"backtesting", true}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::BINANCE_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::BINANCE_SPOT),
                            "https://binance.com",
                            0.001,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BINANCE_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::BINANCE_US_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::BINANCE_US_SPOT),
                            "https://binance.us",
                            0.001,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BINANCE_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::BINANCE_PERPETUAL_FUTURES,
     exchange::ExchangeData(toString(enums::ExchangeName::BINANCE_PERPETUAL_FUTURES),
                            "https://binance.com",
                            0.0004,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BINANCE_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET),
                            "https://binance.com",
                            0.0004,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BINANCE_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::COINBASE_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::COINBASE_SPOT),
                            "https://www.coinbase.com/advanced-trade/spot/BTC-USD",
                            0.0003,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            COINBASE_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::APEX_PRO_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::APEX_PRO_PERPETUAL_TESTNET),
                            "https://testnet.pro.apex.exchange/trade/BTCUSD",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            APEX_PRO_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "free")},
    {enums::ExchangeName::APEX_PRO_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::APEX_PRO_PERPETUAL),
                            "https://pro.apex.exchange/trade/BTCUSD",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            APEX_PRO_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::APEX_OMNI_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::APEX_OMNI_PERPETUAL_TESTNET),
                            "https://testnet.omni.apex.exchange/trade/BTCUSD",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            APEX_PRO_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "free")},
    {enums::ExchangeName::APEX_OMNI_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::APEX_OMNI_PERPETUAL),
                            "https://omni.apex.exchange/trade/BTCUSD",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            APEX_PRO_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::GATE_USDT_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::GATE_USDT_PERPETUAL),
                            "https://jesse.trade/gate",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            GATE_TIMEFRAMES,
                            {{"backtesting", true}, {"live_trading", true}},
                            "premium",
                            "USDT")},
    {enums::ExchangeName::GATE_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::GATE_SPOT),
                            "https://jesse.trade/gate",
                            0.0005,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            GATE_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", true}},
                            "premium")},
    {enums::ExchangeName::FTX_PERPETUAL_FUTURES,
     exchange::ExchangeData(toString(enums::ExchangeName::FTX_PERPETUAL_FUTURES),
                            "https://ftx.com/markets/future",
                            0.0006,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            FTX_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::FTX_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::FTX_SPOT),
                            "https://ftx.com/markets/spot",
                            0.0007,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS},
                            FTX_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::FTX_US_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::FTX_US_SPOT),
                            "https://ftx.us",
                            0.002,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS},
                            FTX_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::BITGET_USDT_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::BITGET_USDT_PERPETUAL_TESTNET),
                            CIPHER_TRADER_WEBSITE_URL + "/bitget",
                            0.0006,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BITGET_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium",
                            "USDT")},
    {enums::ExchangeName::BITGET_USDT_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::BITGET_USDT_PERPETUAL),
                            CIPHER_TRADER_WEBSITE_URL + "/bitget",
                            0.0006,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BITGET_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium",
                            "USDT")},
    {enums::ExchangeName::BITGET_SPOT,
     exchange::ExchangeData(toString(enums::ExchangeName::BITGET_SPOT),
                            CIPHER_TRADER_WEBSITE_URL + "/bitget",
                            0.0006,
                            enums::ExchangeType::SPOT,
                            {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                            BITGET_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::DYDX_PERPETUAL,
     exchange::ExchangeData(toString(enums::ExchangeName::DYDX_PERPETUAL),
                            CIPHER_TRADER_WEBSITE_URL + "/dydx",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            DYDX_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")},
    {enums::ExchangeName::DYDX_PERPETUAL_TESTNET,
     exchange::ExchangeData(toString(enums::ExchangeName::DYDX_PERPETUAL_TESTNET),
                            "https://trade.stage.dydx.exchange/trade/ETH-USD",
                            0.0005,
                            enums::ExchangeType::FUTURES,
                            {enums::LeverageMode::CROSS},
                            DYDX_TIMEFRAMES,
                            {{"backtesting", false}, {"live_trading", false}},
                            "premium")}};

template < typename T >
std::string ct::info::vectorToString(const std::vector< T >& vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        if constexpr (std::is_same_v< T, std::string >)
            oss << "\"" << vec[i] << "\"";
        else
            oss << vec[i];
        if (i != vec.size() - 1)
            oss << ", ";
    }
    oss << "]";
    return oss.str();
}

template < typename T >
std::string ct::info::unorderedMapToString(const std::unordered_map< T, bool >& map)
{
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : map)
    {
        if (!first)
            oss << ", ";

        if constexpr (std::is_same_v< T, std::string >)
            oss << "\"" << key << "\": " << (value ? "true" : "false");
        else
            oss << key << ": " << (value ? "true" : "false");

        first = false;
    }
    oss << "}";
    return oss.str();
}

std::string ct::info::toString(const ct::info::ExchangeInfo& var)
{
    return std::visit(
        [](const auto& value) -> std::string
        {
            using T = std::decay_t< decltype(value) >;

            if constexpr (std::is_same_v< T, std::string >)
                return value;
            else if constexpr (std::is_same_v< T, double >)
                return std::to_string(value);
            else if constexpr (std::is_same_v< T, bool >)
                return value ? "true" : "false";
            else if constexpr (std::is_same_v< T, std::vector< std::string > >)
                return vectorToString(value);
            else if constexpr (std::is_same_v< T, std::vector< enums::Timeframe > >)
                return "[enums::Timeframe objects]"; // Customize this if needed
            else if constexpr (std::is_same_v< T, std::unordered_map< std::string, bool > >)
                return unorderedMapToString(value);
            else
                return "Unknown Type";
        },
        var);
}

std::vector< std::string > ct::info::getExchangesByMode(const std::string& mode)
{
    std::vector< std::string > exchanges;

    for (const auto& [exchange, info] : EXCHANGES_DATA)
    {
        const auto& modes = info.getModes();
        if (modes.at(mode))
        {
            exchanges.push_back(toString(exchange));
        }
    }

    std::sort(exchanges.begin(), exchanges.end());
    return exchanges;
}

const ct::exchange::ExchangeData ct::info::getExchangeData(const ct::enums::ExchangeName& exchange)
{
    return EXCHANGES_DATA.at(exchange);
}

const std::vector< std::string > ct::info::BACKTESTING_EXCHANGES = ct::info::getExchangesByMode("backtesting");

const std::vector< std::string > ct::info::LIVE_TRADING_EXCHANGES = ct::info::getExchangesByMode("live_trading");

const std::vector< ct::enums::Timeframe > ct::info::CIPHER_TRADER_SUPPORTED_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_3,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::MINUTE_45,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_2,
    enums::Timeframe::HOUR_3,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::HOUR_8,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};
