// Exchange.hpp
#ifndef CIPHER_EXCHANGE_HPP
#define CIPHER_EXCHANGE_HPP

#include <memory>
#include <string>
#include "DB.hpp"

namespace ct
{
namespace exchange
{

class Exchange
{
   public:
    /**
     * The interface that every Exchange driver has to implement
     */
    virtual ~Exchange() = default;

    /**
     * Place a market order
     * @param symbol The trading pair symbol
     * @param qty The quantity to trade
     * @param current_price The current market price
     * @param side The order side ("buy" or "sell")
     * @param reduce_only Whether the order should only reduce position
     * @return A shared pointer to the created Order
     */
    virtual std::shared_ptr< ct::db::Order > marketOrder(
        const std::string& symbol, double qty, double current_price, const std::string& side, bool reduce_only) = 0;

    /**
     * Place a limit order
     * @param symbol The trading pair symbol
     * @param qty The quantity to trade
     * @param price The limit price
     * @param side The order side ("buy" or "sell")
     * @param reduce_only Whether the order should only reduce position
     * @return A shared pointer to the created Order
     */
    virtual std::shared_ptr< ct::db::Order > limitOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) = 0;

    /**
     * Place a stop order
     * @param symbol The trading pair symbol
     * @param qty The quantity to trade
     * @param price The stop price
     * @param side The order side ("buy" or "sell")
     * @param reduce_only Whether the order should only reduce position
     * @return A shared pointer to the created Order
     */
    virtual std::shared_ptr< ct::db::Order > stopOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) = 0;

    /**
     * Cancel all orders for a symbol
     * @param symbol The trading pair symbol
     */
    virtual void cancelAllOrders(const std::string& symbol) = 0;

    /**
     * Cancel a specific order
     * @param symbol The trading pair symbol
     * @param order_id The ID of the order to cancel
     */
    virtual void cancelOrder(const std::string& symbol, const std::string& order_id) = 0;

   protected:
    /**
     * Fetch trading pair precisions
     * This is a protected method as it should only be called internally
     */
    virtual void fetchPrecisions() = 0;
};

} // namespace exchange
} // namespace ct

#endif // CIPHER_EXCHANGE_HPP
