#ifndef CIPHER_API_HPP
#define CIPHER_API_HPP

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "DB.hpp"
#include "Enum.hpp"
#include "Helper.hpp"

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
     * @param currentPrice Current price
     * @param order_side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< db::Order > marketOrder(const enums::ExchangeName& exchange_name,
                                           const std::string& symbol,
                                           double qty,
                                           double currentPrice,
                                           const enums::OrderSide& order_side,
                                           bool reduce_only);

    /**
     * @brief Create a limit order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param qty Order quantity
     * @param price Order price
     * @param order_side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< db::Order > limitOrder(const enums::ExchangeName& exchange_name,
                                          const std::string& symbol,
                                          double qty,
                                          double price,
                                          const enums::OrderSide& order_side,
                                          bool reduce_only);

    /**
     * @brief Create a stop order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param qty Order quantity
     * @param price Order price
     * @param order_side Order side (buy/sell)
     * @param reduce_only Whether this is a reduce-only order
     * @return Order object if successful, nullopt otherwise
     */
    std::optional< db::Order > stopOrder(const enums::ExchangeName& exchange,
                                         const std::string& symbol,
                                         double qty,
                                         double price,
                                         const enums::OrderSide& order_side,
                                         bool reduce_only);

    /**
     * @brief Cancel all orders for a symbol
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @return True if successful, false otherwise
     */
    bool cancelAllOrders(const enums::ExchangeName& exchange, const std::string& symbol);

    /**
     * @brief Cancel a specific order
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param orderId Order ID to cancel
     * @return True if successful, false otherwise
     */
    bool cancelOrder(const enums::ExchangeName& exchange, const std::string& symbol, const std::string& orderId);

   private:
    Api();
    ~Api()                     = default;
    Api(const Api&)            = delete;
    Api& operator=(const Api&) = delete;
    Api(Api&&)                 = delete;
    Api& operator=(Api&&)      = delete;

    std::unordered_map< std::string, std::shared_ptr< void > > drivers_;
};

// Global API instance
extern Api& api;

} // namespace api
} // namespace ct

#endif // CIPHER_API_HPP
