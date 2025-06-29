#include "Api.hpp"
#include "Config.hpp"
#include "DB.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Logger.hpp"

ct::api::Api& ct::api::Api::getInstance()
{
    static Api instance;
    return instance;
}

ct::api::Api::Api()
{
    if (!helper::isLive())
    {
        initDrivers();
    }
}

void ct::api::Api::initDrivers()
{
    auto& config = config::Config::getInstance();
    std::vector< std::string > consideringExchanges =
        config.getValue< std::vector< std::string > >("app_considering_exchanges");

    for (const auto& e : consideringExchanges)
    {
        auto exchangeName = enums::toExchangeName(e);

        if (helper::isLive())
        {
            auto initiateWs = [e]()
            {
                auto& config           = config::Config::getInstance();
                auto allExchangeClass  = config.getValue< std::map< std::string, std::string > >("app_live_drivers");
                auto exchangeClassName = allExchangeClass[e];


                // TODO:
                // Note: This is a simplified implementation
                // In a real implementation, you would need to create the appropriate exchange driver
                // based on the exchangeClass string, possibly using a factory pattern

                // TODO: self.drivers[exchange_name] = exchange_class()
            };

            std::thread t(initiateWs);
            t.detach();
        }
        else
        {
            drivers_[exchangeName] = std::make_shared< exchange::Sandbox >();
        }
    }
}

std::optional< std::shared_ptr< ct::db::Order > > ct::api::Api::marketOrder(const enums::ExchangeName& exchange_name,
                                                                            const std::string& symbol,
                                                                            double qty,
                                                                            double current_price,
                                                                            const enums::OrderSide& side,
                                                                            bool reduce_only)
{
    if (drivers_.find(exchange_name) == drivers_.end())
    {
        logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
                         "\" driver not initiated yet. Trying again in the next candle");
        return std::nullopt;
    }

    return drivers_[exchange_name]->marketOrder(symbol, qty, current_price, side, reduce_only);
}

std::optional< std::shared_ptr< ct::db::Order > > ct::api::Api::limitOrder(const enums::ExchangeName& exchange_name,
                                                                           const std::string& symbol,
                                                                           double qty,
                                                                           double price,
                                                                           const enums::OrderSide& side,
                                                                           bool reduce_only)
{
    if (drivers_.find(exchange_name) == drivers_.end())
    {
        logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
                         "\" driver not initiated yet. Trying again in the next candle");
        return std::nullopt;
    }

    return drivers_[exchange_name]->limitOrder(symbol, qty, price, side, reduce_only);
}

std::optional< std::shared_ptr< ct::db::Order > > ct::api::Api::stopOrder(const enums::ExchangeName& exchange_name,
                                                                          const std::string& symbol,
                                                                          double qty,
                                                                          double price,
                                                                          const ct::enums::OrderSide& side,
                                                                          bool reduce_only)
{
    if (drivers_.find(exchange_name) == drivers_.end())
    {
        logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
                         "\" driver not initiated yet. Trying again in the next candle");
        return std::nullopt;
    }

    return drivers_[exchange_name]->stopOrder(symbol, qty, price, side, reduce_only);
}

void ct::api::Api::cancelAllOrders(const enums::ExchangeName& exchange_name, const std::string& symbol)
{
    if (drivers_.find(exchange_name) == drivers_.end())
    {
        logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
                         "\" driver not initiated yet. Trying again in the next candle");
        return;
    }

    drivers_[exchange_name]->cancelAllOrders(symbol);
}

void ct::api::Api::cancelOrder(const enums::ExchangeName& exchange_name,
                               const std::string& symbol,
                               const std::string& order_id)
{
    if (drivers_.find(exchange_name) == drivers_.end())
    {
        logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
                         "\" driver not initiated yet. Trying again in the next candle");
        return;
    }

    drivers_[exchange_name]->cancelOrder(symbol, order_id);
}
