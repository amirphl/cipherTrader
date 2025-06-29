#pragma once

#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "LimitOrderbook.hpp"

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
     * @return blaze::StaticVector< lob::LimitOrderbook< lob::R_, lob::C_ >, 2UL, blaze::rowVector > Formatted orderbook
     */
    blaze::StaticVector< lob::LimitOrderbook< lob::R_, lob::C_ >, 2UL, blaze::rowVector > formatOrderbook(
        const enums::ExchangeName& exchange_name, const std::string& symbol) const;

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
     */
    auto getCurrentOrderbook(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    /**
     * @brief Get the current asks for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return lob::LimitOrderbook< lob::R_, lob::C_ > Current asks data
     */
    lob::LimitOrderbook< lob::R_, lob::C_ > getCurrentAsks(const enums::ExchangeName& exchange_name,
                                                           const std::string& symbol) const;

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
     * @return lob::LimitOrderbook< lob::R_, lob::C_ > Current bids data
     */
    lob::LimitOrderbook< lob::R_, lob::C_ > getCurrentBids(const enums::ExchangeName& exchange_name,
                                                           const std::string& symbol) const;

    /**
     * @brief Get the best bid for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return blaze::StaticVector< double, 2UL > Best ask data
     */
    blaze::StaticVector< double, 2UL > getBestBid(const enums::ExchangeName& exchange_name,
                                                  const std::string& symbol) const;

    /**
     * @brief Get all orderbooks for a specific exchange and symbol
     *
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return datastructure::DynamicBlazeArray< lob::LimitOrderbook< lob::R_, lob::C_ > > All orderbook data
     */
    datastructure::DynamicBlazeArray< lob::LimitOrderbook< lob::R_, lob::C_ > > getOrderbooks(
        const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    static double trim(double price, bool ascending, double unit);

   private:
    OrderbooksState()                                  = default;
    OrderbooksState(const OrderbooksState&)            = delete;
    OrderbooksState& operator=(const OrderbooksState&) = delete;

    // Storage for orderbook data
    std::unordered_map< std::string,
                        std::shared_ptr< datastructure::DynamicBlazeArray< lob::LimitOrderbook< lob::R_, lob::C_ > > > >
        storage_;

    // Temporary storage for orderbook data before processing
    struct TempOrderbookData
    {
        int64_t last_updated_timestamp_ = 0;
        std::vector< std::array< double, 2 > > asks_;
        std::vector< std::array< double, 2 > > bids_;
    };

    std::unordered_map< std::string, TempOrderbookData > temp_storage_;

    /**
     * @brief Trim orderbook to specified precision
     *
     * @param arr Input array of price/quantity pairs
     * @param ascending Sort direction
     * @param limit_len Maximum length of output array
     * @return std::vector< std::array< double, 2 > > Trimmed orderbook
     */
    std::vector< std::array< double, 2 > > trim(const std::vector< std::array< double, 2 > >& arr,
                                                bool ascending,
                                                size_t limit_len = lob::R_) const;

    /**
     * @brief Fix array length by padding with NaN values
     *
     * @param arr Input array
     * @param target_len Target length
     * @return lob::LimitOrderbook< lob::R_, lob::C_ > Fixed length array
     */
    lob::LimitOrderbook< lob::R_, lob::C_ > fixLen(const std::vector< std::array< double, 2 > >& arr,
                                                   size_t target_len) const;
};

} // namespace orderbook
} // namespace ct
