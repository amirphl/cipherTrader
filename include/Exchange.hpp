// Exchange.hpp
#ifndef CIPHER_EXCHANGE_HPP
#define CIPHER_EXCHANGE_HPP

#include "DB.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Timeframe.hpp"

namespace ct
{
namespace exchange
{

class Exchange
{
   public:
    Exchange(const enums::ExchangeName& name,
             double starting_balance,
             double fee_rate,
             const enums::ExchangeType& exchange_type);

    // Properties
    enums::ExchangeName getName() const { return name_; }
    double getStartingBalance() const { return starting_balance_; }
    double getFeeRate() const { return fee_rate_; }
    enums::ExchangeType getExchangeType() const { return exchange_type_; }
    std::string getSettlementCurrency() const { return settlement_currency_; }
    nlohmann::json getVars() const { return vars_; }

    // Virtual methods to be implemented by derived classes
    virtual double getStartedBalance() const            = 0;
    virtual double getWalletBalance() const             = 0;
    virtual double getAvailableMargin() const           = 0;
    virtual enums::LeverageMode getLeverageMode() const = 0;

    virtual void addRealizedPnl(double realized_pnl)                                     = 0;
    virtual void chargeFee(double amount)                                                = 0;
    virtual void increateAssetTempReducedAmount(const std::string& asset, double amount) = 0;

    virtual void onOrderSubmission(const db::Order& order)   = 0;
    virtual void onOrderExecution(const db::Order& order)    = 0;
    virtual void onOrderCancellation(const db::Order& order) = 0;

    // Asset-Balance management
    double getAsset(const std::string& asset) const;
    void setAsset(const std::string& asset, double balance);
    const std::unordered_map< std::string, double >& getAssets() const { return assets_; }
    const std::unordered_map< std::string, double >& getStartingAssets() const { return starting_assets_; }

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
    virtual std::shared_ptr< db::Order > marketOrder(
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
    virtual std::shared_ptr< db::Order > limitOrder(
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
    virtual std::shared_ptr< db::Order > stopOrder(
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

    enums::ExchangeName name_;
    // in running session's quote currency
    double starting_balance_;
    double fee_rate_;
    enums::ExchangeType exchange_type_;
    std::string settlement_currency_;
    nlohmann::json vars_;

    // currently holding assets
    std::unordered_map< std::string, double > assets_;
    // used for calculating available balance in futures mode
    std::unordered_map< std::string, double > temp_reduced_amount_;
    // used for calculating final performance metrics
    std::unordered_map< std::string, double > starting_assets_;
    // current available assets (dynamically changes based on active orders)
    std::unordered_map< std::string, double > available_assets_;

    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > buy_orders_;
    std::unordered_map< std::string, std::shared_ptr< datastructure::DynamicBlazeArray< double > > > sell_orders_;
};

// NOTE: Check proper locking procedure.
class SpotExchange : public Exchange
{
   public:
    SpotExchange(const enums::ExchangeName& name, double starting_balance, double fee_rate);
    ~SpotExchange() override = default;

    // Override base class methods
    double getStartedBalance() const override;
    double getWalletBalance() const override;
    double getAvailableMargin() const override;


    enums::LeverageMode getLeverageMode() const override
    {
        throw std::runtime_error("Leverage is not supported on a spot exchange.");
    };

    void addRealizedPnl(double realized_pnl) override;
    void chargeFee(double amount) override;
    void increateAssetTempReducedAmount(const std::string& asset, double amount) override;

    void onOrderSubmission(const db::Order& order) override;
    void onOrderExecution(const db::Order& order) override;
    void onOrderCancellation(const db::Order& order) override;

    // Live trading specific methods
    void onUpdateFromStream(const nlohmann::json& data);

    // Order placement methods (required by base class)
    std::shared_ptr< db::Order > marketOrder(const std::string& symbol,
                                             double qty,
                                             double current_price,
                                             const std::string& side,
                                             bool reduce_only) override;

    std::shared_ptr< db::Order > limitOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) override;

    std::shared_ptr< db::Order > stopOrder(
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

// NOTE: Check proper locking procedure.
class FuturesExchange : public Exchange
{
   public:
    FuturesExchange(const enums::ExchangeName& name,
                    double starting_balance,
                    double fee_rate,
                    const enums::LeverageMode& futures_leverage_mode,
                    int futures_leverage);

    ~FuturesExchange() override = default;

    // Override base class methods
    double getStartedBalance() const override;
    double getWalletBalance() const override;
    double getAvailableMargin() const override;

    enums::LeverageMode getLeverageMode() const override { return futures_leverage_mode_; };

    void addRealizedPnl(double realized_pnl) override;
    void chargeFee(double amount) override;
    void increateAssetTempReducedAmount(const std::string& asset, double amount) override;

    void onOrderSubmission(const db::Order& order) override;
    void onOrderExecution(const db::Order& order) override;
    void onOrderCancellation(const db::Order& order) override;

    // Live trading specific methods
    void onUpdateFromStream(const nlohmann::json& data);

    // Order placement methods (required by base class)
    std::shared_ptr< db::Order > marketOrder(const std::string& symbol,
                                             double qty,
                                             double current_price,
                                             const std::string& side,
                                             bool reduce_only) override;

    std::shared_ptr< db::Order > limitOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) override;

    std::shared_ptr< db::Order > stopOrder(
        const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only) override;

    void cancelAllOrders(const std::string& symbol) override;

    void cancelOrder(const std::string& symbol, const std::string& order_id) override;

    void fetchPrecisions() override;

   private:
    enums::LeverageMode futures_leverage_mode_;
    int futures_leverage_;

    // For live trading
    // in futures trading, margin is only with one asset, so:
    double available_margin_ = 0.0;
    // in futures trading, wallet is only with one asset, so:
    double wallet_balance_  = 0.0;
    double started_balance_ = 0.0;

    mutable std::mutex mutex_;
};

class ExchangeData
{
   public:
    ExchangeData(const std::string& name,
                 const std::string& url,
                 double fee,
                 enums::ExchangeType type,
                 const std::vector< enums::LeverageMode >& supported_leverage_modes,
                 const std::vector< timeframe::Timeframe >& supported_timeframes,
                 const std::unordered_map< std::string, bool >& modes,
                 const std::string& required_live_plan,
                 const std::string& settlement_currency = "USDT")
        : name_(name)
        , url_(url)
        , fee_(fee)
        , exchange_type_(type)
        , supported_leverage_modes_(supported_leverage_modes)
        , supported_timeframes_(supported_timeframes)
        , modes_(modes)
        , required_live_plan_(required_live_plan)
        , settlement_currency_(settlement_currency)
    {
    }

    // Getters
    const std::string& getName() const { return name_; }
    const std::string& getUrl() const { return url_; }
    double getFee() const { return fee_; }
    enums::ExchangeType getExchangeType() const { return exchange_type_; }
    const std::vector< enums::LeverageMode >& getSupportedLeverageModes() const { return supported_leverage_modes_; }
    const std::vector< timeframe::Timeframe >& getSupportedTimeframes() const { return supported_timeframes_; }
    const std::unordered_map< std::string, bool >& getModes() const { return modes_; }
    const std::string& getRequiredLivePlan() const { return required_live_plan_; }
    const std::string& getSettlementCurrency() const { return settlement_currency_; }

    // Convenience methods
    bool supportsBacktesting() const
    {
        auto it = modes_.find("backtesting");
        return it != modes_.end() && it->second;
    }

    bool supportsLiveTrading() const
    {
        auto it = modes_.find("live_trading");
        return it != modes_.end() && it->second;
    }

    bool supportsLeverageMode(enums::LeverageMode mode) const
    {
        return std::find(supported_leverage_modes_.begin(), supported_leverage_modes_.end(), mode) !=
               supported_leverage_modes_.end();
    }

    bool supportsTimeframe(timeframe::Timeframe timeframe) const
    {
        return std::find(supported_timeframes_.begin(), supported_timeframes_.end(), timeframe) !=
               supported_timeframes_.end();
    }

   private:
    std::string name_;
    std::string url_;
    double fee_;
    enums::ExchangeType exchange_type_;
    std::vector< enums::LeverageMode > supported_leverage_modes_;
    std::vector< timeframe::Timeframe > supported_timeframes_;
    std::unordered_map< std::string, bool > modes_;
    std::string required_live_plan_;
    std::string settlement_currency_;
};

/**
 * @brief Singleton state for managing exchange instances
 *
 * This class is responsible for creating and storing exchange instances
 * based on configuration settings. It follows the singleton pattern to
 * ensure only one instance exists throughout the application.
 */
class ExchangesState
{
   public:
    /**
     * @brief Get the singleton instance of ExchangesState
     *
     * @return ExchangesState& Reference to the singleton instance
     */
    static ExchangesState& getInstance();

    /**
     * @brief Initialize the repository with exchanges from configuration
     */
    void init();

    /**
     * @brief Reset the repository, clearing all exchanges
     */
    void reset();

    /**
     * @brief Get an exchange by name
     *
     * @param exchange_name The name of the exchange
     * @return std::shared_ptr<Exchange> Pointer to the exchange instance
     * @throws std::runtime_error if the exchange doesn't exist
     */
    std::shared_ptr< Exchange > getExchange(const enums::ExchangeName& exchange_name) const;

    /**
     * @brief Check if an exchange exists in the repository
     *
     * @param exchange_name The name of the exchange
     * @return bool True if the exchange exists, false otherwise
     */
    bool hasExchange(const enums::ExchangeName& exchange_name) const;

    // Deleted to enforce Singleton
    ExchangesState(const ExchangesState&)            = delete;
    ExchangesState& operator=(const ExchangesState&) = delete;

   private:
    // Private constructor for Singleton
    ExchangesState() = default;

    // Helper method to get exchange type from config
    enums::ExchangeType getExchangeType(const enums::ExchangeName& exchange_name) const;

    // Storage for exchange instances
    std::unordered_map< enums::ExchangeName, std::shared_ptr< Exchange > > storage_;

    // Mutex for thread safety
    mutable std::mutex mutex_;
};

extern const std::unordered_map< enums::ExchangeName, exchange::ExchangeData > EXCHANGES_DATA;
// TODO: Write tests.
extern const exchange::ExchangeData getExchangeData(const enums::ExchangeName& exchangeName);

extern std::vector< std::string > getExchangesByMode(const std::string& mode);

extern const std::vector< std::string > BACKTESTING_EXCHANGES;
extern const std::vector< std::string > LIVE_TRADING_EXCHANGES;

extern std::string getAppCurrency();

} // namespace exchange
} // namespace ct

#endif // CIPHER_EXCHANGE_HPP
