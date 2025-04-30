#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Route.hpp"

#include <blaze/Math.h>

namespace ct
{
namespace ticker
{

class TickerRepository
{
   public:
    TickerRepository()  = default;
    ~TickerRepository() = default;

    void init()
    {
        auto routes = ct::route::Router::getInstance().formattedRoutes();
        for (const auto& route : routes)
        {
            auto exchange = route["exchange_name"].get< enums::ExchangeName >();
            auto symbol   = route["symbol"].get< std::string >();
            auto key      = makeKey(exchange, symbol);

            // Create a dynamic array with 60 rows and 5 columns, dropping at 120
            std::array< size_t, 2 > shape = {60, 5};
            storage_.at(key)              = std::make_shared< datastructure::DynamicBlazeArray< double > >(shape, 120);
        }
    }

    void addTicker(const blaze::DynamicMatrix< double >& ticker,
                   const enums::ExchangeName& exchange_name,
                   const std::string& symbol)
    {
        std::string key = helper::generateCompositeKey(exchange_name, symbol);

        // Only process once per second
        if (storage_.at(key)->size() == 0 || helper::nowToTimestamp() - (*storage_.at(key))[-1][0] >= 1000)
        {
            storage_.at(key)->append(ticker);
        }
    }

    blaze::DynamicMatrix< double > getTickers(const enums::ExchangeName& exchange_name, const std::string& symbol) const
    {
        std::string key = helper::generateCompositeKey(exchange_name, symbol);

        return storage_.at(key)->slice(0, -1);
    }

    blaze::DynamicVector< double > getCurrentTicker(const enums::ExchangeName& exchange_name,
                                                    const std::string& symbol) const
    {
        std::string key = helper::generateCompositeKey(exchange_name, symbol);

        return storage_.at(key)->getRow(-1);
    }

    blaze::DynamicVector< double > getPastTicker(const enums::ExchangeName& exchange_name,
                                                 const std::string& symbol,
                                                 int numberOfTickersAgo) const
    {
        if (numberOfTickersAgo > 120)
        {
            throw std::invalid_argument("Max accepted value for numberOfTickersAgo is 120");
        }

        numberOfTickersAgo = std::abs(numberOfTickersAgo);
        std::string key    = helper::generateCompositeKey(exchange_name, symbol);

        return storage_.at(key)->getRow(-1 - numberOfTickersAgo);
    }

   private:
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > storage_;

    std::string makeKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const
    {
        return enums::toString(exchange_name) + "-" + symbol;
    }
};

} // namespace ticker
} // namespace ct
