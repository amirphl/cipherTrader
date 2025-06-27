#ifndef CIPHER_API_HPP
#define CIPHER_API_HPP

#include "DB.hpp"
#include "Enum.hpp"
#include "Exchange.hpp"

namespace ct
{
namespace api
{

/**
 * @brief API class for handling exchange operations
 *
 * This class manages exchange drivers and provides methods for order operations
 */
class Api
{
   public:
    /**
     * @brief Get the singleton instance of the API
     * @return Reference to the API instance
     */
    static Api& getInstance();

    /**
     * @brief Initialize exchange drivers
     */
    void initDrivers();

    /**
     * @brief Create a market order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param qty Order quantity
     * @param current_price Current price
     * @param side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< std::shared_ptr< db::Order > > marketOrder(const enums::ExchangeName& exchange_name,
                                                              const std::string& symbol,
                                                              double qty,
                                                              double current_price,
                                                              const enums::OrderSide& side,
                                                              bool reduce_only);

    /**
     * @brief Create a limit order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param qty Order quantity
     * @param price Order price
     * @param side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< std::shared_ptr< db::Order > > limitOrder(const enums::ExchangeName& exchange_name,
                                                             const std::string& symbol,
                                                             double qty,
                                                             double price,
                                                             const enums::OrderSide& side,
                                                             bool reduce_only);

    /**
     * @brief Create a stop order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param qty Order quantity
     * @param price Order price
     * @param side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< std::shared_ptr< db::Order > > stopOrder(const enums::ExchangeName& exchange_name,
                                                            const std::string& symbol,
                                                            double qty,
                                                            double price,
                                                            const enums::OrderSide& side,
                                                            bool reduce_only);

    /**
     * @brief Cancel all orders for a symbol
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     */
    void cancelAllOrders(const enums::ExchangeName& exchange_name, const std::string& symbol);

    /**
     * @brief Cancel a specific order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param orderId Order ID to cancel
     */
    void cancelOrder(const enums::ExchangeName& exchange_name, const std::string& symbol, const std::string& orderId);

   private:
    Api();
    ~Api()                     = default;
    Api(const Api&)            = delete;
    Api& operator=(const Api&) = delete;
    Api(Api&&)                 = delete;
    Api& operator=(Api&&)      = delete;

    std::unordered_map< enums::ExchangeName, std::shared_ptr< exchange::Exchange > > drivers_;
};

} // namespace api
} // namespace ct

#endif // CIPHER_API_HPP
