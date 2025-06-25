#include "Ticker.hpp"
#include "Helper.hpp"
#include "Route.hpp"


void ct::ticker::TickersState::init()
{
    auto routes = route::Router::getInstance().formattedRoutes();
    for (const auto& route : routes)
    {
        auto exchange = route["exchange_name"].get< enums::ExchangeName >();
        auto symbol   = route["symbol"].get< std::string >();
        auto key      = helper::makeKey(exchange, symbol);

        // Create a dynamic array with 60 rows and 5 columns, dropping at 120
        std::array< size_t, 2 > shape = {60, 5};
        storage_.at(key)              = std::make_shared< datastructure::DynamicBlazeArray< double > >(shape, 120);
    }
}

void ct::ticker::TickersState::addTicker(const blaze::DynamicVector< double, blaze::rowVector >& ticker,
                                         const enums::ExchangeName& exchange_name,
                                         const std::string& symbol)
{
    std::string key = helper::makeKey(exchange_name, symbol);

    // Only process once per second
    if (storage_.at(key)->size() == 0 || (helper::nowToTimestamp() - (*storage_.at(key))[-1][0]) >= 1000)
    {
        storage_.at(key)->append(ticker);
    }
}

blaze::DynamicMatrix< double > ct::ticker::TickersState::getTickers(const enums::ExchangeName& exchange_name,
                                                                    const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);

    return storage_.at(key)->rows(0, storage_.at(key)->size());
}

auto ct::ticker::TickersState::getCurrentTicker(const enums::ExchangeName& exchange_name,
                                                const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);

    return storage_.at(key)->row(-1);
}

auto ct::ticker::TickersState::getPastTicker(const enums::ExchangeName& exchange_name,
                                             const std::string& symbol,
                                             int numberOfTickersAgo) const
{
    if (numberOfTickersAgo > 120)
    {
        throw std::invalid_argument("Max accepted value for numberOfTickersAgo is 120");
    }

    numberOfTickersAgo = std::abs(numberOfTickersAgo);
    std::string key    = helper::makeKey(exchange_name, symbol);

    return storage_.at(key)->row(-1 - numberOfTickersAgo);
}
