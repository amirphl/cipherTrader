#include "Broker.hpp"
#include "Api.hpp"
#include "DB.hpp"
#include "Enum.hpp"
#include "Exception.hpp"
#include "Helper.hpp"
#include "Position.hpp"
#include "Timeframe.hpp"

namespace ct
{

namespace broker
{

ct::broker::Broker::Broker(std::shared_ptr< position::Position > position,
                           const enums::ExchangeName& exchange_name,
                           const std::string& symbol,
                           const timeframe::Timeframe& timeframe)
    : position_(position), symbol_(symbol), timeframe_(timeframe), exchange_name_(exchange_name)
{
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::sellAtMarket(double qty)
{
    assert(qty != 0);

    return api::Api::getInstance().marketOrder(
        exchange_name_, symbol_, std::abs(qty), position_->getCurrentPrice().value(), enums::OrderSide::SELL, false);
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::sellAt(double qty, double price)
{
    assert(qty != 0);

    if (price < 0)
    {
        throw std::invalid_argument("price cannot be negative.");
    }

    return api::Api::getInstance().limitOrder(
        exchange_name_, symbol_, std::abs(qty), price, enums::OrderSide::SELL, false);
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::buyAtMarket(double qty)
{
    assert(qty != 0);

    return api::Api::getInstance().marketOrder(
        exchange_name_, symbol_, std::abs(qty), position_->getCurrentPrice().value(), enums::OrderSide::BUY, false);
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::buyAt(double qty, double price)
{
    assert(qty != 0);

    if (price < 0)
    {
        throw std::invalid_argument("price cannot be negative.");
    }

    return api::Api::getInstance().limitOrder(
        exchange_name_, symbol_, std::abs(qty), price, enums::OrderSide::BUY, false);
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::reducePositionAt(double qty,
                                                                                   double price,
                                                                                   double current_price)
{
    assert(qty != 0);

    // validation
    if (price < 0)
    {
        throw std::invalid_argument("order price cannot be negative. You passed " + std::to_string(price));
    }

    qty = std::abs(qty);

    // validation
    if (position_->isClose())
    {
        throw exception::OrderNotAllowed("Cannot submit a reduce_position order when there is no open position");
    }

    auto side = helper::oppositeSide(helper::positionTypeToOrderSide(position_->getPositionType()));

    // MARKET order
    // if the price difference is below 0.01% of the current price, then we submit a market order
    if (helper::isPriceNear(price, current_price))
    {
        return api::Api::getInstance().marketOrder(exchange_name_, symbol_, qty, price, side, true);
    }

    // LIMIT order
    else if ((side == enums::OrderSide::SELL && position_->getPositionType() == enums::PositionType::LONG &&
              price > current_price) ||
             (side == enums::OrderSide::BUY && position_->getPositionType() == enums::PositionType::SHORT &&
              price < current_price))
    {
        return api::Api::getInstance().limitOrder(exchange_name_, symbol_, qty, price, side, true);
    }

    // STOP order
    else if ((side == enums::OrderSide::SELL && position_->getPositionType() == enums::PositionType::LONG &&
              price < current_price) ||
             (side == enums::OrderSide::BUY && position_->getPositionType() == enums::PositionType::SHORT &&
              price > current_price))
    {
        return api::Api::getInstance().stopOrder(exchange_name_, symbol_, std::abs(qty), price, side, true);
    }
    else
    {
        throw exception::OrderNotAllowed("This order doesn't seem to be for reducing the position.");
    }
}

std::optional< std::shared_ptr< db::Order > > ct::broker::Broker::startProfitAt(const enums::OrderSide& side,
                                                                                double qty,
                                                                                double price)
{
    assert(qty != 0);

    if (price < 0)
    {
        throw std::invalid_argument("price cannot be negative.");
    }

    if (side == enums::OrderSide::BUY && price < position_->getCurrentPrice())
    {
        throw exception::OrderNotAllowed("A buy start_profit(" + std::to_string(price) +
                                         ") order must have a price higher than current_price(" +
                                         std::to_string(position_->getCurrentPrice().value()) + ").");
    }
    if (side == enums::OrderSide::SELL && price > position_->getCurrentPrice())
    {
        throw exception::OrderNotAllowed("A sell start_profit(" + std::to_string(price) +
                                         ") order must have a price lower than current_price(" +
                                         std::to_string(position_->getCurrentPrice().value()) + ").");
    }

    return api::Api::getInstance().stopOrder(exchange_name_, symbol_, std::abs(qty), price, side, false);
}

void ct::broker::Broker::cancelAllOrders()
{
    return api::Api::getInstance().cancelAllOrders(exchange_name_, symbol_);
}

void ct::broker::Broker::cancelOrder(const std::string& order_id)
{
    return api::Api::getInstance().cancelOrder(exchange_name_, symbol_, order_id);
}

} // namespace broker
} // namespace ct
