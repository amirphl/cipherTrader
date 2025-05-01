#include <algorithm>

#include "Config.hpp"
#include "DB.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Order.hpp"
#include "Position.hpp"


// Singleton instance
ct::order::OrdersState& ct::order::OrdersState::getInstance()
{
    static ct::order::OrdersState instance;
    return instance;
}

ct::order::OrdersState::OrdersState()
{
    // Initialize storage with trading exchanges and symbols from config
    const auto& config = config::Config::getInstance();

    std::vector< std::string > tradingExchanges;
    std::vector< std::string > tradingSymbols;

    tradingExchanges = config.getValue< std::vector< std::string > >("app_trading_exchanges");
    tradingSymbols   = config.getValue< std::vector< std::string > >("app_trading_symbols");

    for (const auto& exchange_name : tradingExchanges)
    {
        for (const auto& symbol : tradingSymbols)
        {
            std::string key     = makeKey(enums::toExchangeName(exchange_name), symbol);
            storage_[key]       = {};
            activeStorage_[key] = {};
        }
    }
}

void ct::order::OrdersState::reset()
{
    // Used for testing
    for (auto& [key, orders] : storage_)
    {
        orders.clear();
        activeStorage_[key].clear();
    }
}

void ct::order::OrdersState::resetTradeOrders(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    // Used after each completed trade
    std::string key = makeKey(exchange_name, symbol);
    storage_[key].clear();
    activeStorage_[key].clear();
}

void ct::order::OrdersState::addOrder(const db::Order& order)
{
    std::string key = makeKey(order.getExchangeName(), order.getSymbol());
    storage_[key].push_back(order);
    activeStorage_[key].push_back(order);
}

void ct::order::OrdersState::removeOrder(const db::Order& order)
{
    std::string key = makeKey(order.getExchangeName(), order.getSymbol());

    // Remove from storage
    auto& storageOrders = storage_[key];
    storageOrders.erase(std::remove_if(storageOrders.begin(),
                                       storageOrders.end(),
                                       [&order](const db::Order& o) { return o.getId() == order.getId(); }),
                        storageOrders.end());

    // Remove from active storage
    auto& activeOrders = activeStorage_[key];
    activeOrders.erase(std::remove_if(activeOrders.begin(),
                                      activeOrders.end(),
                                      [&order](const db::Order& o) { return o.getId() == order.getId(); }),
                       activeOrders.end());
}

void ct::order::OrdersState::executePendingMarketOrders()
{
    if (toExecute_.empty())
    {
        return;
    }

    for (auto& order : toExecute_)
    {
        order.execute();
    }

    toExecute_.clear();
}

std::vector< ct::db::Order > ct::order::OrdersState::getOrders(const enums::ExchangeName& exchange_name,
                                                               const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    auto it         = storage_.find(key);
    if (it != storage_.end())
    {
        return it->second;
    }
    return {};
}

std::vector< ct::db::Order > ct::order::OrdersState::getActiveOrders(const enums::ExchangeName& exchange_name,
                                                                     const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    auto it         = activeStorage_.find(key);
    if (it != activeStorage_.end())
    {
        return it->second;
    }
    return {};
}

std::vector< ct::db::Order > ct::order::OrdersState::getAllOrders(const enums::ExchangeName& exchange_name) const
{
    std::vector< db::Order > result;

    for (const auto& [key, orders] : storage_)
    {
        for (const auto& order : orders)
        {
            if (order.getExchangeName() == exchange_name)
            {
                result.push_back(order);
            }
        }
    }

    return result;
}

int ct::order::OrdersState::countAllActiveOrders() const
{
    int count = 0;

    for (const auto& [key, orders] : activeStorage_)
    {
        if (orders.empty())
        {
            continue;
        }

        for (const auto& order : orders)
        {
            if (order.isActive())
            {
                count++;
            }
        }
    }

    return count;
}

int ct::order::OrdersState::countActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::vector< db::Order > orders = getActiveOrders(exchange_name, symbol);

    return std::count_if(orders.begin(), orders.end(), [](const db::Order& o) { return o.isActive(); });
}

int ct::order::OrdersState::countOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    return getOrders(exchange_name, symbol).size();
}

ct::db::Order ct::order::OrdersState::getOrderById(const enums::ExchangeName& exchange_name,
                                                   const std::string& symbol,
                                                   const std::string& exchange_id,
                                                   bool use_exchange_id) const
{
    std::string key = makeKey(exchange_name, symbol);
    auto it         = storage_.find(key);

    if (it == storage_.end())
    {
        return db::Order(); // Return empty order
    }

    const auto& orders = it->second;

    if (use_exchange_id)
    {
        // Find by exchange ID
        auto orderIt = std::find_if(orders.begin(),
                                    orders.end(),
                                    [&exchange_id](const db::Order& o)
                                    {
                                        auto exchangeId = o.getExchangeId();
                                        return exchangeId && *exchangeId == exchange_id;
                                    });

        if (orderIt != orders.end())
        {
            return *orderIt;
        }
    }
    else
    {
        // Make sure ID is not empty
        if (exchange_id.empty())
        {
            return db::Order();
        }

        // Find by client ID (contains the ID string)
        // Note: In Python, this searches in reverse order
        for (auto it = orders.rbegin(); it != orders.rend(); ++it)
        {
            if (it->getIdAsString().find(exchange_id) != std::string::npos)
            {
                return *it;
            }
        }
    }

    return db::Order(); // Return empty order if not found
}

std::vector< ct::db::Order > ct::order::OrdersState::getEntryOrders(const enums::ExchangeName& exchange_name,
                                                                    const std::string& symbol) const
{
    auto& positionsState = position::PositionsState::getInstance();
    auto position        = positionsState.getPosition(exchange_name, symbol);

    if (position.isClose())
    {
        return getOrders(exchange_name, symbol);
    }

    std::vector< db::Order > allOrders = getActiveOrders(exchange_name, symbol);
    auto pSide                         = helper::positionTypeToOrderSide(position.getPositionType());

    std::vector< db::Order > entryOrders;
    std::copy_if(allOrders.begin(),
                 allOrders.end(),
                 std::back_inserter(entryOrders),
                 [&pSide](const db::Order& o) { return o.getOrderSide() == pSide && !o.isCanceled(); });

    return entryOrders;
}

std::vector< ct::db::Order > ct::order::OrdersState::getExitOrders(const enums::ExchangeName& exchange_name,
                                                                   const std::string& symbol) const
{
    std::vector< db::Order > allOrders = getOrders(exchange_name, symbol);

    if (allOrders.empty())
    {
        return {};
    }

    auto& positionsState = position::PositionsState::getInstance();
    auto position        = positionsState.getPosition(exchange_name, symbol);
    if (position.isClose())
    {
        return {};
    }

    std::vector< db::Order > exitOrders;
    auto pSide = helper::positionTypeToOrderSide(position.getPositionType());

    std::copy_if(allOrders.begin(),
                 allOrders.end(),
                 std::back_inserter(exitOrders),
                 [&pSide](const db::Order& o) { return o.getOrderSide() != pSide && !o.isCanceled(); });


    return exitOrders;
}

std::vector< ct::db::Order > ct::order::OrdersState::getActiveExitOrders(const enums::ExchangeName& exchange_name,
                                                                         const std::string& symbol) const
{
    std::vector< db::Order > allOrders = getActiveOrders(exchange_name, symbol);

    if (allOrders.empty())
    {
        return {};
    }

    auto& positionState = position::PositionsState::getInstance();
    auto position       = positionState.getPosition(exchange_name, symbol);
    if (position.isClose())
    {
        return {};
    }

    std::vector< db::Order > exitOrders;
    auto pSide = helper::positionTypeToOrderSide(position.getPositionType());

    std::copy_if(allOrders.begin(),
                 allOrders.end(),
                 std::back_inserter(exitOrders),
                 [&pSide](const db::Order& o) { return o.getOrderSide() != pSide && !o.isCanceled(); });


    return exitOrders;
}

std::string ct::order::OrdersState::makeKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    return enums::toString(exchange_name) + "-" + symbol;
}
