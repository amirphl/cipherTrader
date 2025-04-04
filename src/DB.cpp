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

bool CipherDB::Candle::save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // Convert UUID to binary for PostgreSQL
        // auto uuid_data = std::vector< uint8_t >(id_.size());
        // std::copy(id_.begin(), id_.end(), uuid_data.begin());
        // std::string uuid_binary(reinterpret_cast< char* >(uuid_data.data()), uuid_data.size());

        // Convert Boost UUID to string
        std::string uuid_binary = boost::uuids::to_string(id_);

        // Create and log select statement
        auto select_stmt = select(sqlpp::count(t.id)).from(t).where(t.id == parameter(t.id));
        // CipherDB::SQLLogger::getInstance().logStatement(select_stmt, "SELECT");

        // Prepare the statement
        auto sprep = conn.prepare(select_stmt);

        // Log parameter binding
        // CipherDB::SQLLogger::getInstance().logPrepared(select_stmt, "id", uuid_binary);

        // Bind the parameter value
        sprep.params.id = uuid_binary;

        // Execute the prepared statement
        auto result = conn(sprep);

        if (result.empty() || result.front().count.value() == 0)
        {
            // Create insert statement
            auto insert_stmt = insert_into(t).set(t.id        = uuid_binary,
                                                  t.timestamp = timestamp_,
                                                  t.open      = open_,
                                                  t.close     = close_,
                                                  t.high      = high_,
                                                  t.low       = low_,
                                                  t.volume    = volume_,
                                                  t.exchange  = exchange_,
                                                  t.symbol    = symbol_,
                                                  t.timeframe = timeframe_);

            // Log the insert statement
            // CipherDB::SQLLogger::getInstance().logStatement(insert_stmt, "INSERT");

            // Execute
            conn(insert_stmt);
        }
        else
        {
            // Create update statement
            auto update_stmt = update(t)
                                   .set(t.timestamp = timestamp_,
                                        t.open      = open_,
                                        t.close     = close_,
                                        t.high      = high_,
                                        t.low       = low_,
                                        t.volume    = volume_,
                                        t.exchange  = exchange_,
                                        t.symbol    = symbol_,
                                        t.timeframe = timeframe_)
                                   .where(t.id == parameter(t.id));

            // Log the update statement
            // CipherDB::SQLLogger::getInstance().logStatement(update_stmt, "UPDATE");

            // Prepare and log parameter binding
            // CipherDB::SQLLogger::getInstance().logPrepared(update_stmt, "id", uuid_binary);

            // Prepare, bind and execute
            auto uprep      = conn.prepare(update_stmt);
            uprep.params.id = uuid_binary;
            conn(uprep);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error saving candle: " << e.what() << std::endl;
        return false;
    }
}

// Find candle by ID
std::optional< CipherDB::Candle > CipherDB::Candle::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                             const boost::uuids::uuid& id)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // Convert Boost UUID to string for query
        std::string uuid_str = boost::uuids::to_string(id);

        // Use prepared statements for consistency with other methods
        auto prep = conn.prepare(select(all_of(t)).from(t).where(t.id == parameter(t.id)));

        // Bind the parameter
        prep.params.id = uuid_str;

        // Execute the prepared statement
        auto result = conn(prep);

        if (result.empty())
        {
            return std::nullopt;
        }

        const auto& row = *result.begin();

        Candle candle;

        // Parse the UUID string into a Boost UUID
        try
        {
            candle.id_ = boost::uuids::string_generator()(row.id.value());
        }
        catch (const std::runtime_error& e)
        {
            // TODO: Log the error
            std::cerr << "Error casting uuid: " << e.what() << std::endl;
            return std::nullopt;
        }

        candle.timestamp_ = row.timestamp;
        candle.open_      = row.open;
        candle.close_     = row.close;
        candle.high_      = row.high;
        candle.low_       = row.low;
        candle.volume_    = row.volume;
        candle.exchange_  = row.exchange;
        candle.symbol_    = row.symbol;
        candle.timeframe_ = row.timeframe;

        return candle;
    }
    catch (const std::exception& e)
    {
        // TODO: Log the error
        std::cerr << "Error finding candle by ID: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional< std::vector< CipherDB::Candle > > CipherDB::Candle::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // We need to use sqlpp11's prepared statement functionality properly
        // First, construct the query using sqlpp11's API
        auto query = dynamic_select(conn, all_of(t)).from(t).dynamic_where();

        // Add conditions based on filter
        if (filter.id_)
        {
            query.where.add(t.id == boost::uuids::to_string(*filter.id_));
        }
        if (filter.timestamp_)
        {
            query.where.add(t.timestamp == *filter.timestamp_);
        }
        if (filter.open_)
        {
            query.where.add(t.open == *filter.open_);
        }
        if (filter.close_)
        {
            query.where.add(t.close == *filter.close_);
        }
        if (filter.high_)
        {
            query.where.add(t.high == *filter.high_);
        }
        if (filter.low_)
        {
            query.where.add(t.low == *filter.low_);
        }
        if (filter.volume_)
        {
            query.where.add(t.volume == *filter.volume_);
        }
        if (filter.exchange_)
        {
            query.where.add(t.exchange == *filter.exchange_);
        }
        if (filter.symbol_)
        {
            query.where.add(t.symbol == *filter.symbol_);
        }
        if (filter.timeframe_)
        {
            query.where.add(t.timeframe == *filter.timeframe_);
        }

        // Execute the query and get the result set
        auto rows = conn(query);

        // Process results
        std::vector< Candle > results;

        // Iterate through the result set and create Candle objects
        for (const auto& row : rows)
        {
            Candle candle;
            try
            {
                candle.id_        = boost::uuids::string_generator()(row.id.value());
                candle.timestamp_ = row.timestamp;
                candle.open_      = row.open;
                candle.close_     = row.close;
                candle.high_      = row.high;
                candle.low_       = row.low;
                candle.volume_    = row.volume;
                candle.exchange_  = row.exchange;
                candle.symbol_    = row.symbol;
                candle.timeframe_ = row.timeframe;

                results.push_back(std::move(candle));
            }
            catch (const std::exception& e)
            {
                // TODO: LOG
                std::cerr << "Error processing row: " << e.what() << std::endl;
                return std::nullopt;
            }
        }

        return results;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error in findByFilter: " << e.what() << std::endl;
        return std::nullopt;
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

bool CipherDB::ClosedTrade::save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
{
    // TODO:
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // Convert UUID to string
        std::string uuid_binary = boost::uuids::to_string(id_);

        // Check if trade already exists
        auto select_stmt = select(sqlpp::count(t.id)).from(t).where(t.id == parameter(t.id));
        auto sprep       = conn.prepare(select_stmt);
        sprep.params.id  = uuid_binary;
        auto result      = conn(sprep);

        if (result.empty() || result.front().count.value() == 0)
        {
            // Create insert statement
            auto insert_stmt = insert_into(t).set(t.id            = uuid_binary,
                                                  t.strategy_name = strategy_name_,
                                                  t.symbol        = symbol_,
                                                  t.exchange      = exchange_,
                                                  t.type          = type_,
                                                  t.timeframe     = timeframe_,
                                                  t.opened_at     = opened_at_,
                                                  t.closed_at     = closed_at_,
                                                  t.leverage      = leverage_);

            // Execute insert
            conn(insert_stmt);
        }
        else
        {
            // Create update statement
            auto update_stmt = update(t)
                                   .set(t.strategy_name = strategy_name_,
                                        t.symbol        = symbol_,
                                        t.exchange      = exchange_,
                                        t.type          = type_,
                                        t.timeframe     = timeframe_,
                                        t.opened_at     = opened_at_,
                                        t.closed_at     = closed_at_,
                                        t.leverage      = leverage_)
                                   .where(t.id == parameter(t.id));

            // Prepare, bind, and execute
            auto uprep      = conn.prepare(update_stmt);
            uprep.params.id = uuid_binary;
            conn(uprep);
        }

        // TODO: Implement saving of orders to a related table

        return true;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error saving closed trade: " << e.what() << std::endl;
        return false;
    }
}

std::optional< CipherDB::ClosedTrade > CipherDB::ClosedTrade::findById(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const boost::uuids::uuid& id)
{
    // TODO:
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // Convert UUID to string
        std::string uuid_str = boost::uuids::to_string(id);

        // Prepare statement
        auto prep      = conn.prepare(select(all_of(t)).from(t).where(t.id == parameter(t.id)));
        prep.params.id = uuid_str;

        // Execute
        auto result = conn(prep);

        if (result.empty())
        {
            return std::nullopt;
        }

        const auto& row = *result.begin();

        ClosedTrade trade;

        try
        {
            trade.id_ = boost::uuids::string_generator()(row.id.value());
        }
        catch (const std::runtime_error& e)
        {
            // TODO: LOG
            std::cerr << "Error parsing UUID: " << e.what() << std::endl;
            return std::nullopt;
        }

        trade.strategy_name_ = row.strategy_name;
        trade.symbol_        = row.symbol;
        trade.exchange_      = row.exchange;
        trade.type_          = row.type;
        trade.timeframe_     = row.timeframe;
        trade.opened_at_     = row.opened_at;
        trade.closed_at_     = row.closed_at;
        trade.leverage_      = row.leverage;

        // TODO: Load orders from related table

        return trade;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error finding closed trade by ID: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional< std::vector< CipherDB::ClosedTrade > > CipherDB::ClosedTrade::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = table();

        // Build dynamic query
        auto query = dynamic_select(conn, all_of(t)).from(t).dynamic_where();

        // Add filter conditions
        if (filter.id_)
        {
            query.where.add(t.id == boost::uuids::to_string(*filter.id_));
        }
        if (filter.strategy_name_)
        {
            query.where.add(t.strategy_name == *filter.strategy_name_);
        }
        if (filter.symbol_)
        {
            query.where.add(t.symbol == *filter.symbol_);
        }
        if (filter.exchange_)
        {
            query.where.add(t.exchange == *filter.exchange_);
        }
        if (filter.type_)
        {
            query.where.add(t.type == *filter.type_);
        }
        if (filter.timeframe_)
        {
            query.where.add(t.timeframe == *filter.timeframe_);
        }
        if (filter.opened_at_)
        {
            query.where.add(t.opened_at == *filter.opened_at_);
        }
        if (filter.closed_at_)
        {
            query.where.add(t.closed_at == *filter.closed_at_);
        }
        // Execute the query and get the result set

        auto rows = conn(query);

        // Process results
        std::vector< ClosedTrade > results;

        // Iterate through the result set and create Candle objects
        for (const auto& row : rows)
        {
            ClosedTrade closedTrade;
            try
            {
                closedTrade.id_            = boost::uuids::string_generator()(row.id.value());
                closedTrade.strategy_name_ = row.strategy_name;
                closedTrade.symbol_        = row.symbol;
                closedTrade.exchange_      = row.exchange;
                closedTrade.type_          = row.type;
                closedTrade.timeframe_     = row.timeframe;
                closedTrade.opened_at_     = row.opened_at;
                closedTrade.closed_at_     = row.closed_at;
                closedTrade.leverage_      = row.leverage;

                results.push_back(std::move(closedTrade));
            }
            catch (const std::exception& e)
            {
                // TODO: LOG
                std::cerr << "Error processing row: " << e.what() << std::endl;
                return std::nullopt;
            }
        }

        return results;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error in findByFilter: " << e.what() << std::endl;
        return std::nullopt;
    }
}
