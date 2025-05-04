#include "Timeframe.hpp"
#include "Enum.hpp"

const std::vector< ct::enums::Timeframe > ct::timeframe::BYBIT_TIMEFRAMES{
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

const std::vector< ct::enums::Timeframe > ct::timeframe::BINANCE_TIMEFRAMES{
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

const std::vector< ct::enums::Timeframe > ct::timeframe::COINBASE_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_6,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::timeframe::APEX_PRO_TIMEFRAMES{
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

const std::vector< ct::enums::Timeframe > ct::timeframe::GATE_TIMEFRAMES{
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

const std::vector< ct::enums::Timeframe > ct::timeframe::FTX_TIMEFRAMES{
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

const std::vector< ct::enums::Timeframe > ct::timeframe::BITGET_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::HOUR_12,
    enums::Timeframe::DAY_1,
};

const std::vector< ct::enums::Timeframe > ct::timeframe::DYDX_TIMEFRAMES{
    enums::Timeframe::MINUTE_1,
    enums::Timeframe::MINUTE_5,
    enums::Timeframe::MINUTE_15,
    enums::Timeframe::MINUTE_30,
    enums::Timeframe::HOUR_1,
    enums::Timeframe::HOUR_4,
    enums::Timeframe::DAY_1,
};

template < typename T >
std::string ct::timeframe::vectorToString(const std::vector< T >& vec)
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
std::string ct::timeframe::unorderedMapToString(const std::unordered_map< T, bool >& map)
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

const std::vector< ct::enums::Timeframe > ct::timeframe::CIPHER_TRADER_SUPPORTED_TIMEFRAMES{
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
