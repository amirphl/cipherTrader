#include "DB.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "Config.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "SQLLogger.hpp"

// Basic sqlpp11 headers
#include <sqlpp11/boolean_expression.h>
#include <sqlpp11/postgresql/postgresql.h> // If using PostgreSQL
#include <sqlpp11/sqlpp11.h>

// For using parameters
#include <sqlpp11/parameter.h>
#include <sqlpp11/parameter_list.h>

// For data types
#include <sqlpp11/data_types/blob.h>
#include <sqlpp11/data_types/floating_point.h>
#include <sqlpp11/data_types/integral.h>

// For common operations
#include <sqlpp11/aggregate_functions.h>
#include <sqlpp11/functions.h>
#include <sqlpp11/insert.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/select.h>
#include <sqlpp11/transaction.h>
#include <sqlpp11/update.h>
#include <sqlpp11/where.h>

// For prepared statements (if using that approach)
#include <sqlpp11/prepared_insert.h>
#include <sqlpp11/prepared_select.h>
#include <sqlpp11/prepared_update.h>

// For Boost UUID
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// Default constructor
CipherDB::Candle::Candle() : id_(boost::uuids::random_generator()()) {}

// Constructor with attributes
CipherDB::Candle::Candle(const std::unordered_map< std::string, std::any >& attributes) : Candle()
{
    try
    {
        if (attributes.count("id"))
        {
            if (attributes.at("id").type() == typeid(std::string))
            {
                id_ = boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("id")));
            }
            else if (attributes.at("id").type() == typeid(boost::uuids::uuid))
            {
                id_ = std::any_cast< boost::uuids::uuid >(attributes.at("id"));
            }
        }

        if (attributes.count("timestamp"))
            timestamp_ = std::any_cast< int64_t >(attributes.at("timestamp"));
        if (attributes.count("open"))
            open_ = std::any_cast< double >(attributes.at("open"));
        if (attributes.count("close"))
            close_ = std::any_cast< double >(attributes.at("close"));
        if (attributes.count("high"))
            high_ = std::any_cast< double >(attributes.at("high"));
        if (attributes.count("low"))
            low_ = std::any_cast< double >(attributes.at("low"));
        if (attributes.count("volume"))
            volume_ = std::any_cast< double >(attributes.at("volume"));
        if (attributes.count("exchange"))
            exchange_ = std::any_cast< std::string >(attributes.at("exchange"));
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("timeframe"))
            timeframe_ = std::any_cast< std::string >(attributes.at("timeframe"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Candle: ") + e.what());
    }
}

// Default constructor
CipherDB::ClosedTrade::ClosedTrade()
    : id_(boost::uuids::random_generator()())
    , buy_orders_(CipherDynamicArray::DynamicBlazeArray< double >({10, 2}, false))
    , sell_orders_(CipherDynamicArray::DynamicBlazeArray< double >({10, 2}, false))
{
}

// Constructor with attributes
CipherDB::ClosedTrade::ClosedTrade(const std::unordered_map< std::string, std::any >& attributes) : ClosedTrade()
{
    try
    {
        if (attributes.count("id"))
        {
            if (attributes.at("id").type() == typeid(std::string))
            {
                id_ = boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("id")));
            }
            else if (attributes.at("id").type() == typeid(boost::uuids::uuid))
            {
                id_ = std::any_cast< boost::uuids::uuid >(attributes.at("id"));
            }
        }

        if (attributes.count("strategy_name"))
            strategy_name_ = std::any_cast< std::string >(attributes.at("strategy_name"));
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("exchange"))
            exchange_ = std::any_cast< std::string >(attributes.at("exchange"));
        if (attributes.count("type"))
            type_ = std::any_cast< std::string >(attributes.at("type"));
        if (attributes.count("timeframe"))
            timeframe_ = std::any_cast< std::string >(attributes.at("timeframe"));
        if (attributes.count("opened_at"))
            opened_at_ = std::any_cast< int64_t >(attributes.at("opened_at"));
        if (attributes.count("closed_at"))
            closed_at_ = std::any_cast< int64_t >(attributes.at("closed_at"));
        if (attributes.count("leverage"))
            leverage_ = std::any_cast< int >(attributes.at("leverage"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing ClosedTrade: ") + e.what());
    }
}

std::string CipherDB::ClosedTrade::getIdAsString() const
{
    return boost::uuids::to_string(id_);
}

void CipherDB::ClosedTrade::setId(const std::string& id_str)
{
    id_ = boost::uuids::string_generator()(id_str);
}

void CipherDB::ClosedTrade::addBuyOrder(double qty, double price)
{
    // TODO:
    blaze::StaticVector< double, 2 > order;
    order[0] = qty;
    order[1] = price;
    buy_orders_.append(order);
}

void CipherDB::ClosedTrade::addSellOrder(double qty, double price)
{
    // TODO:
    blaze::StaticVector< double, 2 > order;
    order[0] = qty;
    order[1] = price;
    sell_orders_.append(order);
}

void CipherDB::ClosedTrade::addOrder(const Order& order)
{
    // TODO:
    orders_.push_back(order);

    if (order.side == "buy")
    {
        addBuyOrder(order.qty, order.price);
    }
    else if (order.side == "sell")
    {
        addSellOrder(order.qty, order.price);
    }
}

double CipherDB::ClosedTrade::getQty() const
{
    if (isLong())
    {
        double total = 0.0;
        for (size_t i = 0; i < buy_orders_.size(); ++i)
        {
            total += buy_orders_[static_cast< int >(i)][0];
        }
        return total;
    }
    else if (isShort())
    {
        double total = 0.0;
        for (size_t i = 0; i < sell_orders_.size(); ++i)
        {
            total += sell_orders_[static_cast< int >(i)][0];
        }
        return total;
    }
    return 0.0;
}

double CipherDB::ClosedTrade::getEntryPrice() const
{
    const auto& orders = isLong() ? buy_orders_ : sell_orders_;

    double qty_sum   = 0.0;
    double price_sum = 0.0;

    for (size_t i = 0; i < orders.size(); ++i)
    {
        auto row = orders[static_cast< int >(i)];
        qty_sum += row[0];
        price_sum += row[0] * row[1];
    }

    return qty_sum != 0.0 ? price_sum / qty_sum : std::numeric_limits< double >::quiet_NaN();
}

double CipherDB::ClosedTrade::getExitPrice() const
{
    const auto& orders = isLong() ? sell_orders_ : buy_orders_;

    double qty_sum   = 0.0;
    double price_sum = 0.0;

    for (size_t i = 0; i < orders.size(); ++i)
    {
        auto row = orders[static_cast< int >(i)];
        qty_sum += row[0];
        price_sum += row[0] * row[1];
    }

    return qty_sum != 0.0 ? price_sum / qty_sum : std::numeric_limits< double >::quiet_NaN();
}

double CipherDB::ClosedTrade::getFee() const
{
    std::stringstream keys;
    keys << "env.exchanges." << exchange_ << ".fee";

    auto trading_fee = std::get< int >(CipherConfig::Config::getInstance().get(keys.str()));

    return trading_fee * getQty() * (getEntryPrice() + getExitPrice());
}

double CipherDB::ClosedTrade::getSize() const
{
    return getQty() * getEntryPrice();
}

double CipherDB::ClosedTrade::getPnl() const
{
    std::stringstream keys;
    keys << "env.exchanges." << exchange_ << ".fee";

    auto fee           = std::get< int >(CipherConfig::Config::getInstance().get(keys.str()));
    double qty         = getQty();
    double entry_price = getEntryPrice();
    double exit_price  = getExitPrice();
    auto trade_type    = CipherEnum::toTradeType(type_);

    return CipherHelper::estimatePNL(qty, entry_price, exit_price, trade_type, fee);
}

double CipherDB::ClosedTrade::getPnlPercentage() const
{
    return getRoi();
}

double CipherDB::ClosedTrade::getRoi() const
{
    double total_cost = getTotalCost();
    return total_cost != 0.0 ? (getPnl() / total_cost) * 100.0 : 0.0;
}

double CipherDB::ClosedTrade::getTotalCost() const
{
    return getEntryPrice() * std::abs(getQty()) / leverage_;
}

int CipherDB::ClosedTrade::getHoldingPeriod() const
{
    return static_cast< int >((closed_at_ - opened_at_) / 1000);
}

bool CipherDB::ClosedTrade::isLong() const
{
    return type_ == CipherEnum::LONG;
}

bool CipherDB::ClosedTrade::isShort() const
{
    return type_ == CipherEnum::SHORT;
}

bool CipherDB::ClosedTrade::isOpen() const
{
    return opened_at_ != 0;
}

std::unordered_map< std::string, std::any > CipherDB::ClosedTrade::toJson() const
{
    std::unordered_map< std::string, std::any > result;
    result["id"]             = getIdAsString();
    result["strategy_name"]  = strategy_name_;
    result["symbol"]         = symbol_;
    result["exchange"]       = exchange_;
    result["type"]           = type_;
    result["entry_price"]    = getEntryPrice();
    result["exit_price"]     = getExitPrice();
    result["qty"]            = getQty();
    result["fee"]            = getFee();
    result["size"]           = getSize();
    result["PNL"]            = getPnl();
    result["PNL_percentage"] = getPnlPercentage();
    result["holding_period"] = getHoldingPeriod();
    result["opened_at"]      = opened_at_;
    result["closed_at"]      = closed_at_;

    return result;
}

std::unordered_map< std::string, std::any > CipherDB::ClosedTrade::toJsonWithOrders() const
{
    auto result = toJson();

    std::vector< std::unordered_map< std::string, std::any > > orders;
    for (const auto& order : orders_)
    {
        orders.push_back(order.toJson());
    }

    result["orders"] = orders;
    return result;
}

CipherDB::DailyBalance::DailyBalance() : id_(boost::uuids::random_generator()()) {}

CipherDB::DailyBalance::DailyBalance(const std::unordered_map< std::string, std::any >& attributes) : DailyBalance()
{
    try
    {
        if (attributes.count("id"))
        {
            if (attributes.at("id").type() == typeid(std::string))
            {
                id_ = boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("id")));
            }
            else if (attributes.at("id").type() == typeid(boost::uuids::uuid))
            {
                id_ = std::any_cast< boost::uuids::uuid >(attributes.at("id"));
            }
        }

        if (attributes.count("timestamp"))
            timestamp_ = std::any_cast< int64_t >(attributes.at("timestamp"));
        if (attributes.count("identifier"))
            identifier_ = std::any_cast< std::string >(attributes.at("identifier"));
        if (attributes.count("exchange"))
            exchange_ = std::any_cast< std::string >(attributes.at("exchange"));
        if (attributes.count("asset"))
            asset_ = std::any_cast< std::string >(attributes.at("asset"));
        if (attributes.count("balance"))
            balance_ = std::any_cast< double >(attributes.at("balance"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing DailyBalance: ") + e.what());
    }
}

// Default constructor generates a random UUID
CipherDB::ExchangeApiKeys::ExchangeApiKeys()
    : id_(boost::uuids::random_generator()())
    , created_at_(
          std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
              .count()) // TODO: Use Helper
{
}

// Constructor with attribute map
CipherDB::ExchangeApiKeys::ExchangeApiKeys(const std::unordered_map< std::string, std::any >& attributes)
    : ExchangeApiKeys()
{
    try
    {
        if (attributes.count("id"))
        {
            if (attributes.at("id").type() == typeid(std::string))
            {
                id_ = boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("id")));
            }
            else if (attributes.at("id").type() == typeid(boost::uuids::uuid))
            {
                id_ = std::any_cast< boost::uuids::uuid >(attributes.at("id"));
            }
        }

        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< std::string >(attributes.at("exchange_name"));
        if (attributes.count("name"))
            name_ = std::any_cast< std::string >(attributes.at("name"));
        if (attributes.count("api_key"))
            api_key_ = std::any_cast< std::string >(attributes.at("api_key"));
        if (attributes.count("api_secret"))
            api_secret_ = std::any_cast< std::string >(attributes.at("api_secret"));
        if (attributes.count("additional_fields"))
            additional_fields_ = std::any_cast< std::string >(attributes.at("additional_fields"));
        if (attributes.count("created_at"))
            created_at_ = std::any_cast< int64_t >(attributes.at("created_at"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing ExchangeApiKeys: ") + e.what());
    }
}
