#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "DynamicArray.hpp"
#include "Enum.hpp"

#include <blaze/Math.h>

namespace ct
{
namespace trade
{

/**
 * @brief State for managing trade state
 *
 * This class stores and manages trade data for different exchange/symbol pairs.
 * It aggregates trade data into time-based buckets for analysis.
 */
class TradesState
{
   public:
    /**
     * @brief Get the singleton instance
     *
     * @return TradesState& Reference to the singleton instance
     */
    static TradesState& getInstance()
    {
        static TradesState instance;
        return instance;
    }

    /**
     * @brief Initialize storage for all routes
     */
    void init();

    /**
     * @brief Add a trade to the state
     *
     * @param trade Trade data as a matrix
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     */
    void addTrade(const blaze::DynamicVector< double >& trade,
                  const enums::ExchangeName& exchange_name,
                  const std::string& symbol);

    /**
     * @brief Get all trades for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::DynamicMatrix<double> Matrix of trades
     */
    blaze::DynamicMatrix< double > getTrades(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    /**
     * @brief Get the current trade for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::DynamicVector<double> Current trade data
     */
    blaze::DynamicVector< double > getCurrentTrade(const enums::ExchangeName& exchange_name,
                                                   const std::string& symbol) const;

    /**
     * @brief Get a past trade for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param number_of_trades_ago Number of trades to go back
     * @return blaze::DynamicVector<double> Past trade data
     */
    blaze::DynamicVector< double > getPastTrade(const enums::ExchangeName& exchange_name,
                                                const std::string& symbol,
                                                int number_of_trades_ago) const;

   private:
    TradesState()                              = default;
    TradesState(const TradesState&)            = delete;
    TradesState& operator=(const TradesState&) = delete;

    // Storage for aggregated trades data
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > storage_;

    // Temporary storage for individual trades before aggregation
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > temp_storage_;

    /**
     * @brief Create a composite key from exchange name and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return std::string Composite key
     */
    std::string makeKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const
    {
        return enums::toString(exchange_name) + "-" + symbol;
    }
};

} // namespace trade
} // namespace ct
