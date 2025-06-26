#include "Order.hpp"
#include "Config.hpp"
#include "DB.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
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

    for (const auto& exchangeName : tradingExchanges)
    {
        for (const auto& symbol : tradingSymbols)
        {
            std::string key      = helper::makeKey(enums::toExchangeName(exchangeName), symbol);
            storage_[key]        = {};
            active_storage_[key] = {};
        }
    }
}

void ct::order::OrdersState::reset()
{
    // Used for testing
    for (auto& [key, orders] : storage_)
    {
        orders.clear();
        storage_[key].clear();
        active_storage_[key].clear();
    }
}

void ct::order::OrdersState::resetTradeOrders(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    // Used after each completed trade
    std::string key = helper::makeKey(exchange_name, symbol);
    storage_[key].clear();
    active_storage_[key].clear();
}

void ct::order::OrdersState::addOrder(const std::shared_ptr< db::Order > order)
{
    std::string key = helper::makeKey(order->getExchangeName(), order->getSymbol());
    storage_[key].push_back(order);
    active_storage_[key].push_back(order);
}

void ct::order::OrdersState::removeOrder(const std::shared_ptr< db::Order > order)
{
    std::string key = helper::makeKey(order->getExchangeName(), order->getSymbol());

    // Remove from storage
    auto& storageOrders = storage_[key];
    storageOrders.erase(
        std::remove_if(storageOrders.begin(),
                       storageOrders.end(),
                       [&order](const std::shared_ptr< db::Order >& o) { return o->getId() == order->getId(); }),
        storageOrders.end());

    // Remove from active storage
    auto& activeOrders = active_storage_[key];
    activeOrders.erase(
        std::remove_if(activeOrders.begin(),
                       activeOrders.end(),
                       [&order](const std::shared_ptr< db::Order >& o) { return o->getId() == order->getId(); }),
        activeOrders.end());
}

void ct::order::OrdersState::executePendingMarketOrders()
{
    if (to_execute_.empty())
    {
        return;
    }

    for (auto& order : to_execute_)
    {
        order->execute();
    }

    to_execute_.clear();
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getOrders(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);
    auto it         = storage_.find(key);
    if (it != storage_.end())
    {
        return it->second;
    }
    return {};
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getActiveOrders(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    std::string key = helper::makeKey(exchange_name, symbol);
    auto it         = active_storage_.find(key);
    if (it != active_storage_.end())
    {
        return it->second;
    }
    return {};
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getOrders(
    const enums::ExchangeName& exchange_name) const
{
    std::vector< std::shared_ptr< db::Order > > result;

    for (const auto& [key, orders] : storage_)
    {
        for (const auto& order : orders)
        {
            if (order->getExchangeName() == exchange_name)
            {
                result.push_back(order);
            }
        }
    }

    return result;
}

int ct::order::OrdersState::countActiveOrders() const
{
    int count = 0;

    for (const auto& [key, orders] : active_storage_)
    {
        for (const auto& order : orders)
        {
            if (order->isActive())
            {
                count++;
            }
        }
    }

    return count;
}

int ct::order::OrdersState::countActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    auto orders = getActiveOrders(exchange_name, symbol);

    return std::count_if(
        orders.begin(), orders.end(), [](const std::shared_ptr< db::Order > o) { return o->isActive(); });
}

int ct::order::OrdersState::countOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    return getOrders(exchange_name, symbol).size();
}

// TODO: Return null if not found?
std::shared_ptr< ct::db::Order > ct::order::OrdersState::getOrderById(const enums::ExchangeName& exchange_name,
                                                                      const std::string& symbol,
                                                                      const std::string& id,
                                                                      bool use_exchange_id) const
{
    std::string key = helper::makeKey(exchange_name, symbol);
    auto it         = storage_.find(key);

    if (it == storage_.end())
    {
        return std::make_shared< db::Order >(); // Return empty order
    }

    const auto& orders = it->second;

    if (use_exchange_id)
    {
        // Find by exchange ID
        auto it = std::find_if(orders.begin(),
                               orders.end(),
                               [&id](const std::shared_ptr< db::Order >& o)
                               {
                                   auto exchangeId = o->getExchangeId();
                                   return exchangeId && *exchangeId == id;
                               });

        if (it != orders.end())
        {
            return *it;
        }
    }
    else
    {
        // Make sure ID is not empty
        if (id.empty())
        {
            return std::make_shared< db::Order >(); // Return empty order
        }

        // Find by client ID (contains the ID string)
        for (auto it = orders.rbegin(); it != orders.rend(); ++it)
        {
            if ((*it)->getIdAsString().find(id) != std::string::npos)
            {
                return *it;
            }
        }
    }

    return std::make_shared< db::Order >(); // Return empty order
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getEntryOrders(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    auto& positionsState = position::PositionsState::getInstance();
    auto position        = positionsState.getPosition(exchange_name, symbol);

    if (!position)
    {
        return {};
    }

    if (position->isClose())
    {
        return getOrders(exchange_name, symbol);
    }

    auto activeOrders = getActiveOrders(exchange_name, symbol);
    auto pSide        = helper::positionTypeToOrderSide(position->getPositionType());

    std::vector< std::shared_ptr< db::Order > > entryOrders;
    std::copy_if(activeOrders.begin(),
                 activeOrders.end(),
                 std::back_inserter(entryOrders),
                 [&pSide](const std::shared_ptr< db::Order >& o)
                 { return o->getOrderSide() == pSide && !o->isCanceled(); });

    return entryOrders;
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getExitOrders(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    auto orders = getOrders(exchange_name, symbol);

    if (orders.empty())
    {
        return {};
    }

    auto& positionsState = position::PositionsState::getInstance();
    auto position        = positionsState.getPosition(exchange_name, symbol);

    if (!position)
    {
        return {};
    }

    if (position->isClose())
    {
        return {};
    }

    std::vector< std::shared_ptr< db::Order > > exitOrders;
    auto pSide = helper::positionTypeToOrderSide(position->getPositionType());

    std::copy_if(orders.begin(),
                 orders.end(),
                 std::back_inserter(exitOrders),
                 [&pSide](const std::shared_ptr< db::Order >& o)
                 { return o->getOrderSide() != pSide && !o->isCanceled(); });


    return exitOrders;
}

std::vector< std::shared_ptr< ct::db::Order > > ct::order::OrdersState::getActiveExitOrders(
    const enums::ExchangeName& exchange_name, const std::string& symbol) const
{
    auto activeOrders = getActiveOrders(exchange_name, symbol);

    if (activeOrders.empty())
    {
        return {};
    }

    auto& positionState = position::PositionsState::getInstance();
    auto position       = positionState.getPosition(exchange_name, symbol);

    if (!position)
    {
        return {};
    }

    if (position->isClose())
    {
        return {};
    }

    std::vector< std::shared_ptr< db::Order > > exitOrders;
    auto pSide = helper::positionTypeToOrderSide(position->getPositionType());

    std::copy_if(activeOrders.begin(),
                 activeOrders.end(),
                 std::back_inserter(exitOrders),
                 [&pSide](const std::shared_ptr< db::Order >& o)
                 { return o->getOrderSide() != pSide && !o->isCanceled(); });


    return exitOrders;
}

void ct::order::OrdersState::updateActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    auto activeOrders = getActiveOrders(exchange_name, symbol);

    std::vector< std::shared_ptr< db::Order > > result;

    std::copy_if(activeOrders.begin(),
                 activeOrders.end(),
                 std::back_inserter(result),
                 [](const std::shared_ptr< db::Order >& o) { return !o->isCanceled() && !o->isExecuted(); });

    std::string key = helper::makeKey(exchange_name, symbol);

    active_storage_[key] = result;
}
