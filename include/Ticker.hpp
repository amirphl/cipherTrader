#pragma once

#include "Precompiled.hpp"

#include "DynamicArray.hpp"
#include "Enum.hpp"

namespace ct
{
namespace ticker
{

class TickersState
{
   public:
    TickersState()  = default;
    ~TickersState() = default;

    void init();

    void addTicker(const blaze::DynamicVector< double, blaze::rowVector >& ticker,
                   const enums::ExchangeName& exchange_name,
                   const std::string& symbol);
    blaze::DynamicMatrix< double > getTickers(const enums::ExchangeName& exchange_name,
                                              const std::string& symbol) const;
    auto getCurrentTicker(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    auto getPastTicker(const enums::ExchangeName& exchange_name,
                       const std::string& symbol,
                       int numberOfTickersAgo) const;

   private:
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > storage_;

    std::string makeKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
};

} // namespace ticker
} // namespace ct
