
#include "Position.hpp"
#include <any>
#include <csignal>
#include <optional>
#include <string>
#include <unordered_map>
#include "DB.hpp"
#include "Enum.hpp"
#include "Exception.hpp"
#include <blaze/Math.h>
#include <boost/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>

ct::position::Position::Position(const enums::ExchangeName& exchange_name,
                                 const std::string& symbol,
                                 const std::unordered_map< std::string, std::any >& attributes)
    : id_(boost::uuids::random_generator()()), exchange_name_(exchange_name), symbol_(symbol)
{
    // TODO:
    // self.exchange: Exchange = selectors.get_exchange(self.exchange_name)

    // Process attributes if provided
    for (const auto& [key, value] : attributes)
    {
        try
        {
            if (key == "id" && value.type() == typeid(std::string))
            {
                id_ = boost::uuids::string_generator()(std::any_cast< std::string >(value));
            }
            else if (key == "entry_price" && value.type() == typeid(double))
            {
                entry_price_ = std::any_cast< double >(value);
            }
            else if (key == "exit_price" && value.type() == typeid(double))
            {
                exit_price_ = std::any_cast< double >(value);
            }
            else if (key == "current_price" && value.type() == typeid(double))
            {
                current_price_ = std::any_cast< double >(value);
            }
            else if (key == "qty" && value.type() == typeid(double))
            {
                qty_ = std::any_cast< double >(value);
            }
            else if (key == "previous_qty" && value.type() == typeid(double))
            {
                previous_qty_ = std::any_cast< double >(value);
            }
            else if (key == "opened_at" && value.type() == typeid(int64_t))
            {
                opened_at_ = std::any_cast< int64_t >(value);
            }
            else if (key == "closed_at" && value.type() == typeid(int64_t))
            {
                closed_at_ = std::any_cast< int64_t >(value);
            }
            else if (key == "mark_price" && value.type() == typeid(double))
            {
                mark_price_ = std::any_cast< double >(value);
            }
            else if (key == "funding_rate" && value.type() == typeid(double))
            {
                funding_rate_ = std::any_cast< double >(value);
            }
            else if (key == "next_funding_timestamp" && value.type() == typeid(int64_t))
            {
                next_funding_timestamp_ = std::any_cast< int64_t >(value);
            }
            else if (key == "liquidation_price" && value.type() == typeid(double))
            {
                liquidation_price_ = std::any_cast< double >(value);
            }
        }
        catch (const std::bad_any_cast& e)
        {
            throw std::runtime_error("Error initializing Position with attribute " + key + ": " + e.what());
        }
    }
}

std::string ct::position::Position::getIdAsString() const
{
    return boost::uuids::to_string(id_);
}

std::optional< double > ct::position::Position::getMarkPrice() const
{
    if (!helper::isLive())
    {
        return current_price_;
    }

    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        return current_price_;
    }

    return mark_price_;
}

std::optional< double > ct::position::Position::getFundingRate() const
{
    if (!helper::isLiveTrading())
    {
        return 0.0;
    }

    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        throw std::runtime_error("Funding rate is not applicable to spot trading");
    }

    return funding_rate_;
}

std::optional< int64_t > ct::position::Position::getNextFundingTimestamp() const
{
    if (!helper::isLiveTrading())
    {
        return std::nullopt;
    }

    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        throw std::runtime_error("Funding rate is not applicable to spot trading");
    }

    return next_funding_timestamp_;
}

std::optional< double > ct::position::Position::getLiquidationPrice() const
{
    if (isClose())
    {
        return std::nanf("");
    }

    if (helper::isLiveTrading())
    {
        return liquidation_price_;
    }
    auto mode = getLeverageMode();

    if (!mode.has_value() || mode.value() == enums::LeverageMode::CROSS)
    {
        return std::nanf("");
    }
    else if (mode.value() == enums::LeverageMode::ISOLATED)
    {
        if (getPositionType() == enums::PositionType::LONG)
        {
            return entry_price_.value_or(0.0) * (1 - getInitialMarginRate() + 0.004);
        }
        else if (getPositionType() == enums::PositionType::SHORT)
        {
            return entry_price_.value_or(0.0) * (1 + getInitialMarginRate() - 0.004);
        }
        else
        {
            return std::nanf("");
        }
    }
    else
    {
        throw std::runtime_error("Unknown leverage mode");
    }
}

// The value of open position in the quote currency
double ct::position::Position::getValue() const
{
    if (isClose())
    {
        return 0.0;
    }

    if (!current_price_.has_value())
    {
        return std::nanf("");
    }

    return std::abs(current_price_.value() * qty_);
}

ct::enums::PositionType ct::position::Position::getPositionType() const
{
    if (isLong())
    {
        return enums::PositionType::LONG;
    }
    else if (isShort())
    {
        return enums::PositionType::SHORT;
    }

    return enums::PositionType::CLOSE;
}

double ct::position::Position::getRoi() const
{
    if (getPnl() == 0)
    {
        return 0.0;
    }

    return getPnl() / getTotalCost() * 100;
}

// How much we paid to open this position (currently does not include fees, should we?!)
double ct::position::Position::getTotalCost() const
{
    if (isClose())
    {
        return std::nanf("");
    }

    double base_cost = entry_price_.value_or(0.0) * std::abs(qty_);

    if (strategy_)
    {
        return base_cost / getLeverage();
    }

    return base_cost;
}

double ct::position::Position::getLeverage() const
{
    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        return 1.0;
    }

    // TODO: Implement strategy reference
    // if (strategy_) {
    //     return strategy_->getLeverage();
    // }

    return std::nanf("");
}

double ct::position::Position::getPnl() const
{
    if (std::abs(qty_) < getMinQty())
    {
        return 0.0;
    }

    if (!entry_price_.has_value())
    {
        return 0.0;
    }

    if (getValue() == 0 || std::isnan(getValue()))
    {
        return 0.0;
    }

    double diff = getValue() - std::abs(entry_price_.value() * qty_);

    return (getPositionType() == enums::PositionType::SHORT) ? -diff : diff;
}

double ct::position::Position::getBankruptcyPrice() const
{
    if (getPositionType() == enums::PositionType::LONG)
    {
        return entry_price_.value_or(0.0) * (1 - getInitialMarginRate());
    }
    else if (getPositionType() == enums::PositionType::SHORT)
    {
        return entry_price_.value_or(0.0) * (1 + getInitialMarginRate());
    }
    else
    {
        return std::nanf("");
    }
}

std::optional< ct::enums::LeverageMode > ct::position::Position::getLeverageMode() const
{
    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        return std::nullopt;
    }

    return exchange_->getLeverageMode();
}

bool ct::position::Position::isOpen() const
{
    return getPositionType() == enums::PositionType::LONG || getPositionType() == enums::PositionType::SHORT;
}

bool ct::position::Position::isClose() const
{
    return getPositionType() == enums::PositionType::CLOSE;
}

bool ct::position::Position::isLong() const
{
    return qty_ > getMinQty();
}

bool ct::position::Position::isShort() const
{
    return qty_ < -std::abs(getMinQty());
}

void ct::position::Position::close(double close_price)
{
    if (isClose() && canMutateQty())
    {
        throw exception::EmptyPosition("The position is already closed.");
    }

    exit_price_ = close_price;
    closed_at_  = helper::nowToTimestamp();

    // Handle futures exchange PNL
    if (exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
    {
        // Just to prevent confusion
        double close_qty = std::abs(qty_);
        double estimated_profit =
            helper::estimatePNL(close_qty, entry_price_.value_or(0.0), close_price, getPositionType());
        std::string base_asset = helper::getBaseAsset(symbol_);

        exchange_->addRealizedPnl(estimated_profit);
        exchange_->increateAssetTempReducedAmount(base_asset, std::abs(close_qty * close_price));
    }

    if (canMutateQty())
    {
        updateQty(0, Operation::SET);
    }

    entry_price_.reset();

    close();
}

void ct::position::Position::reduce(double qty, double price)
{
    if (!canMutateQty())
    {
        return;
    }

    if (!isOpen())
    {
        throw exception::EmptyPosition("The position is closed.");
    }

    // Just to prevent confusion
    qty = std::abs(qty);

    double estimated_profit = helper::estimatePNL(qty, entry_price_.value_or(0.0), price, getPositionType());

    if (exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
    {
        std::string base_asset = helper::getBaseAsset(symbol_);

        // self.exchange.increase_futures_balance(qty * self.entry_price + estimated_profit)
        exchange_->addRealizedPnl(estimated_profit);
        exchange_->increateAssetTempReducedAmount(base_asset, std::abs(qty * price));
    }
    if (getPositionType() == enums::PositionType::LONG)
    {
        updateQty(qty, Operation::SUBTRACT);
    }
    else if (getPositionType() == enums::PositionType::SHORT)
    {
        updateQty(qty, Operation::ADD);
    }
}

void ct::position::Position::increase(double qty, double price)
{
    if (!isOpen())
    {
        throw exception::OpenPositionError("Position must be already open in order to increase its size");
    }

    qty = std::abs(qty);

    // Calculate new average entry price
    entry_price_ = helper::estimateAveragePrice(qty, price, qty_, entry_price_.value_or(0.0));

    if (canMutateQty())
    {
        if (getPositionType() == enums::PositionType::LONG)
        {
            updateQty(qty, Operation::ADD);
        }
        else if (getPositionType() == enums::PositionType::SHORT)
        {
            updateQty(qty, Operation::SUBTRACT);
        }
    }
}

void ct::position::Position::open(double qty, double price)
{
    if (isOpen() && canMutateQty())
    {
        throw exception::OpenPositionError("An already open position cannot be opened");
    }

    entry_price_ = price;
    exit_price_.reset();

    if (canMutateQty())
    {
        updateQty(qty, Operation::SET);
    }

    opened_at_ = helper::nowToTimestamp();

    open();
}

void ct::position::Position::onExecutedOrder(const db::Order& order)
{
    // For futures (live)
    if (helper::isLiveTrading() && exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
    {
        // If position got closed because of this order
        double before_qty;
        if (order.isPartiallyFilled())
        {
            before_qty = qty_ - order.getFilledQty();
        }
        else
        {
            before_qty = qty_ - order.getQty();
        }
        double after_qty = qty_;

        if (before_qty != 0 && after_qty == 0)
        {
            close();
        }
    }
    // For spot (live)
    else if (helper::isLiveTrading() && exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        // If position got closed because of this order
        double before_qty = previous_qty_;
        double after_qty  = qty_;
        double qty        = order.getQty();
        double price      = order.getPrice().value_or(0.0);

        bool closing_position = before_qty > getMinQty() && after_qty <= getMinQty();
        if (closing_position)
        {
            close(price);
        }

        bool opening_position = before_qty < getMinQty() && after_qty > getMinQty();
        if (opening_position)
        {
            open(qty, price);
        }

        bool increasing_position = after_qty > before_qty && before_qty > getMinQty();
        if (increasing_position)
        {
            increase(qty, price);
        }

        bool reducing_position = after_qty < before_qty && after_qty > getMinQty();
        if (reducing_position)
        {
            reduce(qty, price);
        }
    }
    else
    { // Backtest (both futures and spot)
        double qty   = order.getQty();
        double price = order.getPrice().value_or(0.0);

        if (exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
        {
            exchange_->chargeFee(qty * price);
        }

        // Order opens position
        if (qty_ == 0)
        {
            open(qty, price);
        }
        // Order closes position
        else if ((helper::addFloatsMaintainPrecesion(qty_, qty)) == 0)
        {
            close(price);
        }
        // Order increases the size of the position
        else if (qty_ * qty > 0)
        {
            if (order.isReduceOnly())
            {
                logger::LOG.info("Did not increase position because order is a reduce_only order");
            }
            else
            {
                increase(qty, price);
            }
        }
        // Order reduces the size of the position
        else if (qty_ * qty < 0)
        {
            // If size of the order is big enough to both close the
            // position AND open it on the opposite side
            if (std::abs(qty) > std::abs(qty_))
            {
                if (order.isReduceOnly())
                {
                    logger::LOG.info("Executed order is bigger than the current position size but it is a reduce_only "
                                     "order so it just closes it. Order QTY: " +
                                     std::to_string(qty) + ", Position QTY: " + std::to_string(qty_));
                    close(price);
                }
                else
                {
                    logger::LOG.info(
                        "Executed order is big enough to not close, but flip the position type. Order QTY: " +
                        std::to_string(qty) + ", Position QTY: " + std::to_string(qty_));
                    double diff_qty = helper::addFloatsMaintainPrecesion(qty_, qty);
                    close(price);
                    open(diff_qty, price);
                }
            }
            else
            {
                reduce(qty, price);
            }
        }

        // TODO: Implement strategy reference
        // if (strategy_) {
        //     strategy_->onUpdatedPosition(order);
        // }
    }
}

// Used for updating the position from the WS stream (only for live trading)
void ct::position::Position::onUpdateFromStream(const nlohmann::json& data, bool is_initial)
{
    double before_qty = std::abs(qty_);
    double after_qty  = std::abs(data["qty"].get< double >());

    if (exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
    {
        entry_price_       = data["entry_price"].get< double >();
        liquidation_price_ = data["liquidation_price"].get< double >();
    }
    else
    { // spot
        if (after_qty > getMinQty() && !entry_price_.has_value())
        {
            entry_price_ = current_price_;
        }
    }

    // If the new qty (data['qty']) is different than the current (qty_) then update it:
    if (qty_ != data["qty"].get< double >())
    {
        previous_qty_ = qty_;
        qty_          = data["qty"].get< double >();
    }

    bool opening_position = before_qty <= getMinQty() && after_qty > getMinQty();
    bool closing_position = before_qty > getMinQty() && after_qty <= getMinQty();

    if (opening_position)
    {
        if (is_initial)
        {
            // TODO: Implement completed_trades reference
            // store.completed_trades.addOrderRecordOnly(
            //     exchange_name_, symbol_, helper::typeToSide(getType()),
            //     qty_, entry_price_.value_or(0.0)
            // );
        }
        opened_at_ = helper::nowToTimestamp();
        open();
    }
    else if (closing_position)
    {
        closed_at_ = helper::nowToTimestamp();
    }
}

nlohmann::json ct::position::Position::toJson() const
{
    nlohmann::json result;

    if (entry_price_.has_value())
    {
        result["entry_price"] = entry_price_.value();
    }
    else
    {
        result["entry_price"] = nullptr;
    }

    result["qty"] = qty_;

    if (current_price_.has_value())
    {
        result["current_price"] = current_price_.value();
    }
    else
    {
        result["current_price"] = nullptr;
    }

    result["value"]          = getValue();
    result["position_type"]  = getPositionType();
    result["exchange_name"]  = exchange_name_;
    result["pnl"]            = getPnl();
    result["pnl_percentage"] = getPnlPercentage();
    result["leverage"]       = getLeverage();

    if (getLiquidationPrice().has_value() && !std::isnan(getLiquidationPrice().value()))
    {
        result["liquidation_price"] = getLiquidationPrice().value();
    }
    else
    {
        result["liquidation_price"] = nullptr;
    }

    if (!std::isnan(getBankruptcyPrice()))
    {
        result["bankruptcy_price"] = getBankruptcyPrice();
    }
    else
    {
        result["bankruptcy_price"] = nullptr;
    }

    if (getLeverageMode().has_value())
    {
        result["modes"] = getLeverageMode().value();
    }
    else
    {
        result["modes"] = nullptr;
    }

    return result;
}

void ct::position::Position::close()
{
    // TODO: Implement completed_trades reference
    // store.completed_trades.closeTrade(*this);
}

void ct::position::Position::open()
{
    // TODO: Implement completed_trades reference
    // store.completed_trades.openTrade(*this);
}

void ct::position::Position::updateQty(double qty, const Operation& op)
{
    previous_qty_ = qty_;

    if (exchange_->getExchangeType() == enums::ExchangeType::SPOT)
    {
        if (op == Operation::SET)
        {
            qty_ = qty * (1 - exchange_->getFeeRate());
        }
        else if (op == Operation::ADD)
        {
            qty_ = helper::addFloatsMaintainPrecesion(qty_, qty * (1 - exchange_->getFeeRate()));
        }
        else if (op == Operation::SUBTRACT)
        {
            // Fees are taken from the quote currency in spot mode
            // fees are taken from the quote currency. in spot mode, sell orders cause
            // the qty to reduce but fees are handled on the exchange balance stuff
            qty_ = helper::subtractFloatsMaintainPrecesion(qty_, qty);
        }
    }
    else if (exchange_->getExchangeType() == enums::ExchangeType::FUTURES)
    {
        if (op == Operation::SET)
        {
            qty_ = qty;
        }
        else if (op == Operation::ADD)
        {
            qty_ = helper::addFloatsMaintainPrecesion(qty_, qty);
        }
        else if (op == Operation::SUBTRACT)
        {
            qty_ = helper::subtractFloatsMaintainPrecesion(qty_, qty);
        }
    }
    else
    {
        throw std::runtime_error("Exchange type not implemented");
    }
}

double ct::position::Position::getMinQty() const
{
    if (!(helper::isLiveTrading() && exchange_->getExchangeType() == enums::ExchangeType::SPOT))
    {
        return 0.0;
    }

    // First check if exchange returns min_qty directly
    try
    {
        if (exchange_->getVars()["precisions"][symbol_].contains("min_qty"))
        {
            return exchange_->getVars()["precisions"][symbol_]["min_qty"].get< double >();
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        // Handle json parsing errors
        ct::logger::LOG.error("Error accessing min_qty: " + std::string(e.what()));
    }

    // If min_qty is not directly available, calculate from min_notional_size
    double min_notional = getMinNotionalSize();
    if (min_notional > 0.0 && current_price_.has_value())
    {
        return min_notional / current_price_.value();
    }

    return 0.0;
}

bool ct::position::Position::canMutateQty() const
{
    return !(exchange_->getExchangeType() == enums::ExchangeType::SPOT && helper::isLiveTrading());
}

double ct::position::Position::getInitialMarginRate() const
{
    return 1.0 / getLeverage();
}

double ct::position::Position::getMinNotionalSize() const
{
    if (!(helper::isLiveTrading() && exchange_->getExchangeType() == enums::ExchangeType::SPOT))
    {
        return 0.0;
    }

    try
    {
        if (exchange_->getVars().contains("precisions") && exchange_->getVars()["precisions"].contains(symbol_) &&
            exchange_->getVars()["precisions"][symbol_].contains("min_notional_size"))
        {
            return exchange_->getVars()["precisions"][symbol_]["min_notional_size"].get< double >();
        }
        else
        {
            // Return 0 if the key doesn't exist in the JSON
            return 0.0;
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        // Handle json parsing errors
        ct::logger::LOG.error("Error accessing min_notional_size: " + std::string(e.what()));
        return 0.0;
    }
}
