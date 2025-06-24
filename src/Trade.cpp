#include "Trade.hpp"
#include "DB.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Position.hpp"
#include "Route.hpp"

namespace ct
{
namespace trade
{

void TradesState::init()
{
    auto routes = ct::route::Router::getInstance().formattedRoutes();
    for (const auto& route : routes)
    {
        auto exchange_name = route["exchange_name"].get< enums::ExchangeName >();
        auto symbol        = route["symbol"].get< std::string >();
        std::string key    = helper::generateCompositeKey(exchange_name, symbol);

        // Create a dynamic array with 60 rows and 6 columns, dropping at 120
        std::array< size_t, 2 > storageShape = {60, 6};
        storage_.at(key) = std::make_shared< datastructure::DynamicBlazeArray< double > >(storageShape, 120);

        // Create a temporary storage with 100 rows and 4 columns
        std::array< size_t, 2 > tempShape = {100, 4};
        temp_storage_.at(key)             = std::make_shared< datastructure::DynamicBlazeArray< double > >(tempShape);
    }
}

void TradesState::addTrade(const blaze::StaticVector< double, 6UL, blaze::rowVector >& trade,
                           const enums::ExchangeName& exchange_name,
                           const std::string& symbol)
{
    std::string key = helper::generateCompositeKey(exchange_name, symbol);

    // Check if we need to aggregate trades
    if (temp_storage_.at(key)->size() > 0 && trade[0] - (*temp_storage_.at(key))[0][0] >= 1000)
    {
        const auto& arr = *temp_storage_.at(key);

        size_t timestampColumn      = 0;
        size_t priceColumn          = 1;
        size_t qtyColumn            = 2;
        size_t orderSidecolumnIndex = 3;

        auto timestamp = arr[0][timestampColumn];

        auto buyTrades  = arr.filter(orderSidecolumnIndex, 1);
        auto sellTrades = arr.filter(orderSidecolumnIndex, 0);

        double avgPrice = arr.applyFunction(
            [priceColumn, qtyColumn](const blaze::DynamicMatrix< double > data)
            {
                auto col1 = blaze::column(data, priceColumn);
                auto col2 = blaze::column(data, qtyColumn);

                // Compute element-wise product and its sum
                auto turnOver = blaze::sum(col1 * col2);

                // Compute sum of column 2
                auto qtySum = blaze::sum(col2);

                // Check for division by zero
                if (qtySum == 0)
                {
                    throw std::runtime_error("Sum of quantities is zero");
                }

                return turnOver / qtySum;
            });

        auto buyQty  = blaze::sum(blaze::column(buyTrades, qtyColumn));
        auto sellQty = blaze::sum(blaze::column(sellTrades, qtyColumn));

        blaze::StaticVector< double, 6UL, blaze::rowVector > generated{
            timestamp,
            avgPrice,
            static_cast< double >(buyQty),
            static_cast< double >(sellQty),
            static_cast< double >(buyTrades.rows()),
            static_cast< double >(sellTrades.rows()),
        };

        // Append to storage and clear temp storage
        storage_.at(key)->append(generated);
        temp_storage_.at(key)->flush();
    }

    // Add the trade to temporary storage
    temp_storage_.at(key)->append(trade);
}

blaze::DynamicMatrix< double > TradesState::getTrades(const enums::ExchangeName& exchange_name,
                                                      const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    return storage_.at(key)->rows(0, storage_.at(key)->size());
}

auto TradesState::getCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    return storage_.at(key)->row(-1);
}

auto TradesState::getPastTrade(const enums::ExchangeName& exchange_name,
                               const std::string& symbol,
                               int number_of_trades_ago) const
{
    if (number_of_trades_ago > 120)
    {
        throw std::invalid_argument("Max accepted value for number_of_trades_ago is 120");
    }

    number_of_trades_ago = std::abs(number_of_trades_ago);
    std::string key      = makeKey(exchange_name, symbol);

    return storage_.at(key)->row(-1 - number_of_trades_ago);
}

db::ClosedTrade& ClosedTradesStore::getCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    std::string key = makeKey(exchange_name, symbol);

    // If already exists, return it
    if (tempTrades_.find(key) != tempTrades_.end())
    {
        db::ClosedTrade& trade = tempTrades_[key];

        // Set the trade.id if not generated already
        if (trade.getId() != boost::uuids::nil_uuid())
        {
            trade.setId(boost::uuids::random_generator()());
        }

        return trade;
    }

    // Else, create a new trade, store it, and return it
    db::ClosedTrade trade;
    trade.setId(boost::uuids::random_generator()());
    tempTrades_[key] = trade;

    return tempTrades_[key];
}

void ClosedTradesStore::resetCurrentTrade(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    std::string key  = makeKey(exchange_name, symbol);
    tempTrades_[key] = db::ClosedTrade();
}

void ClosedTradesStore::addExecutedOrder(const db::Order& executedOrder)
{
    db::ClosedTrade& trade = getCurrentTrade(executedOrder.getExchangeName(), executedOrder.getSymbol());

    double qty;
    if (executedOrder.isPartiallyFilled())
    {
        qty = executedOrder.getFilledQty();
    }
    else
    {
        qty                 = executedOrder.getQty();
        db::Order orderCopy = executedOrder;
        orderCopy.setTradeId(trade.getId());
        trade.addOrder(orderCopy);
    }

    addOrderRecordOnly(executedOrder.getExchangeName(),
                       executedOrder.getSymbol(),
                       executedOrder.getOrderSide(),
                       qty,
                       executedOrder.getPrice().value_or(.0));
}

// used in add_executed_order() and for when initially adding open positions in live mode.
// used for correct trade-metrics calculations in persistency support for live mode.
void ClosedTradesStore::addOrderRecordOnly(const enums::ExchangeName& exchange_name,
                                           const std::string& symbol,
                                           const enums::OrderSide& side,
                                           double qty,
                                           double price)
{
    db::ClosedTrade& trade = getCurrentTrade(exchange_name, symbol);

    if (side == enums::OrderSide::BUY)
    {
        trade.addBuyOrder(std::abs(qty), price);
    }
    else if (side == enums::OrderSide::SELL)
    {
        trade.addSellOrder(std::abs(qty), price);
    }
    else
    {
        throw std::invalid_argument("Invalid order side: " + enums::toString(side));
    }
}

void ClosedTradesStore::openTrade(const position::Position& position)
{
    db::ClosedTrade& trade = getCurrentTrade(position.getExchangeName(), position.getSymbol());

    trade.setOpenedAt(position.getOpenedAt().value_or(.0));
    trade.setLeverage(position.getLeverage());

    // TODO:
    // trade.setTimeframe(position.getStrategy().getTimeframe());
    // trade.setStrategyName(position.getStrategy().getName());

    trade.setExchangeName(position.getExchangeName());
    trade.setSymbol(position.getSymbol());
    trade.setPositionType(position.getPositionType());
}

void ClosedTradesStore::closeTrade(const position::Position& position)
{
    db::ClosedTrade& trade = getCurrentTrade(position.getExchangeName(), position.getSymbol());

    // If the trade is not open yet we are calling the close_trade function
    if (!trade.isOpen())
    {
        logger::LOG.info(
            "Unable to close a trade that is not yet open. If you're getting this in the live mode, it is likely due"
            " to an unstable connection to the exchange, either on your side or the exchange's side. Please submit a"
            " report using the report button so that Jesse's support team can investigate the issue.");
        return;
    }

    trade.setClosedAt(position.getClosedAt().value_or(.0));

    // TODO:
    // position.getStrategy().incrementTradesCount();
    // if (helper::isLiveTrading())
    // {
    // Ignore store_completed_trade_into_db as requested
    // }

    // Store the trade into the list
    trades_.push_back(trade);

    if (!helper::isUnitTesting())
    {
        logger::LOG.info("CLOSED a {} trade for {}-{}: qty: {}, entry_price: {}, exit_price: {}, "
                         "PNL: {} ({}%)",
                         enums::toString(trade.getPositionType()),
                         enums::toString(trade.getExchangeName()),
                         trade.getSymbol(),
                         trade.getQty(),
                         trade.getEntryPrice(),
                         trade.getExitPrice(),
                         std::round(trade.getPnl() * 100) / 100,
                         std::round(trade.getPnlPercentage() * 100) / 100);
    }

    // At the end, reset the trade variable
    resetCurrentTrade(position.getExchangeName(), position.getSymbol());
}

} // namespace trade
} // namespace ct
