#ifndef CIPHER_TIMEFRAME_HPP
#define CIPHER_TIMEFRAME_HPP

namespace ct
{
namespace timeframe
{

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

extern const std::vector< Timeframe > BYBIT_TIMEFRAMES;
extern const std::vector< Timeframe > BINANCE_TIMEFRAMES;
extern const std::vector< Timeframe > COINBASE_TIMEFRAMES;

extern const std::vector< Timeframe > APEX_PRO_TIMEFRAMES;
extern const std::vector< Timeframe > GATE_TIMEFRAMES;
extern const std::vector< Timeframe > FTX_TIMEFRAMES;
extern const std::vector< Timeframe > BITGET_TIMEFRAMES;
extern const std::vector< Timeframe > DYDX_TIMEFRAMES;

// TODO:Move to utils.
template < typename T >
std::string vectorToString(const std::vector< T > &vec);

// TODO:Move to utils.
template < typename T >
std::string unorderedMapToString(const std::unordered_map< T, bool > &map);

extern const std::vector< Timeframe > CIPHER_TRADER_SUPPORTED_TIMEFRAMES;

extern const std::string toString(Timeframe timeframe);

extern Timeframe toTimeframe(const std::string &timeframeStr);

inline std::ostream &operator<<(std::ostream &os, const Timeframe &timeframe)
{
    return os << toString(timeframe);
}

int64_t convertTimeframeToOneMinutes(const Timeframe &timeframe);

Timeframe maxTimeframe(const std::vector< Timeframe > &timeframes);

} // namespace timeframe
} // namespace ct

namespace std
{
template <>
struct hash< ct::timeframe::Timeframe >
{
    std::size_t operator()(const ct::timeframe::Timeframe &e) const
    {
        return std::hash< std::underlying_type_t< ct::timeframe::Timeframe > >{}(
            static_cast< std::underlying_type_t< ct::timeframe::Timeframe > >(e));
    }
};
} // namespace std

#endif
