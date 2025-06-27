#pragma once

#include "DB.hpp"
#include "Enum.hpp"
#include "Position.hpp"
#include "Timeframe.hpp"

namespace ct
{
namespace broker
{

class Broker
{
   private:
    std::shared_ptr< position::Position > position_;
    std::string symbol_;
    timeframe::Timeframe timeframe_;
    enums::ExchangeName exchange_name_;

   public:
    /**
     * @brief Constructs a new Broker object
     * @param position Position object
     * @param exchange_name Exchange name
     * @param symbol Trading symbol
     * @param timeframe Timeframe string
     */
    Broker(std::shared_ptr< position::Position > position,
           const enums::ExchangeName& exchange_name,
           const std::string& symbol,
           const timeframe::Timeframe& timeframe);

    /**
     * @brief Creates a market sell order
     * @param qty Quantity to sell
     * @return Order pointer or nullopt if order creation failed
     */
    std::optional< std::shared_ptr< db::Order > > sellAtMarket(double qty);

    /**
     * @brief Creates a limit sell order
     * @param qty Quantity to sell
     * @param price Price at which to sell
     * @return Order pointer or nullopt if order creation failed
     * @throws std::invalid_argument if price is negative
     */
    std::optional< std::shared_ptr< db::Order > > sellAt(double qty, double price);

    /**
     * @brief Creates a market buy order
     * @param qty Quantity to buy
     * @return Order pointer or nullopt if order creation failed
     */
    std::optional< std::shared_ptr< db::Order > > buyAtMarket(double qty);

    /**
     * @brief Creates a limit buy order
     * @param qty Quantity to buy
     * @param price Price at which to buy
     * @return Order pointer or nullopt if order creation failed
     * @throws std::invalid_argument if price is negative
     */
    std::optional< std::shared_ptr< db::Order > > buyAt(double qty, double price);

    /**
     * @brief Reduces position at specified price
     * @param qty Quantity to reduce
     * @param price Target price
     * @param current_price Current market price
     * @return Order pointer or nullopt if order creation failed
     * @throws std::invalid_argument if price is negative
     * @throws OrderNotAllowed if position is closed or order doesn't reduce position
     */
    std::optional< std::shared_ptr< db::Order > > reducePositionAt(double qty, double price, double current_price);

    /**
     * @brief Creates a stop order to start a profit position
     * @param side Order side ("buy" or "sell")
     * @param qty Quantity
     * @param price Target price
     * @return Order pointer or nullopt if order creation failed
     * @throws std::invalid_argument if price is negative
     * @throws OrderNotAllowed if price conditions are invalid
     */
    std::optional< std::shared_ptr< db::Order > > startProfitAt(const enums::OrderSide& side, double qty, double price);

    /**
     * @brief Cancels all orders for the current symbol
     */
    void cancelAllOrders();

    /**
     * @brief Cancels a specific order
     * @param orderId ID of the order to cancel
     */
    void cancelOrder(const std::string& orderId);
};

} // namespace broker
} // namespace ct
