#ifndef CT_ORDER_HPP
#define CT_ORDER_HPP

#include "DB.hpp"

namespace ct
{
namespace order
{

class OrdersState
{
   public:
    // Singleton access
    static OrdersState& getInstance();

    // Constructor
    OrdersState();

    // Reset methods
    void reset();
    void resetTradeOrders(const enums::ExchangeName& exchange_name, const std::string& symbol);

    // Order management
    void addOrder(const std::shared_ptr< db::Order > order);
    void removeOrder(const std::shared_ptr< db::Order > order);
    void executePendingMarketOrders();

    // Getters
    std::vector< std::shared_ptr< db::Order > > getOrders(const enums::ExchangeName& exchange_name,
                                                          const std::string& symbol) const;
    std::vector< std::shared_ptr< db::Order > > getActiveOrders(const enums::ExchangeName& exchange_name,
                                                                const std::string& symbol) const;
    std::vector< std::shared_ptr< db::Order > > getOrders(const enums::ExchangeName& exchange_name) const;

    int countActiveOrders() const;
    int countActiveOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;
    int countOrders(const enums::ExchangeName& exchange_name, const std::string& symbol) const;

    std::shared_ptr< db::Order > getOrderById(const enums::ExchangeName& exchange_name,
                                              const std::string& symbol,
                                              const std::string& id,
                                              bool use_exchange_id = false) const;

    // return all orders if position is not opened yet
    std::vector< std::shared_ptr< db::Order > > getEntryOrders(const enums::ExchangeName& exchange_name,
                                                               const std::string& symbol) const;
    std::vector< std::shared_ptr< db::Order > > getExitOrders(const enums::ExchangeName& exchange_name,
                                                              const std::string& symbol) const;
    std::vector< std::shared_ptr< db::Order > > getActiveExitOrders(const enums::ExchangeName& exchange_name,
                                                                    const std::string& symbol) const;

   private:
    // Used in simulation only
    std::vector< std::shared_ptr< db::Order > > to_execute_;

    // Storage maps
    std::map< std::string, std::vector< std::shared_ptr< db::Order > > > storage_;
    std::map< std::string, std::vector< std::shared_ptr< db::Order > > > active_storage_;

    // Deleted to enforce Singleton
    OrdersState(const OrdersState&)            = delete;
    OrdersState& operator=(const OrdersState&) = delete;
};

} // namespace order
} // namespace ct

#endif // CT_ORDER_HPP
