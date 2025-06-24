// #include "Api.hpp"
// #include "Config.hpp"
// #include "DB.hpp"
// #include "Enum.hpp"
// #include "Helper.hpp"
// #include "Logger.hpp"
// #include "exchanges/Sandbox.hpp"

// ct::api::Api& ct::api::Api::getInstance()
// {
//     static Api instance;
//     return instance;
// }
//
// ct::api::Api::Api()
// {
//     if (!helper::isLive())
//     {
//         initDrivers();
//     }
// }
//
// void ct::api::Api::initDrivers()
// {
//     auto& config = config::Config::getInstance();
//     std::vector< std::string > consideringExchanges =
//         config.getValue< std::vector< std::string > >("app_considering_exchanges");
//
//     // A helpful assertion
//     if (consideringExchanges.empty())
//     {
//         throw std::runtime_error("No exchange is available for initiating in the API class");
//     }
//
//     for (const auto& exchangeName : consideringExchanges)
//     {
//         if (helper::isLive())
//         {
//             auto initiateWs = [exchangeName]()
//             {
//                 auto& config           = config::Config::getInstance();
//                 auto allExchangeClass  = config.getValue< std::map< std::string, std::string > >("app_live_drivers");
//                 auto exchangeClassName = allExchangeClass[exchangeName];
//
//
//                 // TODO:
//                 // Note: This is a simplified implementation
//                 // In a real implementation, you would need to create the appropriate exchange driver
//                 // based on the exchangeClass string, possibly using a factory pattern
//
//                 // TODO: self.drivers[exchange_name] = exchange_class()
//             };
//
//             std::thread t(initiateWs);
//             t.detach();
//         }
//         else
//         {
//             // In a real implementation, you would include the Sandbox exchange class
//             // and create an instance of it
//             // For now, we'll just log that we would create a sandbox driver
//             logger::LOG.info("Creating sandbox driver for exchange: " + exchangeName);
//
//             // Placeholder for actual sandbox driver creation
//             // this->drivers_[e] = std::make_shared<exchanges::Sandbox>(e);
//         }
//     }
// }
//
// std::optional< ct::db::Order > ct::api::Api::marketOrder(const ct::enums::ExchangeName& exchange_name,
//                                                          const std::string& symbol,
//                                                          double qty,
//                                                          double currentPrice,
//                                                          const ct::enums::OrderSide& order_side,
//                                                          bool reduce_only)
// {
//     if (drivers_.find(exchange_name) == drivers_.end())
//     {
//         logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
//                          "\" driver not initiated yet. Trying again in the next candle");
//         return std::nullopt;
//     }
//
//     // In a real implementation, you would call the appropriate method on the driver
//     // For now, we'll just return nullopt
//     return std::nullopt;
// }
//
// std::optional< ct::db::Order > ct::api::Api::limitOrder(const ct::enums::ExchangeName& exchange_name,
//                                                         const std::string& symbol,
//                                                         double qty,
//                                                         double price,
//                                                         const ct::enums::OrderSide& order_side,
//                                                         bool reduce_only)
// {
//     if (drivers_.find(exchange_name) == drivers_.end())
//     {
//         logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
//                          "\" driver not initiated yet. Trying again in the next candle");
//         return std::nullopt;
//     }
//
//     // In a real implementation, you would call the appropriate method on the driver
//     // For now, we'll just return nullopt
//     return std::nullopt;
// }
//
// std::optional< ct::db::Order > ct::api::Api::stopOrder(const ct::enums::ExchangeName& exchange_name,
//                                                        const std::string& symbol,
//                                                        double qty,
//                                                        double price,
//                                                        const ct::enums::OrderSide& order_side,
//                                                        bool reduce_only)
// {
//     if (drivers_.find(exchange_name) == drivers_.end())
//     {
//         logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
//                          "\" driver not initiated yet. Trying again in the next candle");
//         return std::nullopt;
//     }
//
//     // In a real implementation, you would call the appropriate method on the driver
//     // For now, we'll just return nullopt
//     return std::nullopt;
// }
//
// bool ct::api::Api::cancelAllOrders(const ct::enums::ExchangeName& exchange_name, const std::string& symbol)
// {
//     if (drivers_.find(exchange_name) == drivers_.end())
//     {
//         logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
//                          "\" driver not initiated yet. Trying again in the next candle");
//         return false;
//     }
//
//     // In a real implementation, you would call the appropriate method on the driver
//     // For now, we'll just return false
//     return false;
// }
//
// bool ct::api::Api::cancelOrder(const ct::enums::ExchangeName& exchange_name,
//                                const std::string& symbol,
//                                const std::string& order_id)
// {
//     if (drivers_.find(exchange_name) == drivers_.end())
//     {
//         logger::LOG.info("Exchange \"" + enums::toString(exchange_name) +
//                          "\" driver not initiated yet. Trying again in the next candle");
//         return false;
//     }
//
//     // In a real implementation, you would call the appropriate method on the driver
//     // For now, we'll just return false
//     return false;
// }
