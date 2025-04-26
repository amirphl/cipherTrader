// Exchange.hpp
#ifndef CIPHER_EXCHANGE_HPP
#define CIPHER_EXCHANGE_HPP

#include <memory>
#include <string>
#include "DB.hpp"
#include "Enum.hpp"

namespace ct
{
namespace exchange
{

class Exchange
{
   public:
    Exchange(const std::string& name,
             double starting_balance,
             double fee_rate,
             const ct::enums::ExchangeType& exchange_type);

    // Properties
    std::string getName() const { return name_; }
    double getStartingBalance() const { return starting_balance_; }
    double getFeeRate() const { return fee_rate_; }
    ct::enums::ExchangeType getExchangeType() const { return exchange_type_; }
    std::string getSettlementCurrency() const { return settlement_currency_; }

    // Virtual methods to be implemented by derived classes
    virtual double getStartedBalance() const  = 0;
    virtual double getWalletBalance() const   = 0;
    virtual double getAvailableMargin() const = 0;

    virtual void onOrderSubmission(const ct::db::Order& order)   = 0;
    virtual void onOrderExecution(const ct::db::Order& order)    = 0;
    virtual void onOrderCancellation(const ct::db::Order& order) = 0;

    // Token-Balance management
    double getTokenBalance(const std::string& token) const;
    void setTokenBalance(const std::string& token, double balance);
    const std::unordered_map< std::string, double >& getTokenBalances() const { return token_balances_; }
    const std::unordered_map< std::string, double >& getStartingTokenBalances() const
    {
        return starting_token_balances_;
    }

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

    std::string name_;
    double starting_balance_;
    double fee_rate_;
    enums::ExchangeType exchange_type_;
    std::string settlement_currency_;
    std::unordered_map< std::string, double > token_balances_;
    std::unordered_map< std::string, double > starting_token_balances_;
};

class SpotExchange : public Exchange
{
   public:
    SpotExchange(const std::string& name, double starting_balance, double fee_rate);
    ~SpotExchange() override = default;

    // Override base class methods
    double getStartedBalance() const override;
    double getWalletBalance() const override;
    double getAvailableMargin() const override;

    void onOrderSubmission(const ct::db::Order& order) override;
    void onOrderExecution(const ct::db::Order& order) override;
    void onOrderCancellation(const ct::db::Order& order) override;

    // Live trading specific methods
    void onUpdateFromStream(const nlohmann::json& data);

    std::shared_ptr< ct::db::Order > marketOrder(const std::string& symbol,
                                                 double qty,
                                                 double current_price,
                                                 const std::string& side,
                                                 bool reduce_only) override;

    std::shared_ptr< ct::db::Order > limitOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) override;

    std::shared_ptr< ct::db::Order > stopOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) override;

    void cancelAllOrders(const std::string& symbol) override;

    void cancelOrder(const std::string& symbol, const std::string& order_id) override;

    void fetchPrecisions() override;

   private:
    std::unordered_map< std::string, double > stop_sell_orders_qty_sum_;
    std::unordered_map< std::string, double > limit_sell_orders_qty_sum_;

    double started_balance_ = 0.0;
    mutable std::mutex mutex_;
};

} // namespace exchange
} // namespace ct

#endif // CIPHER_EXCHANGE_HPP
