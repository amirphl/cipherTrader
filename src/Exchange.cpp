
#include "Exchange.hpp"
#include <stdexcept>
#include "Enum.hpp"
#include "Exception.hpp"
#include "Helper.hpp"

// TODO: Rename Token to Currency?

ct::exchange::Exchange::Exchange(const std::string& name,
                                 double starting_balance,
                                 double fee_rate,
                                 const ct::enums::ExchangeType& exchange_type)
    : name_(name)
    , starting_balance_(starting_balance)
    , fee_rate_(fee_rate)
    , exchange_type_(exchange_type)
    , settlement_currency_("USDT") // Default, can be overridden
{
    // Initialize starting tokens and current tokens
    // TODO: Rename asset to token
    starting_token_balances_[settlement_currency_] = starting_balance;
    token_balances_[settlement_currency_]          = starting_balance;
    // TODO:
    // # currently holding assets
    // self.assets = {}
    // # used for calculating available balance in futures mode:
    // self.temp_reduced_amount = {}
    // # used for calculating final performance metrics
    // self.starting_assets = {}
    // # current available assets (dynamically changes based on active orders)
    // self.available_assets = {}
    // # some exchanges might require even further info
    // self.vars = {}

    // self.buy_orders = {}
    // self.sell_orders = {}

    // all_trading_routes = selectors.get_all_trading_routes()
    // first_route = all_trading_routes[0]
    // # check the settlement_currency is in the exchange info with name equal to the exchange name
    // if self.name in exchange_info and 'settlement_currency' in exchange_info[self.name]:
    //     self.settlement_currency = exchange_info[self.name]['settlement_currency']
    // else:
    //     self.settlement_currency = jh.quote_asset(first_route.symbol)

    // # initiate dict keys for trading assets
    // for r in all_trading_routes:
    //     base_asset = jh.base_asset(r.symbol)
    //     self.buy_orders[base_asset] = DynamicNumpyArray((10, 2))
    //     self.sell_orders[base_asset] = DynamicNumpyArray((10, 2))
    //     self.assets[base_asset] = 0.0
    //     self.assets[self.settlement_currency] = starting_balance
    //     self.temp_reduced_amount[base_asset] = 0.0
    //     self.temp_reduced_amount[self.settlement_currency] = 0.0
    //     self.starting_assets[base_asset] = 0.0
    //     self.starting_assets[self.settlement_currency] = starting_balance
    //     self.available_assets[base_asset] = 0.0
    //     self.available_assets[self.settlement_currency] = starting_balance
}

double ct::exchange::Exchange::getTokenBalance(const std::string& token) const
{
    auto it = token_balances_.find(token);
    if (it != token_balances_.end())
    {
        return it->second;
    }
    return 0.0;
}

void ct::exchange::Exchange::setTokenBalance(const std::string& token, double balance)
{
    token_balances_[token] = balance;
}

ct::exchange::SpotExchange::SpotExchange(const std::string& name, double starting_balance, double fee_rate)
    : ct::exchange::Exchange::Exchange(name, starting_balance, fee_rate, enums::ExchangeType::SPOT)
{
    // Initialize the orders sum maps
    stop_sell_orders_qty_sum_  = {};
    limit_sell_orders_qty_sum_ = {};
    // TODO:
    // live-trading only
    // starting_balance_ = 0;
}

double ct::exchange::SpotExchange::getStartedBalance() const
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (ct::helper::isLiveTrading())
    {
        return started_balance_;
    }

    auto it = starting_token_balances_.find(ct::helper::getAppCurrency());
    if (it != starting_token_balances_.end())
    {
        return it->second;
    }
    return 0.0;
}

double ct::exchange::SpotExchange::getWalletBalance() const
{
    std::lock_guard< std::mutex > lock(mutex_);
    return getTokenBalance(settlement_currency_);
}

double ct::exchange::SpotExchange::getAvailableMargin() const
{
    return getWalletBalance();
}

void ct::exchange::SpotExchange::onOrderSubmission(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (ct::helper::isLiveTrading())
    {
        return;
    }

    // Save original state before making any changes
    std::string symbol     = order.getSymbol();
    std::string base_token = ct::helper::baseToken(symbol);

    // Save original balances and order sums
    double original_settlement_balance = token_balances_[settlement_currency_];
    double original_base_balance       = token_balances_[base_token];
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];

    try
    {
        if (order.getSide() == ct::enums::OrderSide::SELL)
        {
            if (order.getType() == ct::enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] =
                    helper::addFloatsMaintainPrecesion(stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getType() == ct::enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] =
                    helper::addFloatsMaintainPrecesion(limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
        }

        if (order.getSide() == ct::enums::OrderSide::BUY)
        {
            // Cannot buy if we don't have enough balance (of the settlement currency)
            token_balances_[settlement_currency_] =
                helper::subtractFloatsMaintainPrecesion(token_balances_[settlement_currency_], order.getValue());

            if (token_balances_[settlement_currency_] < 0)
            {
                // Restore the original balance
                token_balances_[settlement_currency_] = original_settlement_balance;

                auto msg = "InsufficientBalance: Not enough balance. Available balance at " + name_ + " for " +
                           settlement_currency_ + " is " + std::to_string(original_settlement_balance) +
                           " but you're trying to spend " + std::to_string(order.getValue());

                throw ct::exception::InsufficientBalance(msg);
            }
        }
        else
        {
            double base_balance = token_balances_[base_token];

            // Sell order's qty cannot be bigger than the amount of existing base token
            double order_qty = 0.0;

            if (order.getType() == ct::enums::OrderType::MARKET)
            {
                order_qty =
                    helper::addFloatsMaintainPrecesion(std::abs(order.getQty()), limit_sell_orders_qty_sum_[symbol]);
            }
            else if (order.getType() == ct::enums::OrderType::STOP)
            {
                order_qty = stop_sell_orders_qty_sum_[symbol];
            }
            else if (order.getType() == ct::enums::OrderType::LIMIT)
            {
                order_qty = limit_sell_orders_qty_sum_[symbol];
            }
            else
            {
                throw std::runtime_error("Unknown order type " + ct::enums::toString(order.getType()));
            }

            // Validate that the total selling amount is not bigger than the amount of the existing base token
            if (order_qty > base_balance)
            {
                auto msg = "InsufficientBalance: Not enough balance. Available balance at " + name_ + " for " +
                           base_token + " is " + std::to_string(base_balance) + " but you're trying to sell " +
                           std::to_string(order_qty);

                throw ct::exception::InsufficientBalance(msg);
            }
        }
    }
    catch (...)
    {
        // Revert to original state in case of any exception
        token_balances_[settlement_currency_] = original_settlement_balance;
        token_balances_[base_token]           = original_base_balance;
        stop_sell_orders_qty_sum_[symbol]     = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol]    = original_limit_sum;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onOrderExecution(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (ct::helper::isLiveTrading())
    {
        return;
    }

    // Store original state
    std::string symbol     = order.getSymbol();
    std::string base_token = ct::helper::baseToken(symbol);

    // Save original values
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];
    double original_base_balance       = token_balances_[base_token];
    double original_settlement_balance = token_balances_[settlement_currency_];

    try
    {
        if (order.getSide() == ct::enums::OrderSide::SELL)
        {
            if (order.getType() == ct::enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getType() == ct::enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
        }

        // Buy order
        if (order.getSide() == ct::enums::OrderSide::BUY)
        {
            // Token's balance is increased by the amount of the order's qty after fees are deducted
            token_balances_[base_token] = helper::addFloatsMaintainPrecesion(
                token_balances_[base_token], std::abs(order.getQty()) * (1 - fee_rate_));
        }
        // Sell order
        else
        {
            double current_balance = token_balances_[base_token];
            double order_qty;

            if (std::abs(order.getQty()) > current_balance)
            {
                double adjusted_qty = current_balance;
                order_qty           = std::abs(adjusted_qty);
            }
            else
            {
                order_qty = std::abs(order.getQty());
            }

            // Settlement currency's balance is increased by the amount of the order's qty after fees are deducted
            token_balances_[settlement_currency_] = helper::addFloatsMaintainPrecesion(
                token_balances_[settlement_currency_], (order_qty * order.getPrice().value_or(0.0)) * (1 - fee_rate_));

            // Now reduce base token's balance by the amount of the order's qty
            token_balances_[base_token] =
                helper::subtractFloatsMaintainPrecesion(token_balances_[base_token], order_qty);
        }
    }
    catch (...)
    {
        // Restore original state in case of any exception
        stop_sell_orders_qty_sum_[symbol]     = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol]    = original_limit_sum;
        token_balances_[base_token]           = original_base_balance;
        token_balances_[settlement_currency_] = original_settlement_balance;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onOrderCancellation(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (ct::helper::isLiveTrading())
    {
        return;
    }

    // Store original state
    std::string symbol     = order.getSymbol();
    std::string base_token = ct::helper::baseToken(symbol);

    // Save original values
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];
    double original_settlement_balance = token_balances_[settlement_currency_];

    // FIXME: Duplicate subtraction?
    // if (order.getSide() == ct::enums::OrderSide::SELL)
    // {
    //     if (order.getType() == ct::enums::OrderType::STOP)
    //     {
    //         stop_sell_orders_qty_sum_[order.getSymbol()] = helper::subtractFloatsMaintainPrecesion(
    //             stop_sell_orders_qty_sum_[order.getSymbol()], std::abs(order.getQty()));
    //     }
    //     else if (order.getType() == ct::enums::OrderType::LIMIT)
    //     {
    //         limit_sell_orders_qty_sum_[order.getSymbol()] = helper::subtractFloatsMaintainPrecesion(
    //             limit_sell_orders_qty_sum_[order.getSymbol()], std::abs(order.getQty()));
    //     }
    // }

    try
    {
        // Buy order
        if (order.getSide() == ct::enums::OrderSide::BUY)
        {
            token_balances_[settlement_currency_] =
                helper::addFloatsMaintainPrecesion(token_balances_[settlement_currency_], std::abs(order.getValue()));
        }
        // Sell order
        else
        {
            if (order.getType() == ct::enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getType() == ct::enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
        }
    }
    catch (...)
    {
        // Restore original state in case of any exception
        stop_sell_orders_qty_sum_[symbol]     = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol]    = original_limit_sum;
        token_balances_[settlement_currency_] = original_settlement_balance;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onUpdateFromStream(const nlohmann::json& data)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (!ct::helper::isLiveTrading())
    {
        throw std::runtime_error("This method is only for live trading");
    }

    token_balances_[settlement_currency_] = data["balance"].get< double >();

    if (started_balance_ == 0)
    {
        started_balance_ = data["balance"].get< double >();
    }
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::marketOrder(
    const std::string& symbol, double qty, double current_price, const std::string& side, bool reduce_only)
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::limitOrder(
    const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only)
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::stopOrder(
    const std::string& symbol, double qty, double price, const std::string& side, bool reduce_only)
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::cancelAllOrders(const std::string& symbol)
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::cancelOrder(const std::string& symbol, const std::string& order_id)
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::fetchPrecisions()
{
    // TODO:
    throw std::runtime_error("Not implemented!");
}
