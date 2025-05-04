
#ifndef CIPHER_POSITION_HPP
#define CIPHER_POSITION_HPP

#include "Precompiled.hpp"

#include "DB.hpp"
#include "Enum.hpp"
#include "Exchange.hpp"

namespace ct
{
namespace position
{

enum class Operation
{
    SET,
    ADD,
    SUBTRACT,
};

class Position
{
   public:
    // Constructors
    Position(const enums::ExchangeName& exchange_name,
             const std::string& symbol,
             const std::unordered_map< std::string, std::any >& attributes = {});

    // Rule of five
    Position(const Position&)                = default;
    Position(Position&&) noexcept            = default;
    Position& operator=(const Position&)     = default;
    Position& operator=(Position&&) noexcept = default;
    ~Position()                              = default;

    // Getters and setters
    const boost::uuids::uuid& getId() const { return id_; }
    std::string getIdAsString() const;

    std::optional< double > getEntryPrice() const { return entry_price_; }
    void setEntryPrice(double price) { entry_price_ = price; }
    void clearEntryPrice() { entry_price_.reset(); }

    std::optional< double > getExitPrice() const { return exit_price_; }
    void setExitPrice(double price) { exit_price_ = price; }
    void clearExitPrice() { exit_price_.reset(); }

    std::optional< double > getCurrentPrice() const { return current_price_; }
    void setCurrentPrice(double price) { current_price_ = price; }
    void clearCurrentPrice() { current_price_.reset(); }

    double getQty() const { return qty_; }
    void setQty(double qty) { qty_ = qty; }

    double getPreviousQty() const { return previous_qty_; }

    std::optional< int64_t > getOpenedAt() const { return opened_at_; }
    void setOpenedAt(int64_t timestamp) { opened_at_ = timestamp; }
    void clearOpenedAt() { opened_at_.reset(); }

    std::optional< int64_t > getClosedAt() const { return closed_at_; }
    void setClosedAt(int64_t timestamp) { closed_at_ = timestamp; }
    void clearClosedAt() { closed_at_.reset(); }

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    // Futures-specific getters and setters
    std::optional< double > getMarkPrice() const;
    void setMarkPrice(double price) { mark_price_ = price; }
    void clearMarkPrice() { mark_price_.reset(); }

    std::optional< double > getFundingRate() const;
    void setFundingRate(double rate) { funding_rate_ = rate; }
    void clearFundingRate() { funding_rate_.reset(); }

    std::optional< int64_t > getNextFundingTimestamp() const;
    void setNextFundingTimestamp(int64_t timestamp) { next_funding_timestamp_ = timestamp; }
    void clearNextFundingTimestamp() { next_funding_timestamp_.reset(); }

    std::optional< double > getLiquidationPrice() const;
    void setLiquidationPrice(double price) { liquidation_price_ = price; }
    void clearLiquidationPrice() { liquidation_price_.reset(); }

    // Calculated properties
    double getValue() const;
    enums::PositionType getPositionType() const;
    double getPnlPercentage() const { return getRoi(); }
    double getRoi() const;
    double getTotalCost() const;
    double getLeverage() const;
    double getEntryMargin() const { return getTotalCost(); }
    double getPnl() const;
    double getBankruptcyPrice() const;
    std::optional< ct::enums::LeverageMode > getLeverageMode() const;

    // State checks
    bool isOpen() const;
    bool isClose() const;
    bool isLong() const;
    bool isShort() const;

    // Position operations
    void close(double close_price);
    void reduce(double qty, double price);
    void increase(double qty, double price);
    void open(double qty, double price);

    // Order execution handling
    void onExecutedOrder(const db::Order& order);

    // Stream updates for live trading
    void onUpdateFromStream(const nlohmann::json& data, bool is_initial);

    // Convert to JSON
    nlohmann::json toJson() const;

   private:
    boost::uuids::uuid id_;
    std::optional< double > entry_price_;
    std::optional< double > exit_price_;
    std::optional< double > current_price_;
    double qty_          = 0.0;
    double previous_qty_ = 0.0;
    std::optional< int64_t > opened_at_;
    std::optional< int64_t > closed_at_;

    // Futures-specific fields
    std::optional< double > mark_price_;
    std::optional< double > funding_rate_;
    std::optional< int64_t > next_funding_timestamp_;
    std::optional< double > liquidation_price_;

    enums::ExchangeName exchange_name_;
    std::shared_ptr< exchange::Exchange > exchange_;
    std::string symbol_;
    std::optional< std::string > strategy_;

    // Helper methods
    void close();
    void open();
    void updateQty(double qty, const Operation& operation = Operation::SET);
    double getMinQty() const;
    bool canMutateQty() const;
    double getInitialMarginRate() const;
    double getMinNotionalSize() const;
};

class PositionsState
{
   public:
    // Singleton access
    static PositionsState& getInstance();

    // Initialize positions state
    void init();

    // Get the number of open positions
    int countOpenPositions() const;

    // Get position by exchange and symbol
    Position& getPosition(const enums::ExchangeName& exchange_name, const std::string& symbol);

   private:
    PositionsState()  = default;
    ~PositionsState() = default;

    // Deleted to enforce Singleton
    PositionsState(const PositionsState&)            = delete;
    PositionsState& operator=(const PositionsState&) = delete;

    std::map< std::string, Position > storage_;
};

} // namespace position
} // namespace ct

#endif
