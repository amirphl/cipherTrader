#ifndef CT_ORDER_HPP
#define CT_ORDER_HPP

#include <map>
#include <string>
#include <vector>

#include "DB.hpp"

namespace ct
{
namespace order
{

class OrderRepository
{
   public:
    // Singleton access
    static OrderRepository& getInstance();

    // Constructor
    OrderRepository();

    // Reset methods
    void reset();
    void resetTradeOrders(const enums::ExchangeName& exchange_name, const std::string& symbol);

    // Order management
    void addOrder(const db::Order& order);
    void removeOrder(const db::Order& order);
    void executePendingMarketOrders();

    // Getters
    std::vector< db::Order > getOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    std::vector< db::Order > getActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    std::vector< db::Order > getAllOrders(const enums::ExchangeName& exchange_name) const;

    int countAllActiveOrders() const;
    int countActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    int countOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    db::Order getOrderById(const enums::ExchangeName& exchange_name,
                           const std::string& symbol,
                           const std::string& id,
                           bool useExchangeId = false) const;

    std::vector< db::Order > getEntryOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    std::vector< db::Order > getExitOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    std::vector< db::Order > getActiveExitOrders(const enums::ExchangeName& exchange_name,
                                                 const std::string& symbol) const;

   private:
    // Used in simulation only
    std::vector< db::Order > toExecute_;

    // Storage maps
    std::map< std::string, std::vector< db::Order > > storage_;
    std::map< std::string, std::vector< db::Order > > activeStorage_;

    // Helper methods
    std::string makeKey(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    // Deleted to enforce Singleton
    OrderRepository(const OrderRepository&)            = delete;
    OrderRepository& operator=(const OrderRepository&) = delete;
};

} // namespace order
} // namespace ct

#endif // CT_ORDER_HPP
