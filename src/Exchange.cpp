#include "Exchange.hpp"
#include "Config.hpp"
#include "Enum.hpp"
#include "Exception.hpp"
#include "Helper.hpp"
#include "Logger.hpp"
#include "Order.hpp"
#include "Position.hpp"
#include "Route.hpp"

// TODO: Read logic again.
// TODO: When any exception raised, revert the state to point before function execution.

ct::exchange::Exchange::Exchange(const enums::ExchangeName& name,
                                 double starting_balance,
                                 double fee_rate,
                                 const enums::ExchangeType& exchange_type)
    : name_(name)
    , starting_balance_(starting_balance)
    , fee_rate_(fee_rate)
    , exchange_type_(exchange_type)
    , settlement_currency_("USDT") // Default, can be overridden
{
    // Initialize starting assets and current assets
    starting_assets_[settlement_currency_] = starting_balance;
    assets_[settlement_currency_]          = starting_balance;

    try
    {
        settlement_currency_ = getExchangeData(name_).getSettlementCurrency();
    }
    catch (...)
    {
        // TODO:
        // all_trading_routes = selectors.get_all_trading_routes()
        // auto first_route = all_trading_routes[0];
        // settlement_currency_ = helper::getQuoteAsset();
    }

    // TODO:
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

double ct::exchange::Exchange::getAsset(const std::string& asset) const
{
    auto it = assets_.find(asset);
    if (it != assets_.end())
    {
        return it->second;
    }
    return 0.0;
}

void ct::exchange::Exchange::setAsset(const std::string& asset, double balance)
{
    assets_[asset] = balance;
}

ct::exchange::SpotExchange::SpotExchange(const enums::ExchangeName& name, double starting_balance, double fee_rate)
    : Exchange(name, starting_balance, fee_rate, enums::ExchangeType::SPOT)
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

    if (helper::isLiveTrading())
    {
        return started_balance_;
    }

    auto it = starting_assets_.find(getAppCurrency());
    if (it != starting_assets_.end())
    {
        return it->second;
    }
    return 0.0;
}

double ct::exchange::SpotExchange::getWalletBalance() const
{
    std::lock_guard< std::mutex > lock(mutex_);
    return getAsset(settlement_currency_);
}

double ct::exchange::SpotExchange::getAvailableMargin() const
{
    return getWalletBalance();
}

void ct::exchange::SpotExchange::addRealizedPnl([[maybe_unused]] double realized_pnl)
{
    throw std::runtime_error("Not implemented!");
};

void ct::exchange::SpotExchange::chargeFee([[maybe_unused]] double amount)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::increateAssetTempReducedAmount([[maybe_unused]] const std::string& asset,
                                                                [[maybe_unused]] double amount)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::onOrderSubmission(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    // Save original state before making any changes
    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);

    // Save original balances and order sums
    double original_settlement_balance = assets_[settlement_currency_];
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];

    try
    {
        if (order.getOrderSide() == enums::OrderSide::SELL)
        {
            if (order.getOrderType() == enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] =
                    helper::addFloatsMaintainPrecesion(stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getOrderType() == enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] =
                    helper::addFloatsMaintainPrecesion(limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }

            double base_balance = assets_[base_asset];

            // Sell order's qty cannot be bigger than the amount of existing base asset
            double order_qty = 0.0;

            if (order.getOrderType() == enums::OrderType::MARKET)
            {
                order_qty =
                    helper::addFloatsMaintainPrecesion(std::abs(order.getQty()), limit_sell_orders_qty_sum_[symbol]);
            }
            else if (order.getOrderType() == enums::OrderType::STOP)
            {
                order_qty = stop_sell_orders_qty_sum_[symbol];
            }
            else if (order.getOrderType() == enums::OrderType::LIMIT)
            {
                order_qty = limit_sell_orders_qty_sum_[symbol];
            }
            else
            {
                throw std::runtime_error("Unknown order type " + enums::toString(order.getOrderType()));
            }

            // Validate that the total selling amount is not bigger than the amount of the existing base asset
            if (order_qty > base_balance)
            {
                auto msg = "InsufficientBalance: Not enough balance. Available balance at " + enums::toString(name_) +
                           " for " + base_asset + " is " + std::to_string(base_balance) +
                           " but you're trying to sell " + std::to_string(order_qty);

                throw exception::InsufficientBalance(msg);
            }
        }
        else
        {
            // Cannot buy if we don't have enough balance (of the settlement currency)
            auto rem = helper::subtractFloatsMaintainPrecesion(assets_[settlement_currency_], order.getValue());

            if (rem < 0)
            {
                auto msg = "InsufficientBalance: Not enough balance. Available balance at " + enums::toString(name_) +
                           " for " + std::string(settlement_currency_) + " is " +
                           std::to_string(original_settlement_balance) + " but you're trying to spend " +
                           std::to_string(order.getValue());

                throw exception::InsufficientBalance(msg);
            }

            assets_[settlement_currency_] = rem;
        }
    }
    catch (...)
    {
        // Revert to original state in case of any exception
        assets_[settlement_currency_]      = original_settlement_balance;
        stop_sell_orders_qty_sum_[symbol]  = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol] = original_limit_sum;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onOrderExecution(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);

    // Save original values
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];
    double original_base_balance       = assets_[base_asset];
    double original_settlement_balance = assets_[settlement_currency_];

    try
    {
        if (order.getOrderSide() == enums::OrderSide::SELL)
        {
            if (order.getOrderType() == enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getOrderType() == enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }

            double current_balance = assets_[base_asset];
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
            assets_[settlement_currency_] = helper::addFloatsMaintainPrecesion(
                assets_[settlement_currency_], (order_qty * order.getPrice().value_or(0.0)) * (1 - fee_rate_));

            // Now reduce base asset's balance by the amount of the order's qty
            assets_[base_asset] = helper::subtractFloatsMaintainPrecesion(assets_[base_asset], order_qty);
        }
        else
        {
            // Asset's balance is increased by the amount of the order's qty after fees are deducted
            assets_[base_asset] =
                helper::addFloatsMaintainPrecesion(assets_[base_asset], std::abs(order.getQty()) * (1 - fee_rate_));
        }
    }
    catch (...)
    {
        // Restore original state in case of any exception
        stop_sell_orders_qty_sum_[symbol]  = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol] = original_limit_sum;
        assets_[base_asset]                = original_base_balance;
        assets_[settlement_currency_]      = original_settlement_balance;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onOrderCancellation(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    // Store original state
    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);

    // Save original values
    double original_stop_sum           = stop_sell_orders_qty_sum_[symbol];
    double original_limit_sum          = limit_sell_orders_qty_sum_[symbol];
    double original_settlement_balance = assets_[settlement_currency_];

    // FIXME: Duplicate subtraction?
    // if (order.getSide() == enums::OrderSide::SELL)
    // {
    //     if (order.getType() == enums::OrderType::STOP)
    //     {
    //         stop_sell_orders_qty_sum_[order.getSymbol()] = helper::subtractFloatsMaintainPrecesion(
    //             stop_sell_orders_qty_sum_[order.getSymbol()], std::abs(order.getQty()));
    //     }
    //     else if (order.getType() == enums::OrderType::LIMIT)
    //     {
    //         limit_sell_orders_qty_sum_[order.getSymbol()] = helper::subtractFloatsMaintainPrecesion(
    //             limit_sell_orders_qty_sum_[order.getSymbol()], std::abs(order.getQty()));
    //     }
    // }

    try
    {
        // Buy order
        if (order.getOrderSide() == enums::OrderSide::BUY)
        {
            assets_[settlement_currency_] =
                helper::addFloatsMaintainPrecesion(assets_[settlement_currency_], std::abs(order.getValue()));
        }
        // Sell order
        else
        {
            if (order.getOrderType() == enums::OrderType::STOP)
            {
                stop_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    stop_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
            else if (order.getOrderType() == enums::OrderType::LIMIT)
            {
                limit_sell_orders_qty_sum_[symbol] = helper::subtractFloatsMaintainPrecesion(
                    limit_sell_orders_qty_sum_[symbol], std::abs(order.getQty()));
            }
        }
    }
    catch (...)
    {
        // Restore original state in case of any exception
        stop_sell_orders_qty_sum_[symbol]  = original_stop_sum;
        limit_sell_orders_qty_sum_[symbol] = original_limit_sum;
        assets_[settlement_currency_]      = original_settlement_balance;

        // Re-throw the exception
        throw;
    }
}

void ct::exchange::SpotExchange::onUpdateFromStream(const nlohmann::json& data)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (!helper::isLiveTrading())
    {
        throw std::runtime_error("This method is only for live trading");
    }

    assets_[settlement_currency_] = data["balance"].get< double >();

    if (started_balance_ == 0)
    {
        started_balance_ = data["balance"].get< double >();
    }
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::marketOrder([[maybe_unused]] const std::string& symbol,
                                                                         [[maybe_unused]] double qty,
                                                                         [[maybe_unused]] double current_price,
                                                                         [[maybe_unused]] const enums::OrderSide& side,
                                                                         [[maybe_unused]] bool reduce_only)
{
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::limitOrder([[maybe_unused]] const std::string& symbol,
                                                                        [[maybe_unused]] double qty,
                                                                        [[maybe_unused]] double price,
                                                                        [[maybe_unused]] const enums::OrderSide& side,
                                                                        [[maybe_unused]] bool reduce_only)
{
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::SpotExchange::stopOrder([[maybe_unused]] const std::string& symbol,
                                                                       [[maybe_unused]] double qty,
                                                                       [[maybe_unused]] double price,
                                                                       [[maybe_unused]] const enums::OrderSide& side,
                                                                       [[maybe_unused]] bool reduce_only)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::cancelAllOrders([[maybe_unused]] const std::string& symbol)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::cancelOrder([[maybe_unused]] const std::string& symbol,
                                             [[maybe_unused]] const std::string& order_id)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::SpotExchange::fetchPrecisions()
{
    throw std::runtime_error("Not implemented!");
}

ct::exchange::FuturesExchange::FuturesExchange(const enums::ExchangeName& name,
                                               double starting_balance,
                                               double fee_rate,
                                               const enums::LeverageMode& futures_leverage_mode,
                                               int futures_leverage)
    : Exchange(name, starting_balance, fee_rate, enums::ExchangeType::FUTURES)
    , futures_leverage_mode_(futures_leverage_mode)
    , futures_leverage_(futures_leverage)
{
    // In futures trading, everything is denominated in the settlement currency
}

double ct::exchange::FuturesExchange::getStartedBalance() const
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return started_balance_;
    }

    auto it = starting_assets_.find(getAppCurrency());
    if (it != starting_assets_.end())
    {
        return it->second;
    }
    return 0.0;
}

double ct::exchange::FuturesExchange::getWalletBalance() const
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return wallet_balance_;
    }

    return getAsset(settlement_currency_);
}

double ct::exchange::FuturesExchange::getAvailableMargin() const
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return available_margin_;
    }

    // Start with the wallet balance
    // In both live trading and backtesting/paper trading, we start with the balance
    double margin = getAsset(settlement_currency_);

    // Calculate the total spent amount considering leverage
    // Here we need to calculate the total cost of all open positions and orders, considering leverage
    double total_spent = 0.0;

    // For now, a simplified implementation:
    for (const auto& [asset, balance] : assets_)
    {
        if (asset == settlement_currency_)
            continue;

        // TODO: Position handling
        // auto position = selectors.get_position(name_, asset + "-" + settlement_currency_);
        auto position = std::make_optional< position::Position >(enums::ExchangeName::BINANCE_SPOT, "");

        if (position && position->isOpen())
        {
            total_spent += position->getTotalCost();
            total_spent -= position->getPnl();
        }

        // Summing up the cost of open orders (buy and sell), considering leverage

        // Calculate the sum of buy orders (qty * price) for the asset
        double sum_buy_orders = 0.0;
        if (buy_orders_.find(asset) != buy_orders_.end() && buy_orders_.at(asset)->size() > 0)
        {
            const auto& mt = buy_orders_.at(asset);

            // Get a view of the entire data for the calculation
            const auto& data = mt->rows(0, mt->size());

            // Calculate the element-wise product of column 0 (qty) and column 1 (price)
            auto products = blaze::column(data, 0) * blaze::column(data, 1);

            // Sum all the products
            sum_buy_orders = blaze::sum(products);
        }

        // Calculate the sum of sell orders (qty * price) for the asset
        double sum_sell_orders = 0.0;
        if (sell_orders_.find(asset) != sell_orders_.end() && sell_orders_.at(asset)->size() > 0)
        {
            const auto& tm = sell_orders_.at(asset);
            // Get a view of the entire data for the calculation
            const auto& data = tm->rows(0, tm->size());

            // Calculate the element-wise product of column 0 (qty) and column 1 (price)
            auto products = blaze::column(data, 0) * blaze::column(data, 1);

            // Sum all the products
            sum_sell_orders = blaze::sum(products);
        }

        total_spent +=
            std::max(std::abs(sum_buy_orders) / futures_leverage_, std::abs(sum_sell_orders) / futures_leverage_);
    }

    // Subtract the total spent from the margin
    margin -= total_spent;

    return margin;
}

void ct::exchange::FuturesExchange::addRealizedPnl(double realized_pnl)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    double new_balance = assets_[settlement_currency_] + realized_pnl;

    logger::LOG.info("Added realized PNL of " + std::to_string(std::round(realized_pnl * 100) / 100) +
                     ". Balance for " + std::string(settlement_currency_) + " on " + enums::toString(name_) +
                     " changed from " + std::to_string(std::round(assets_[settlement_currency_] * 100) / 100) + " to " +
                     std::to_string(std::round(new_balance * 100) / 100));

    assets_[settlement_currency_] = new_balance;
}

void ct::exchange::FuturesExchange::chargeFee(double amount)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    double fee_amount  = std::abs(amount) * fee_rate_;
    double new_balance = assets_[settlement_currency_] - fee_amount;

    if (fee_amount != 0)
    {
        logger::LOG.info("Charged " + std::to_string(std::round(fee_amount * 100) / 100) + " as fee. Balance for " +
                         std::string(settlement_currency_) + " on " + enums::toString(name_) + " changed from " +
                         std::to_string(std::round(assets_[settlement_currency_] * 100) / 100) + " to " +
                         std::to_string(std::round(new_balance * 100) / 100));
    }

    assets_[settlement_currency_] = new_balance;
}

void ct::exchange::FuturesExchange::increateAssetTempReducedAmount([[maybe_unused]] const std::string& asset,
                                                                   [[maybe_unused]] double amount)
{
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::FuturesExchange::onOrderSubmission(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    // Store the original state for rollback in case of exceptions
    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);

    // Check margin availability if not a reduce-only order
    // make sure we don't spend more than we're allowed considering current allowed leverage
    if (!order.isReduceOnly())
    {
        // Calculate effective order size considering leverage
        double effective_order_size = std::abs(order.getQty() * order.getPrice().value_or(0.0)) / futures_leverage_;

        if (effective_order_size > getAvailableMargin())
        {
            throw exception::InsufficientMargin(
                "Cannot submit an order with a value of $" +
                std::to_string(std::round(order.getQty() * order.getPrice().value_or(0.0) * 100) / 100) +
                " when your available margin is $" + std::to_string(std::round(getAvailableMargin() * 100) / 100) +
                ". Consider increasing leverage number from the settings or reducing the order size.");
        }
    }

    // Update available assets
    available_assets_[base_asset] += order.getQty();

    // Track the order for margin calculations
    if (!order.isReduceOnly())
    {
        blaze::StaticVector< double, 2UL, blaze::rowVector > row = {order.getQty(), order.getPrice().value_or(.0)};

        if (order.getOrderSide() == enums::OrderSide::BUY)
        {
            buy_orders_.at(base_asset)->append(row);
        }
        else
        {
            sell_orders_.at(base_asset)->append(row);
        }
    }
}

void ct::exchange::FuturesExchange::onOrderExecution(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    // Store original state for exception handling
    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);


    if (!order.isReduceOnly())
    {
        blaze::StaticVector< double, 2, blaze::rowVector > row = {order.getQty(), order.getPrice().value_or(.0)};

        auto& tm =
            (order.getOrderSide() == enums::OrderSide::BUY) ? buy_orders_.at(base_asset) : sell_orders_.at(base_asset);

        auto index = tm->find(row, 1);
        if (index >= 0)
        {
            tm->deleteRow(index);
        }
    }
}

void ct::exchange::FuturesExchange::onOrderCancellation(const ct::db::Order& order)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (helper::isLiveTrading())
    {
        return;
    }

    // Store original state for exception handling
    std::string symbol     = order.getSymbol();
    std::string base_asset = helper::getBaseAsset(symbol);

    // Update available assets
    available_assets_[base_asset] -= order.getQty();

    if (!order.isReduceOnly())
    {
        blaze::StaticVector< double, 2, blaze::rowVector > row = {order.getQty(), order.getPrice().value_or(.0)};

        auto& tm =
            (order.getOrderSide() == enums::OrderSide::BUY) ? buy_orders_.at(base_asset) : sell_orders_.at(base_asset);

        auto index = tm->find(row, 1);
        if (index >= 0)
        {
            tm->deleteRow(index);
        }
    }
}

// Used for updating the exchange from the WS stream (only for live trading)
void ct::exchange::FuturesExchange::onUpdateFromStream(const nlohmann::json& data)
{
    std::lock_guard< std::mutex > lock(mutex_);

    if (!helper::isLiveTrading())
    {
        throw std::runtime_error("This method is only for live trading");
    }

    available_margin_ = data["available_margin"].get< double >();
    wallet_balance_   = data["wallet_balance"].get< double >();

    if (started_balance_ == 0)
    {
        started_balance_ = wallet_balance_;
    }
}

// Implement the required virtual methods from base class
std::shared_ptr< ct::db::Order > ct::exchange::FuturesExchange::marketOrder(
    [[maybe_unused]] const std::string& symbol,
    [[maybe_unused]] double qty,
    [[maybe_unused]] double current_price,
    [[maybe_unused]] const enums::OrderSide& side,
    [[maybe_unused]] bool reduce_only)
{
    // TODO: Implement market order logic for futures
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::FuturesExchange::limitOrder(
    [[maybe_unused]] const std::string& symbol,
    [[maybe_unused]] double qty,
    [[maybe_unused]] double price,
    [[maybe_unused]] const enums::OrderSide& side,
    [[maybe_unused]] bool reduce_only)
{
    // TODO: Implement limit order logic for futures
    throw std::runtime_error("Not implemented!");
}

std::shared_ptr< ct::db::Order > ct::exchange::FuturesExchange::stopOrder([[maybe_unused]] const std::string& symbol,
                                                                          [[maybe_unused]] double qty,
                                                                          [[maybe_unused]] double price,
                                                                          [[maybe_unused]] const enums::OrderSide& side,
                                                                          [[maybe_unused]] bool reduce_only)
{
    // TODO: Implement stop order logic for futures
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::FuturesExchange::cancelAllOrders([[maybe_unused]] const std::string& symbol)
{
    // TODO: Implement cancel all orders logic for futures
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::FuturesExchange::cancelOrder([[maybe_unused]] const std::string& symbol,
                                                [[maybe_unused]] const std::string& order_id)
{
    // TODO: Implement cancel order logic for futures
    throw std::runtime_error("Not implemented!");
}

void ct::exchange::FuturesExchange::fetchPrecisions()
{
    // TODO: Implement fetch precisions logic for futures
    throw std::runtime_error("Not implemented!");
}


ct::exchange::ExchangesState& ct::exchange::ExchangesState::getInstance()
{
    static ExchangesState instance;
    return instance;
}

void ct::exchange::ExchangesState::init()
{
    std::lock_guard< std::mutex > lock(mutex_);

    const auto& config = config::Config::getInstance();

    // Get considering exchanges from config
    std::vector< std::string > consideringExchanges =
        config.getValue< std::vector< std::string > >("app.considering_exchanges");

    for (const auto& exchangeNameStr : consideringExchanges)
    {
        enums::ExchangeName exchangeName = enums::toExchangeName(exchangeNameStr);

        // Get exchange configuration
        double startingBalance = config.getValue< double >("env.exchanges." + exchangeNameStr + ".balance");
        double fee             = config.getValue< double >("env.exchanges." + exchangeNameStr + ".fee");

        // Get exchange type
        enums::ExchangeType exchangeType = getExchangeType(exchangeName);

        // Create appropriate exchange type
        if (exchangeType == enums::ExchangeType::SPOT)
        {
            storage_[exchangeName] = std::make_shared< SpotExchange >(exchangeName, startingBalance, fee);
        }
        else if (exchangeType == enums::ExchangeType::FUTURES)
        {
            // Get futures-specific configuration
            std::string futuresLeverageMode =
                config.getValue< std::string >("env.exchanges." + exchangeNameStr + ".futures_leverage_mode");
            enums::LeverageMode leverageMode = enums::toLeverageMode(futuresLeverageMode);
            int leverage = config.getValue< int >("env.exchanges." + exchangeNameStr + ".futures_leverage");

            storage_[exchangeName] =
                std::make_shared< FuturesExchange >(exchangeName, startingBalance, fee, leverageMode, leverage);
        }
        else
        {
            // Log error and throw exception
            logger::LOG.error("Invalid exchange type: " + enums::toString(exchangeType) +
                              ". Supported values are 'spot' and 'futures'.");

            throw exception::InvalidConfig("Value for exchange type in your config file is not valid. "
                                           "Supported values are \"spot\" and \"futures\". "
                                           "Your value is \"" +
                                           enums::toString(exchangeType) + "\"");
        }
    }
}

void ct::exchange::ExchangesState::reset()
{
    std::lock_guard< std::mutex > lock(mutex_);
    storage_.clear();
}

std::shared_ptr< ct::exchange::Exchange > ct::exchange::ExchangesState::getExchange(
    const enums::ExchangeName& exchange_name) const
{
    std::lock_guard< std::mutex > lock(mutex_);

    auto it = storage_.find(exchange_name);
    if (it == storage_.end())
    {
        throw std::runtime_error("Exchange not found: " + enums::toString(exchange_name));
    }

    return it->second;
}

bool ct::exchange::ExchangesState::hasExchange(const enums::ExchangeName& name) const
{
    std::lock_guard< std::mutex > lock(mutex_);
    return storage_.find(name) != storage_.end();
}

ct::exchange::Sandbox::Sandbox(const enums::ExchangeName& name) : Exchange(name, 0.0, 0.0, enums::ExchangeType::SPOT) {}

double ct::exchange::Sandbox::getStartedBalance() const
{
    return starting_balance_;
}

double ct::exchange::Sandbox::getWalletBalance() const
{
    return starting_balance_; // Simplified for sandbox
}

double ct::exchange::Sandbox::getAvailableMargin() const
{
    return starting_balance_; // Simplified for sandbox
}

ct::enums::LeverageMode ct::exchange::Sandbox::getLeverageMode() const
{
    return enums::LeverageMode::CROSS; // Default to cross leverage
}

void ct::exchange::Sandbox::addRealizedPnl([[maybe_unused]] double realized_pnl)
{
    // Implementation for sandbox
}

void ct::exchange::Sandbox::chargeFee([[maybe_unused]] double amount)
{
    // Implementation for sandbox
}

void ct::exchange::Sandbox::increateAssetTempReducedAmount([[maybe_unused]] const std::string& asset,
                                                           [[maybe_unused]] double amount)
{
    // Implementation for sandbox
}

void ct::exchange::Sandbox::onOrderSubmission([[maybe_unused]] const db::Order& order)
{
    // Implementation for sandbox
}

void ct::exchange::Sandbox::onOrderExecution([[maybe_unused]] const db::Order& order)
{
    // Implementation for sandbox
}

void ct::exchange::Sandbox::onOrderCancellation([[maybe_unused]] const db::Order& order)
{
    // Implementation for sandbox
}

std::shared_ptr< ct::db::Order > ct::exchange::Sandbox::marketOrder(
    const std::string& symbol, double qty, double current_price, const enums::OrderSide& side, bool reduce_only)
{
    auto tradeId    = boost::uuids::nil_uuid();
    auto sessionId  = boost::uuids::nil_uuid();
    auto exchangeId = std::nullopt;
    auto orderType  = enums::OrderType::MARKET;
    auto filledQty  = .0;
    auto status     = enums::OrderStatus::QUEUED; // TODO: Proper value?
    auto createdAt  = helper::nowToTimestamp();
    nlohmann::json vars;
    auto submittedVia = std::nullopt;

    auto order = std::make_shared< db::Order >(tradeId,
                                               sessionId,
                                               exchangeId,
                                               symbol,
                                               name_,
                                               side,
                                               orderType,
                                               reduce_only,
                                               helper::prepareQty(qty, enums::toString(side)),
                                               filledQty,
                                               current_price,
                                               status,
                                               createdAt,
                                               std::nullopt,
                                               std::nullopt,
                                               vars,
                                               submittedVia);

    // Add to orders state
    order::OrdersState::getInstance().addOrder(order);

    // Add to execution queue
    order::OrdersState::getInstance().addOrderToExecute(order);

    return order;
}

std::shared_ptr< ct::db::Order > ct::exchange::Sandbox::limitOrder(
    const std::string& symbol, double qty, double price, const enums::OrderSide& side, bool reduce_only)
{
    auto tradeId    = boost::uuids::nil_uuid();
    auto sessionId  = boost::uuids::nil_uuid();
    auto exchangeId = std::nullopt;
    auto orderType  = enums::OrderType::STOP;
    auto filledQty  = .0;
    auto status     = enums::OrderStatus::QUEUED; // TODO: Proper value?
    auto createdAt  = helper::nowToTimestamp();
    nlohmann::json vars;
    auto submittedVia = std::nullopt;

    auto order = std::make_shared< db::Order >(tradeId,
                                               sessionId,
                                               exchangeId,
                                               symbol,
                                               name_,
                                               side,
                                               orderType,
                                               reduce_only,
                                               helper::prepareQty(qty, enums::toString(side)),
                                               filledQty,
                                               price,
                                               status,
                                               createdAt,
                                               std::nullopt,
                                               std::nullopt,
                                               vars,
                                               submittedVia);
    // Add to orders state
    order::OrdersState::getInstance().addOrder(order);

    return order;
}

std::shared_ptr< ct::db::Order > ct::exchange::Sandbox::stopOrder(
    const std::string& symbol, double qty, double price, const enums::OrderSide& side, bool reduce_only)
{
    auto tradeId    = boost::uuids::nil_uuid();
    auto sessionId  = boost::uuids::nil_uuid();
    auto exchangeId = std::nullopt;
    auto orderType  = enums::OrderType::LIMIT;
    auto filledQty  = .0;
    auto status     = enums::OrderStatus::QUEUED; // TODO: Proper value?
    auto createdAt  = helper::nowToTimestamp();
    nlohmann::json vars;
    auto submittedVia = std::nullopt;

    auto order = std::make_shared< db::Order >(tradeId,
                                               sessionId,
                                               exchangeId,
                                               symbol,
                                               name_,
                                               side,
                                               orderType,
                                               reduce_only,
                                               helper::prepareQty(qty, enums::toString(side)),
                                               filledQty,
                                               price,
                                               status,
                                               createdAt,
                                               std::nullopt,
                                               std::nullopt,
                                               vars,
                                               submittedVia);
    // Add to orders state
    order::OrdersState::getInstance().addOrder(order);

    return order;
}

void ct::exchange::Sandbox::cancelAllOrders(const std::string& symbol)
{
    // Get active orders for this symbol
    auto orders = order::OrdersState::getInstance().getActiveOrders(name_, symbol);

    // Cancel each order
    for (auto& order : orders)
    {
        if (order->isNew())
        {
            order->cancel();
        }
    }

    // Clear storage if not unit testing
    if (!helper::isUnitTesting())
    {
        order::OrdersState::getInstance().clearOrders(name_, symbol);
    }
}

void ct::exchange::Sandbox::cancelOrder(const std::string& symbol, const std::string& order_id)
{
    // Get the order and cancel it
    auto order = order::OrdersState::getInstance().getOrderById(name_, symbol, order_id);
    if (order)
    {
        order->cancel();
    }
}

void ct::exchange::Sandbox::fetchPrecisions()
{
    // No-op for sandbox
}

const std::string CIPHER_TRADER_API_URL{"https://api.ciphertrader.trade/api/v1"};

const std::string CIPHER_TRADER_WEBSITE_URL{"https://ciphertrader.trade"};

ct::enums::ExchangeType ct::exchange::ExchangesState::getExchangeType(const enums::ExchangeName& exchange_name) const
{
    const auto& config = config::Config::getInstance();

    if (helper::isLive())
    {
        // In live trading, exchange type is not configurable, so we get it from exchange info
        return getExchangeData(exchange_name).getExchangeType();
    }

    // For other trading modes, get the exchange type from config
    std::string exchangeTypeStr =
        config.getValue< std::string >("env.exchanges." + enums::toString(exchange_name) + ".type");
    return enums::toExchangeType(exchangeTypeStr);
}

const std::unordered_map< ct::enums::ExchangeName, ct::exchange::ExchangeData > ct::exchange::EXCHANGES_DATA{
    {enums::ExchangeName::BYBIT_USDT_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::BYBIT_USDT_PERPETUAL),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.00055,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium",
                  "USDT")},
    {enums::ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::BYBIT_USDT_PERPETUAL_TESTNET),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.00055,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium",
                  "USDT")},
    {enums::ExchangeName::BYBIT_USDC_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::BYBIT_USDC_PERPETUAL),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.00055,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium",
                  "USDC")},
    {enums::ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::BYBIT_USDC_PERPETUAL_TESTNET),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.00055,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium",
                  "USDC")},
    {enums::ExchangeName::BYBIT_SPOT,
     ExchangeData(toString(enums::ExchangeName::BYBIT_SPOT),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.001,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::BYBIT_SPOT_TESTNET,
     ExchangeData(toString(enums::ExchangeName::BYBIT_SPOT_TESTNET),
                  CIPHER_TRADER_WEBSITE_URL + "/bybit",
                  0.001,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BYBIT_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::BITFINEX_SPOT,
     ExchangeData(toString(enums::ExchangeName::BITFINEX_SPOT),
                  "https://bitfinex.com",
                  0.002,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS},
                  {
                      timeframe::Timeframe::MINUTE_1,
                      timeframe::Timeframe::MINUTE_5,
                      timeframe::Timeframe::MINUTE_15,
                      timeframe::Timeframe::MINUTE_30,
                      timeframe::Timeframe::HOUR_1,
                      timeframe::Timeframe::HOUR_3,
                      timeframe::Timeframe::HOUR_6,
                      timeframe::Timeframe::HOUR_12,
                      timeframe::Timeframe::DAY_1,
                  },
                  {{"backtesting", true}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::BINANCE_SPOT,
     ExchangeData(toString(enums::ExchangeName::BINANCE_SPOT),
                  "https://binance.com",
                  0.001,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BINANCE_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::BINANCE_US_SPOT,
     ExchangeData(toString(enums::ExchangeName::BINANCE_US_SPOT),
                  "https://binance.us",
                  0.001,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BINANCE_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::BINANCE_PERPETUAL_FUTURES,
     ExchangeData(toString(enums::ExchangeName::BINANCE_PERPETUAL_FUTURES),
                  "https://binance.com",
                  0.0004,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BINANCE_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET,
     ExchangeData(toString(enums::ExchangeName::BINANCE_PERPETUAL_FUTURES_TESTNET),
                  "https://binance.com",
                  0.0004,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BINANCE_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::COINBASE_SPOT,
     ExchangeData(toString(enums::ExchangeName::COINBASE_SPOT),
                  "https://www.coinbase.com/advanced-trade/spot/BTC-USD",
                  0.0003,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::COINBASE_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::APEX_PRO_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::APEX_PRO_PERPETUAL_TESTNET),
                  "https://testnet.pro.apex.exchange/trade/BTCUSD",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::APEX_PRO_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "free")},
    {enums::ExchangeName::APEX_PRO_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::APEX_PRO_PERPETUAL),
                  "https://pro.apex.exchange/trade/BTCUSD",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::APEX_PRO_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::APEX_OMNI_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::APEX_OMNI_PERPETUAL_TESTNET),
                  "https://testnet.omni.apex.exchange/trade/BTCUSD",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::APEX_PRO_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "free")},
    {enums::ExchangeName::APEX_OMNI_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::APEX_OMNI_PERPETUAL),
                  "https://omni.apex.exchange/trade/BTCUSD",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::APEX_PRO_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::GATE_USDT_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::GATE_USDT_PERPETUAL),
                  "https://jesse.trade/gate",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::GATE_TIMEFRAMES,
                  {{"backtesting", true}, {"live_trading", true}},
                  "premium",
                  "USDT")},
    {enums::ExchangeName::GATE_SPOT,
     ExchangeData(toString(enums::ExchangeName::GATE_SPOT),
                  "https://jesse.trade/gate",
                  0.0005,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::GATE_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", true}},
                  "premium")},
    {enums::ExchangeName::FTX_PERPETUAL_FUTURES,
     ExchangeData(toString(enums::ExchangeName::FTX_PERPETUAL_FUTURES),
                  "https://ftx.com/markets/future",
                  0.0006,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::FTX_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::FTX_SPOT,
     ExchangeData(toString(enums::ExchangeName::FTX_SPOT),
                  "https://ftx.com/markets/spot",
                  0.0007,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS},
                  timeframe::FTX_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::FTX_US_SPOT,
     ExchangeData(toString(enums::ExchangeName::FTX_US_SPOT),
                  "https://ftx.us",
                  0.002,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS},
                  timeframe::FTX_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::BITGET_USDT_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::BITGET_USDT_PERPETUAL_TESTNET),
                  CIPHER_TRADER_WEBSITE_URL + "/bitget",
                  0.0006,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BITGET_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium",
                  "USDT")},
    {enums::ExchangeName::BITGET_USDT_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::BITGET_USDT_PERPETUAL),
                  CIPHER_TRADER_WEBSITE_URL + "/bitget",
                  0.0006,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BITGET_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium",
                  "USDT")},
    {enums::ExchangeName::BITGET_SPOT,
     ExchangeData(toString(enums::ExchangeName::BITGET_SPOT),
                  CIPHER_TRADER_WEBSITE_URL + "/bitget",
                  0.0006,
                  enums::ExchangeType::SPOT,
                  {enums::LeverageMode::CROSS, enums::LeverageMode::ISOLATED},
                  timeframe::BITGET_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::DYDX_PERPETUAL,
     ExchangeData(toString(enums::ExchangeName::DYDX_PERPETUAL),
                  CIPHER_TRADER_WEBSITE_URL + "/dydx",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::DYDX_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")},
    {enums::ExchangeName::DYDX_PERPETUAL_TESTNET,
     ExchangeData(toString(enums::ExchangeName::DYDX_PERPETUAL_TESTNET),
                  "https://trade.stage.dydx.exchange/trade/ETH-USD",
                  0.0005,
                  enums::ExchangeType::FUTURES,
                  {enums::LeverageMode::CROSS},
                  timeframe::DYDX_TIMEFRAMES,
                  {{"backtesting", false}, {"live_trading", false}},
                  "premium")}};

const ct::exchange::ExchangeData ct::exchange::getExchangeData(const enums::ExchangeName& exchange)
{
    return EXCHANGES_DATA.at(exchange);
}

std::vector< std::string > ct::exchange::getExchangesByMode(const std::string& mode)
{
    std::vector< std::string > exchanges;

    for (const auto& [exchange_name, data] : EXCHANGES_DATA)
    {
        const auto& modes = data.getModes();
        if (modes.at(mode))
        {
            exchanges.push_back(toString(exchange_name));
        }
    }

    std::sort(exchanges.begin(), exchanges.end());
    return exchanges;
}

const std::vector< std::string > ct::exchange::BACKTESTING_EXCHANGES = ct::exchange::getExchangesByMode("backtesting");

const std::vector< std::string > ct::exchange::LIVE_TRADING_EXCHANGES =
    ct::exchange::getExchangesByMode("live_trading");

std::string ct::exchange::getAppCurrency()
{
    auto route = route::Router::getInstance().getRoute(0);

    auto exchange_name = route.exchange_name;
    if (EXCHANGES_DATA.find(exchange_name) != EXCHANGES_DATA.end())
    {
        return EXCHANGES_DATA.at(exchange_name).getSettlementCurrency();
    }

    return helper::getQuoteAsset(route.symbol);
}
