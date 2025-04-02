#include "DB.hpp"
#include <optional>
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
#include <sqlpp11/select.h>
#include <sqlpp11/update.h>

// For prepared statements (if using that approach)
#include <sqlpp11/prepared_insert.h>
#include <sqlpp11/prepared_select.h>
#include <sqlpp11/prepared_update.h>

// For Boost UUID
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sqlpp11/where.h>


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

// Save the candle to the database
bool CipherDB::Candle::save()
{
    try
    {
        auto& db      = CipherDB::db::Database::getInstance().getConnection();
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
        auto sprep = db.prepare(select_stmt);

        // Log parameter binding
        // CipherDB::SQLLogger::getInstance().logPrepared(select_stmt, "id", uuid_binary);

        // Bind the parameter value
        sprep.params.id = uuid_binary;

        // Execute the prepared statement
        auto result = db(sprep);

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
            db(insert_stmt);
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
            auto uprep      = db.prepare(update_stmt);
            uprep.params.id = uuid_binary;
            db(uprep);
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
std::optional< CipherDB::Candle > CipherDB::Candle::findById(const boost::uuids::uuid& id)
{
    try
    {
        auto& db      = db::Database::getInstance().getConnection();
        const auto& t = table();

        // Convert Boost UUID to string for query
        std::string uuid_str = boost::uuids::to_string(id);

        // Use prepared statements for consistency with other methods
        auto prep = db.prepare(select(all_of(t)).from(t).where(t.id == parameter(t.id)));

        // Bind the parameter
        prep.params.id = uuid_str;

        // Execute the prepared statement
        auto result = db(prep);

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

std::optional< std::vector< CipherDB::Candle > > CipherDB::Candle::findByFilter(const Filter& filter)
{
    try
    {
        // Get a connection from the database pool
        auto& db      = db::Database::getInstance().getConnection();
        const auto& t = table();

        // We need to use sqlpp11's prepared statement functionality properly
        // First, construct the query using sqlpp11's API
        auto query = dynamic_select(db, all_of(t)).from(t).dynamic_where();

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
        auto rows = db(query);

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
