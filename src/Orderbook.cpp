#include "Orderbook.hpp"
#include "Helper.hpp"
#include "LimitOrderbook.hpp"
#include "Route.hpp"

namespace ct
{
namespace orderbook
{

void OrderbooksState::init()
{
    auto routes = ct::route::Router::getInstance().formattedRoutes();
    for (const auto& route : routes)
    {
        auto exchangeName = route["exchange_name"].get< enums::ExchangeName >();
        auto symbol       = route["symbol"].get< std::string >();
        auto key          = helper::makeKey(exchangeName, symbol);

        // Initialize temp storage
        temp_storage_.at(key) = TempOrderbookData{};

        // Create a dynamic array with shape [60, 2, 50, 2] and drop at 60
        // This represents 60 timeframes, 2 sides (ask/bid), 50 levels, 2 values (price/qty)
        std::array< size_t, 2 > shape{60, 2};
        storage_.at(key) =
            std::make_shared< datastructure::DynamicBlazeArray< lob::LimitOrderbook< lob::R_, lob::C_ > > >(shape, 60);
    }
}

lob::LimitOrderbook< lob::R_, lob::C_ > OrderbooksState::fixLen(const std::vector< std::array< double, 2 > >& arr,
                                                                size_t target_len) const
{
    auto nan = std::numeric_limits< double >::quiet_NaN();
    lob::LimitOrderbook< lob::R_, lob::C_ > result{};
    for (auto& row : result.data)
    {
        row.fill(nan);
    }

    // Copy available data
    for (size_t i = 0; i < std::min(arr.size(), target_len); ++i)
    {
        result[i][0] = arr[i][0];
        result[i][1] = arr[i][1];
    }

    return result;
}

blaze::StaticVector< lob::LimitOrderbook< lob::R_, lob::C_ >, 2UL, blaze::rowVector > OrderbooksState::formatOrderbook(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);

    // Trim prices
    auto asks = trim(temp_storage_.at(key).asks_, true);
    auto bids = trim(temp_storage_.at(key).bids_, false);

    // Fill empty values with NaN
    auto formattedAsks = fixLen(asks, lob::R_);
    auto formattedBids = fixLen(bids, lob::R_);

    return {formattedAsks, formattedBids};
}

void OrderbooksState::addOrderbook(const enums::ExchangeName& exchange_name,
                                   const std::string& symbol,
                                   const std::vector< std::array< double, 2 > >& asks,
                                   const std::vector< std::array< double, 2 > >& bids)
{
    std::string key             = helper::makeKey(exchange_name, symbol);
    temp_storage_.at(key).asks_ = asks;
    temp_storage_.at(key).bids_ = bids;

    // Generate new formatted orderbook if it is either the first time,
    // or it has passed 1000 milliseconds since the last time
    int64_t currentTimestamp = helper::nowToTimestamp();
    if (temp_storage_.at(key).last_updated_timestamp_ == 0 ||
        currentTimestamp - temp_storage_.at(key).last_updated_timestamp_ >= 1000)
    {
        temp_storage_.at(key).last_updated_timestamp_ = currentTimestamp;

        auto formattedOrderbook = formatOrderbook(exchange_name, symbol);
        storage_.at(key)->append(formattedOrderbook);
    }
}

auto OrderbooksState::getCurrentOrderbook(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);
    return storage_.at(key)->row(-1);
}

lob::LimitOrderbook< lob::R_, lob::C_ > OrderbooksState::getCurrentAsks(const enums::ExchangeName& exchange_name,
                                                                        const std::string& symbol) const
{
    return getCurrentOrderbook(exchange_name, symbol)[0];
}

blaze::StaticVector< double, 2UL > OrderbooksState::getBestAsk(const enums::ExchangeName& exchange_name,
                                                               const std::string& symbol) const
{
    auto currentAsks = getCurrentAsks(exchange_name, symbol);
    auto price       = currentAsks[0][0];
    auto qty         = currentAsks[1][0];
    return {price, qty};
}

lob::LimitOrderbook< lob::R_, lob::C_ > OrderbooksState::getCurrentBids(const enums::ExchangeName& exchange_name,
                                                                        const std::string& symbol) const
{
    return getCurrentOrderbook(exchange_name, symbol)[1];
}

blaze::StaticVector< double, 2UL > OrderbooksState::getBestBid(const enums::ExchangeName& exchange_name,
                                                               const std::string& symbol) const
{
    auto currentBids = getCurrentBids(exchange_name, symbol);
    auto price       = currentBids[0][0];
    auto qty         = currentBids[1][0];
    return {price, qty};
}

datastructure::DynamicBlazeArray< lob::LimitOrderbook< lob::R_, lob::C_ > > OrderbooksState::getOrderbooks(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);
    return *storage_.at(key);
}

double OrderbooksState::trim(double price, bool ascending, double unit)
{
    if (unit <= 0)
    {
        throw std::invalid_argument("Unit must be positive");
    }

    double trimmed;
    if (ascending)
    {
        trimmed = std::ceil(price / unit) * unit;
        if (std::log10(unit) < 0)
        {
            trimmed = std::round(trimmed * std::pow(10.0, std::abs(std::log10(unit)))) /
                      std::pow(10.0, std::abs(std::log10(unit)));
        }
        return (trimmed == price + unit) ? price : trimmed;
    }
    else
    {
        trimmed = std::ceil(price / unit) * unit - unit;
        if (std::log10(unit) < 0)
        {
            trimmed = std::round(trimmed * std::pow(10.0, std::abs(std::log10(unit)))) /
                      std::pow(10.0, std::abs(std::log10(unit)));
        }
        return (trimmed == price - unit) ? price : trimmed;
    }
}

std::vector< std::array< double, 2 > > OrderbooksState::trim(const std::vector< std::array< double, 2 > >& arr,
                                                             bool ascending,
                                                             size_t limit_len) const
{
    if (arr.empty())
    {
        return {};
    }

    double first_price = arr[0][0];
    double unit;

    // Determine the precision unit based on the first price
    if (first_price < 0.1)
    {
        unit = 1e-5;
    }
    else if (first_price < 1)
    {
        unit = 1e-4;
    }
    else if (first_price < 10)
    {
        unit = 1e-3;
    }
    else if (first_price < 100)
    {
        unit = 1e-2;
    }
    else if (first_price < 1000)
    {
        unit = 1e-1;
    }
    else if (first_price < 10000)
    {
        unit = 1;
    }
    else
    {
        unit = 10;
    }

    double trimmed_price = trim(first_price, ascending, unit);

    double temp_qty = 0;
    std::vector< std::array< double, 2 > > trimmed_arr;

    for (const auto& a : arr)
    {
        if (trimmed_arr.size() == limit_len)
        {
            break;
        }

        if ((ascending && a[0] > trimmed_price) || (!ascending && a[0] < trimmed_price))
        {
            trimmed_arr.push_back({trimmed_price, temp_qty});
            temp_qty      = a[1];
            trimmed_price = trim(a[0], ascending, unit);
        }

        // Accumulate quantity for the current trimmed price
        temp_qty += a[1];
    }

    return trimmed_arr;
}

} // namespace orderbook
} // namespace ct
