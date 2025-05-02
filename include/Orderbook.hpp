#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DynamicArray.hpp"
#include "Enum.hpp"

#include <blaze/Math.h>

namespace ct
{
namespace orderbook
{

/**
 * @brief State for managing orderbook state
 *
 * This class stores and manages orderbook data for different exchange/symbol pairs.
 * It aggregates orderbook data into time-based buckets for analysis.
 */
class OrderbooksState
{
    const static size_t R_ = 50;
    const static size_t C_ = 2;
    using LOB              = std::array< std::array< double, R_ >, C_ >;

   public:
    /**
     * @brief Get the singleton instance
     *
     * @return OrderbooksState& Reference to the singleton instance
     */
    static OrderbooksState& getInstance()
    {
        static OrderbooksState instance;
        return instance;
    }

    /**
     * @brief Initialize storage for all routes
     */
    void init();

    /**
     * @brief Format orderbook data for storage
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::StaticVector< LOB, 2UL > Formatted orderbook
     */
    blaze::StaticVector< LOB, 2UL > formatOrderbook(const enums::ExchangeName& exchange_name,
                                                    const std::string& symbol) const;

    /**
     * @brief Add orderbook data to the state
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param asks List of ask prices and quantities
     * @param bids List of bid prices and quantities
     */
    void addOrderbook(const enums::ExchangeName& exchange_name,
                      const std::string& symbol,
                      const std::vector< std::array< double, 2 > >& asks,
                      const std::vector< std::array< double, 2 > >& bids);

    /**
     * @brief Get the current orderbook for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::DynamicVector< LOB > Current orderbook data
     */
    blaze::DynamicVector< LOB > getCurrentOrderbook(const enums::ExchangeName& exchange_name,
                                                    const std::string& symbol) const;

    /**
     * @brief Get the current asks for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return LOB Current asks data
     */
    LOB getCurrentAsks(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    /**
     * @brief Get the best ask for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::StaticVector<double, 2UL> Best ask data
     */
    blaze::StaticVector< double, 2UL > getBestAsk(const enums::ExchangeName& exchange_name,
                                                  const std::string& symbol) const;

    /**
     * @brief Get the current bids for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return LOB Current bids data
     */
    LOB getCurrentBids(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    /**
     * @brief Get the best bid for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::StaticVector<double, 2UL> Best ask data
     */
    blaze::StaticVector< double, 2UL > getBestBid(const enums::ExchangeName& exchange_name,
                                                  const std::string& symbol) const;

    /**
     * @brief Get all orderbooks for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::DynamicMatrix<LOB> All orderbook data
     */
    datastructure::DynamicBlazeArray< LOB > getOrderbooks(const enums::ExchangeName& exchange_name,
                                                          const std::string& symbol) const;

   private:
    OrderbooksState()                                  = default;
    OrderbooksState(const OrderbooksState&)            = delete;
    OrderbooksState& operator=(const OrderbooksState&) = delete;

    // Storage for orderbook data
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< LOB > > > storage_;

    // Temporary storage for orderbook data before processing
    struct TempOrderbookData
    {
        int64_t last_updated_timestamp = 0;
        std::vector< std::array< double, 2 > > asks;
        std::vector< std::array< double, 2 > > bids;
    };

    std::unordered_map< std::string, TempOrderbookData > temp_storage_;

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

    /**
     * @brief Trim orderbook to specified precision
     *
     * @param arr Input array of price/quantity pairs
     * @param ascending Sort direction
     * @param limit_len Maximum length of output array
     * @return std::vector<std::array<double, 2>> Trimmed orderbook
     */
    std::vector< std::array< double, 2 > > trim(const std::vector< std::array< double, 2 > >& arr,
                                                bool ascending,
                                                size_t limit_len = R_) const;

    /**
     * @brief Fix array length by padding with NaN values
     *
     * @param arr Input array
     * @param target_len Target length
     * @return std::array<std::array<double,R_>,C_> Fixed length array
     * @return blaze::DynamicMatrix<double>
     */
    auto fixLen(const std::vector< std::array< double, 2 > >& arr, size_t target_len) const;

    double trimPrice(double price, bool ascending, double unit) const;
};

} // namespace orderbook
} // namespace ct
