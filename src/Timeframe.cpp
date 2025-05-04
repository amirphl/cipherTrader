#include "Timeframe.hpp"

const std::vector< ct::timeframe::Timeframe > ct::timeframe::BYBIT_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_3,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::BINANCE_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_3,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_8,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::COINBASE_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::APEX_PRO_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::GATE_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_8,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
    timeframe::Timeframe::WEEK_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::FTX_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_3,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::BITGET_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::vector< ct::timeframe::Timeframe > ct::timeframe::DYDX_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::DAY_1,
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

const std::vector< ct::timeframe::Timeframe > ct::timeframe::CIPHER_TRADER_SUPPORTED_TIMEFRAMES{
    timeframe::Timeframe::MINUTE_1,
    timeframe::Timeframe::MINUTE_3,
    timeframe::Timeframe::MINUTE_5,
    timeframe::Timeframe::MINUTE_15,
    timeframe::Timeframe::MINUTE_30,
    timeframe::Timeframe::MINUTE_45,
    timeframe::Timeframe::HOUR_1,
    timeframe::Timeframe::HOUR_2,
    timeframe::Timeframe::HOUR_3,
    timeframe::Timeframe::HOUR_4,
    timeframe::Timeframe::HOUR_6,
    timeframe::Timeframe::HOUR_8,
    timeframe::Timeframe::HOUR_12,
    timeframe::Timeframe::DAY_1,
};

const std::string ct::timeframe::toString(Timeframe timeframe)
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

ct::timeframe::Timeframe ct::timeframe::toTimeframe(const std::string& timeframeStr)
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
