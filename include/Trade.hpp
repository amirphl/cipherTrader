#pragma once

#include "DB.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Position.hpp"

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
    void addTrade(const blaze::StaticVector< double, 6UL, blaze::rowVector >& trade,
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
     */
    auto getCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    /**
     * @brief Get a past trade for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param number_of_trades_ago Number of trades to go back
     */
    auto getPastTrade(const enums::ExchangeName& exchange_name,
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
};

/**
 * @brief Store for managing closed trades
 *
 * This class stores and manages completed trades for different exchange/symbol pairs.
 */
class ClosedTradesStore
{
   public:
    /**
     * @brief Get the singleton instance
     *
     * @return ClosedTradesStore& Reference to the singleton instance
     */
    static ClosedTradesStore& getInstance()
    {
        static ClosedTradesStore instance;
        return instance;
    }

    /**
     * @brief Get the current trade for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return db::ClosedTrade& Reference to the current trade
     */
    db::ClosedTrade& getCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol);

    /**
     * @brief Reset the current trade for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     */
    void resetCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol);

    /**
     * @brief Add an executed order to the current trade
     *
     * @param executedOrder Executed order
     */
    void addExecutedOrder(const db::Order& executedOrder);

    /**
     * @brief Add order record only
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param side Order side
     * @param qty Order quantity
     * @param price Order price
     */
    void addOrderRecordOnly(const enums::ExchangeName& exchange_name,
                            const std::string& symbol,
                            const enums::OrderSide& side,
                            double qty,
                            double price);

    /**
     * @brief Open a trade
     *
     * @param position Position object
     */
    void openTrade(const position::Position& position);

    /**
     * @brief Close a trade
     *
     * @param position Position object
     */
    void closeTrade(const position::Position& position);

    /**
     * @brief Get the count of closed trades
     *
     * @return size_t Number of closed trades
     */
    size_t getCount() const { return trades_.size(); }

    /**
     * @brief Get all closed trades
     *
     * @return const std::vector<db::ClosedTrade>& Vector of closed trades
     */
    const std::vector< db::ClosedTrade >& getTrades() const { return trades_; }

   private:
    ClosedTradesStore()                                    = default;
    ClosedTradesStore(const ClosedTradesStore&)            = delete;
    ClosedTradesStore& operator=(const ClosedTradesStore&) = delete;

    // Storage for closed trades
    std::vector< db::ClosedTrade > trades_;

    // Temporary storage for trades in progress
    std::unordered_map< std::string, db::ClosedTrade > tempTrades_;

    /**
     * @brief Create a composite key from exchange name and symbol
     *
     * @param exchange Exchange name
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
