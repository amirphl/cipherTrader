#include "Info.hpp"
#include <sstream>
#include <unordered_map>
#include <vector>
#include "Enum.hpp"

const std::string ct::info::CIPHER_TRADER_API_URL{"https://api.ciphertrader.trade/api/v1"};

const std::string ct::info::CIPHER_TRADER_WEBSITE_URL{"https://ciphertrader.trade"};

const std::vector< ct::enums::Timeframe > ct::info::BYBIT_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_3,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::BINANCE_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_3,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_8,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::COINBASE_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::APEX_PRO_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::GATE_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_8,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
    ct::enums::Timeframe::WEEK_1,
};

const std::vector< ct::enums::Timeframe > ct::info::FTX_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_3,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::BITGET_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::info::DYDX_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::DAY_1,
};

const std::unordered_map< ct::enums::Exchange, std::unordered_map< std::string, ct::info::ExchangeInfo > >
    ct::info::EXCHANGE_INFO{
        {ct::enums::Exchange::BYBIT_USDC_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_USDT_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"settlement_currency", "USDT"},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BYBIT_USDT_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_USDT_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"settlement_currency", "USDT"},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BYBIT_USDC_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_USDC_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"settlement_currency", "USDC"},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BYBIT_USDC_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_USDC_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.00055},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"settlement_currency", "USDC"},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BYBIT_SPOT,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_SPOT)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.001},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BYBIT_SPOT_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::BYBIT_SPOT_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bybit"},
             {"fee", 0.001},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BYBIT_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BITFINEX_SPOT,
         {
             {"name", toString(ct::enums::Exchange::BITFINEX_SPOT)},
             {"url", "https://bitfinex.com"},
             {"fee", 0.002},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes",
              std::vector< ct::enums::Timeframe >{
                  ct::enums::Timeframe::MINUTE_1,
                  ct::enums::Timeframe::MINUTE_5,
                  ct::enums::Timeframe::MINUTE_15,
                  ct::enums::Timeframe::MINUTE_30,
                  ct::enums::Timeframe::HOUR_1,
                  ct::enums::Timeframe::HOUR_3,
                  ct::enums::Timeframe::HOUR_6,
                  ct::enums::Timeframe::HOUR_12,
                  ct::enums::Timeframe::DAY_1,
              }},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BINANCE_SPOT,
         {
             {"name", toString(ct::enums::Exchange::BINANCE_SPOT)},
             {"url", "https://binance.com"},
             {"fee", 0.001},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BINANCE_US_SPOT,
         {
             {"name", toString(ct::enums::Exchange::BINANCE_US_SPOT)},
             {"url", "https://binance.us"},
             {"fee", 0.001},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BINANCE_PERPETUAL_FUTURES,
         {
             {"name", toString(ct::enums::Exchange::BINANCE_PERPETUAL_FUTURES)},
             {"url", "https://binance.com"},
             {"fee", 0.0004},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::BINANCE_PERPETUAL_FUTURES_TESTNET)},
             {"url", "https://binance.com"},
             {"fee", 0.0004},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BINANCE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::COINBASE_SPOT,
         {
             {"name", toString(ct::enums::Exchange::COINBASE_SPOT)},
             {"url", "https://www.coinbase.com/advanced-trade/spot/BTC-USD"},
             {"fee", 0.0003},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", COINBASE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::APEX_PRO_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::APEX_PRO_PERPETUAL_TESTNET)},
             {"url", "https://testnet.pro.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "free"},
         }},
        {ct::enums::Exchange::APEX_PRO_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::APEX_PRO_PERPETUAL)},
             {"url", "https://pro.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::APEX_OMNI_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::APEX_OMNI_PERPETUAL_TESTNET)},
             {"url", "https://testnet.omni.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "free"},
         }},
        {ct::enums::Exchange::APEX_OMNI_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::APEX_OMNI_PERPETUAL)},
             {"url", "https://omni.apex.exchange/trade/BTCUSD"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", APEX_PRO_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::GATE_USDT_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::GATE_USDT_PERPETUAL)},
             {"url", "https://jesse.trade/gate"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", GATE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", true}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::GATE_SPOT,
         {
             {"name", toString(ct::enums::Exchange::GATE_SPOT)},
             {"url", "https://jesse.trade/gate"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", GATE_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", true}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::FTX_PERPETUAL_FUTURES,
         {
             {"name", toString(ct::enums::Exchange::FTX_PERPETUAL_FUTURES)},
             {"url", "https://ftx.com/markets/future"},
             {"fee", 0.0006},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::FTX_SPOT,
         {
             {"name", toString(ct::enums::Exchange::FTX_SPOT)},
             {"url", "https://ftx.com/markets/spot"},
             {"fee", 0.0007},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::FTX_US_SPOT,
         {
             {"name", toString(ct::enums::Exchange::FTX_US_SPOT)},
             {"url", "https://ftx.us"},
             {"fee", 0.002},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", FTX_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BITGET_USDT_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::BITGET_USDT_PERPETUAL_TESTNET)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BITGET_USDT_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::BITGET_USDT_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::BITGET_SPOT,
         {
             {"name", toString(ct::enums::Exchange::BITGET_SPOT)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/bitget"},
             {"fee", 0.0006},
             {"type", ct::enums::ExchangeType::SPOT},
             {"supported_leverage_modes",
              std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS,
                                                     ct::enums::LeverageMode::ISOLATED}},
             {"supported_timeframes", BITGET_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::DYDX_PERPETUAL,
         {
             {"name", toString(ct::enums::Exchange::DYDX_PERPETUAL)},
             {"url", CIPHER_TRADER_WEBSITE_URL + "/dydx"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", DYDX_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},
        {ct::enums::Exchange::DYDX_PERPETUAL_TESTNET,
         {
             {"name", toString(ct::enums::Exchange::DYDX_PERPETUAL_TESTNET)},
             {"url", "https://trade.stage.dydx.exchange/trade/ETH-USD"},
             {"fee", 0.0005},
             {"type", ct::enums::ExchangeType::FUTURES},
             {"supported_leverage_modes", std::vector< ct::enums::LeverageMode >{ct::enums::LeverageMode::CROSS}},
             {"supported_timeframes", DYDX_TIMEFRAMES},
             {"modes", std::unordered_map< std::string, bool >{{"backtesting", false}, {"live_trading", false}}},
             {"required_live_plan", "premium"},
         }},

    };

template < typename T >
std::string ct::info::vectorToString(const std::vector< T > &vec)
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
std::string ct::info::unorderedMapToString(const std::unordered_map< T, bool > &map)
{
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto &[key, value] : map)
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

std::string ct::info::toString(const ct::info::ExchangeInfo &var)
{
    return std::visit(
        [](const auto &value) -> std::string
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
            else if constexpr (std::is_same_v< T, std::vector< ct::enums::Timeframe > >)
                return "[ct::enums::Timeframe objects]"; // Customize this if needed
            else if constexpr (std::is_same_v< T, std::unordered_map< std::string, bool > >)
                return unorderedMapToString(value);
            else
                return "Unknown Type";
        },
        var);
}

std::vector< std::string > ct::info::getExchangesByMode(const std::string &mode)
{
    std::vector< std::string > exchanges;

    for (const auto &[exchange, info] : EXCHANGE_INFO)
    {
        auto it = info.find("modes");
        if (it != info.end())
        {
            const auto &modes = std::get< std::unordered_map< std::string, bool > >(it->second);
            if (modes.at(mode))
            {
                exchanges.push_back(toString(exchange));
            }
        }
    }

    std::sort(exchanges.begin(), exchanges.end());
    return exchanges;
}

const std::vector< std::string > ct::info::BACKTESTING_EXCHANGES = ct::info::getExchangesByMode("backtesting");

const std::vector< std::string > ct::info::LIVE_TRADING_EXCHANGES = ct::info::getExchangesByMode("live_trading");

const std::vector< ct::enums::Timeframe > ct::info::CIPHER_TRADER_SUPPORTED_TIMEFRAMES{
    ct::enums::Timeframe::MINUTE_1,
    ct::enums::Timeframe::MINUTE_3,
    ct::enums::Timeframe::MINUTE_5,
    ct::enums::Timeframe::MINUTE_15,
    ct::enums::Timeframe::MINUTE_30,
    ct::enums::Timeframe::MINUTE_45,
    ct::enums::Timeframe::HOUR_1,
    ct::enums::Timeframe::HOUR_2,
    ct::enums::Timeframe::HOUR_3,
    ct::enums::Timeframe::HOUR_4,
    ct::enums::Timeframe::HOUR_6,
    ct::enums::Timeframe::HOUR_8,
    ct::enums::Timeframe::HOUR_12,
    ct::enums::Timeframe::DAY_1,
};
