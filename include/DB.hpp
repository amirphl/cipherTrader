#ifndef CIPHER_DB_HPP
#define CIPHER_DB_HPP

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "DynamicArray.hpp"
#include <blaze/Math.h>
#include <boost/uuid.hpp>
#include <boost/uuid/uuid.hpp>
#include <sqlpp11/char_sequence.h>
#include <sqlpp11/data_types.h>
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/connection_config.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/table.h>
#include <sqlpp11/transaction.h>

namespace CipherDB
{
namespace db
{

// Thread-safe connection pool for PostgreSQL
class ConnectionPool
{
   public:
    // Get singleton instance
    static ConnectionPool& getInstance()
    {
        static ConnectionPool instance;
        return instance;
    }

    // Initialize the connection pool
    void init(const std::string& host,
              const std::string& dbname,
              const std::string& username,
              const std::string& password,
              unsigned int port = 5432,
              size_t poolSize   = 10) // Default pool size
    {
        std::lock_guard< std::mutex > lock(mutex_);

        // Store connection parameters for creating new connections
        host_     = host;
        dbname_   = dbname;
        username_ = username;
        password_ = password;
        port_     = port;

        // Initialize the pool with connections
        for (size_t i = 0; i < poolSize; ++i)
        {
            createNewConnection();
        }
    }

    // Get a connection from the pool (or create one if pool is empty)
    std::shared_ptr< sqlpp::postgresql::connection > getConnection()
    {
        std::unique_lock< std::mutex > lock(mutex_);

        if (!initialized_)
        {
            throw std::runtime_error("Connection pool not initialized");
        }

        // Wait for a connection to become available
        while (availableConnections_.empty())
        {
            // If we've reached max connections, wait for one to be returned
            if (activeConnections_ >= maxConnections_)
            {
                connectionAvailable_.wait(lock);
            }
            else
            {
                // Create a new connection if below max limit
                createNewConnection();
            }
        }

        // Get a connection from the pool
        auto conn = std::move(availableConnections_.front());
        availableConnections_.pop();

        // Save the raw pointer before transferring ownership
        sqlpp::postgresql::connection* rawConn = conn.get();

        // IMPORTANT: Add to managed connections before creating the shared_ptr
        managedConnections_.push_back(std::move(conn));
        activeConnections_++;

        // Create a wrapper that returns the connection to the pool when it's destroyed
        return std::shared_ptr< sqlpp::postgresql::connection >(
            rawConn, [this](sqlpp::postgresql::connection* conn) { this->returnConnection(conn); });
    }

    // Return a connection to the pool
    void returnConnection(sqlpp::postgresql::connection* conn)
    {
        std::lock_guard< std::mutex > lock(mutex_);

        // Find the connection in our managed connections and move it back to available
        auto it = std::find_if(managedConnections_.begin(),
                               managedConnections_.end(),
                               [conn](const auto& managed) { return managed.get() == conn; });

        if (it != managedConnections_.end())
        {
            availableConnections_.push(std::move(*it));
            managedConnections_.erase(it);
            activeConnections_--;

            // Notify waiting threads that a connection is available
            connectionAvailable_.notify_one();
        }
    }

    // Set maximum number of connections
    void setMaxConnections(size_t maxConnections)
    {
        std::lock_guard< std::mutex > lock(mutex_);
        maxConnections_ = maxConnections;
    }

    // Delete copy constructor and assignment operator
    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

   private:
    // Private constructor for singleton
    ConnectionPool() : activeConnections_(0), maxConnections_(20), initialized_(false) {}

    // Create a new database connection
    void createNewConnection()
    {
        auto config      = std::make_shared< sqlpp::postgresql::connection_config >();
        config->debug    = false; // TODO:
        config->host     = host_;
        config->dbname   = dbname_;
        config->user     = username_;
        config->password = password_;
        config->port     = port_;

        auto conn = std::make_unique< sqlpp::postgresql::connection >(config);
        availableConnections_.push(std::move(conn));
        initialized_ = true;
    }

    // Connection pool members
    std::mutex mutex_;
    std::condition_variable connectionAvailable_;
    std::queue< std::unique_ptr< sqlpp::postgresql::connection > > availableConnections_;
    std::vector< std::unique_ptr< sqlpp::postgresql::connection > > managedConnections_;
    size_t activeConnections_;
    size_t maxConnections_;
    bool initialized_;

    // Connection parameters
    std::string host_;
    std::string dbname_;
    std::string username_;
    std::string password_;
    unsigned int port_;
};

// For backward compatibility, keep a simpler interface
class Database
{
   public:
    // Get singleton instance
    static Database& getInstance()
    {
        static Database instance;
        return instance;
    }

    // Initialize the database with connection parameters
    void init(const std::string& host,
              const std::string& dbname,
              const std::string& username,
              const std::string& password,
              unsigned int port = 5432)
    {
        ConnectionPool::getInstance().init(host, dbname, username, password, port);
    }

    // Get a connection from the pool
    sqlpp::postgresql::connection& getConnection()
    {
        // Get connection from pool and cache it in thread_local storage
        thread_local std::shared_ptr< sqlpp::postgresql::connection > conn =
            ConnectionPool::getInstance().getConnection();

        return *conn;
    }

    // Delete copy constructor and assignment operator
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

   private:
    // Private constructor for singleton
    Database() = default;
};

class TransactionManager
{
   public:
    // Start a new transaction
    static std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > startTransaction()
    {
        auto& db = db::Database::getInstance().getConnection();
        return std::make_shared< sqlpp::transaction_t< sqlpp::postgresql::connection > >(start_transaction(db));
    }

    // Commit the transaction
    static bool commitTransaction(std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx)
    {
        if (!tx)
        {
            // TODO: LOG
            std::cerr << "Cannot commit null transaction" << std::endl;
            return false;
        }

        try
        {
            tx->commit();
            return true;
        }
        catch (const std::exception& e)
        {
            // TODO: LOG
            std::cerr << "Error committing transaction: " << e.what() << std::endl;
            return false;
        }
    }

    // Roll back the transaction
    static bool rollbackTransaction(std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx)
    {
        if (!tx)
        {
            // TODO: LOG
            std::cerr << "Cannot rollback null transaction" << std::endl;
            return false;
        }

        try
        {
            // TODO: LOG
            tx->rollback();
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error rolling back transaction: " << e.what() << std::endl;
            return false;
        }
    }
};

// RAII-style transaction guard
class TransactionGuard
{
   public:
    TransactionGuard() : committed_(false)
    {
        // Get a connection from the pool
        conn_ = CipherDB::db::ConnectionPool::getInstance().getConnection();

        // Start a transaction on this connection
        if (conn_)
        {
            tx_ = std::make_shared< sqlpp::transaction_t< sqlpp::postgresql::connection > >(start_transaction(*conn_));
        }
    }

    ~TransactionGuard()
    {
        if (tx_ && !committed_)
        {
            try
            {
                tx_->rollback();
            }
            catch (const std::exception& e)
            {
                // TODO: LOG
                std::cerr << "Error during auto-rollback: " << e.what() << std::endl;
            }
        }
    }

    bool commit()
    {
        if (!tx_)
            return false;

        try
        {
            tx_->commit();
            committed_ = true;
            return true;
        }
        catch (const std::exception& e)
        {
            // TODO: LOG
            std::cerr << "Error during commit: " << e.what() << std::endl;
            return false;
        }
    }

    bool rollback()
    {
        if (!tx_)
            return false;

        try
        {
            tx_->rollback();
            return true;
        }
        catch (const std::exception& e)
        {
            // TODO: LOG
            std::cerr << "Error during rollback: " << e.what() << std::endl;
            return false;
        }
    }

    // Get the connection associated with this transaction
    std::shared_ptr< sqlpp::postgresql::connection > getConnection() { return conn_; }

    // Delete copy constructor/assignment
    TransactionGuard(const TransactionGuard&)            = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

   private:
    std::shared_ptr< sqlpp::postgresql::connection > conn_;
    std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx_;
    bool committed_;
};


// Generic findById implementation
template < typename ModelType >
std::optional< ModelType > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                    const boost::uuids::uuid& id)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : Database::getInstance().getConnection();
        const auto& t = ModelType::table();

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

        // Create and populate a new model instance
        const auto& row = *result.begin();
        return ModelType::fromRow(row);
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error finding " << ModelType::modelName() << " by ID: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Generic findByFilter implementation
template < typename ModelType, typename FilterType >
std::optional< std::vector< ModelType > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                       const FilterType& filter)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = ModelType::table();

        // Build dynamic query
        auto query = dynamic_select(conn, all_of(t)).from(t).dynamic_where();

        // Apply filter conditions
        filter.applyToQuery(query, t);

        // Execute query
        auto rows = conn(query);

        // Process results
        std::vector< ModelType > results;
        for (const auto& row : rows)
        {
            try
            {
                results.push_back(std::move(ModelType::fromRow(row)));
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
        std::cerr << "Error in findByFilter for " << ModelType::modelName() << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Generic save implementation
template < typename ModelType >
bool save(ModelType& model, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
{
    try
    {
        // Use the provided connection if available, otherwise get the default connection
        auto& conn    = conn_ptr ? *conn_ptr : CipherDB::db::Database::getInstance().getConnection();
        const auto& t = ModelType::table();

        // Convert UUID to string
        std::string uuid_str = model.getIdAsString();

        // Check if record exists
        auto select_stmt = select(sqlpp::count(t.id)).from(t).where(t.id == parameter(t.id));
        auto sprep       = conn.prepare(select_stmt);
        sprep.params.id  = uuid_str;
        auto result      = conn(sprep);

        bool exists = !result.empty() && result.front().count.value() > 0;

        if (!exists)
        {
            // Insert
            auto insert_stmt = model.prepareInsertStatement(t);
            conn(insert_stmt);
        }
        else
        {
            // Update
            auto update_stmt = model.prepareUpdateStatement(t);
            auto uprep       = conn.prepare(update_stmt);
            uprep.params.id  = uuid_str;
            conn(uprep);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        // TODO: LOG
        std::cerr << "Error saving " << ModelType::modelName() << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace db
} // namespace CipherDB

namespace CipherDB
{
class Order
{
   public:
    // TODO:
    std::string id;
    std::string side; // "buy" or "sell"
    double price;
    double qty;
    int64_t timestamp;

    // TODO:
    std::unordered_map< std::string, std::any > toJson() const { return {}; };
};
} // namespace CipherDB

namespace CipherDB
{
// Define the Candle table structure for sqlpp11
namespace candle
{
// Modern column definition style
// Each column has a definition with required components
struct Timestamp
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "timestamp";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T timestamp;
            T& operator()() { return timestamp; }
            const T& operator()() const { return timestamp; }
        };
    };
    // Add the column tag (new style)
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};

struct Open
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "open";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T open;
            T& operator()() { return open; }
            const T& operator()() const { return open; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct Close
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "close";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T close;
            T& operator()() { return close; }
            const T& operator()() const { return close; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct High
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "high";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T high;
            T& operator()() { return high; }
            const T& operator()() const { return high; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct Low
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "low";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T low;
            T& operator()() { return low; }
            const T& operator()() const { return low; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct Volume
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "volume";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T volume;
            T& operator()() { return volume; }
            const T& operator()() const { return volume; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct Exchange
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "exchange";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T exchange;
            T& operator()() { return exchange; }
            const T& operator()() const { return exchange; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Symbol
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "symbol";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T symbol;
            T& operator()() { return symbol; }
            const T& operator()() const { return symbol; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Timeframe
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "timeframe";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T timeframe;
            T& operator()() { return timeframe; }
            const T& operator()() const { return timeframe; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Id
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "id";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T id;
            T& operator()() { return id; }
            const T& operator()() const { return id; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};
} // namespace candle

// Define the Table
struct CandlesTable
    : sqlpp::table_t< CandlesTable,
                      candle::Id,
                      candle::Timestamp,
                      candle::Open,
                      candle::Close,
                      candle::High,
                      candle::Low,
                      candle::Volume,
                      candle::Exchange,
                      candle::Symbol,
                      candle::Timeframe >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "candles";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T candles;
            T& operator()() { return candles; }
            const T& operator()() const { return candles; }
        };
    };
};

class Candle
{
   public:
    // Constructors
    Candle();
    explicit Candle(const std::unordered_map< std::string, std::any >& attributes);

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    double getOpen() const { return open_; }
    void setOpen(double open) { open_ = open; }

    double getClose() const { return close_; }
    void setClose(double close) { close_ = close; }

    double getHigh() const { return high_; }
    void setHigh(double high) { high_ = high; }

    double getLow() const { return low_; }
    void setLow(double low) { low_ = low; }

    double getVolume() const { return volume_; }
    void setVolume(double volume) { volume_ = volume; }

    const std::string& getExchange() const { return exchange_; }
    void setExchange(const std::string& exchange) { exchange_ = exchange; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const std::string& getTimeframe() const { return timeframe_; }
    void setTimeframe(const std::string& timeframe) { timeframe_ = timeframe; }

    static inline auto table() { return CandlesTable{}; }
    static inline std::string modelName() { return "Candle"; }

    // static const CandlesTable& table()
    // {
    //     static const CandlesTable instance{};
    //     return instance;
    // }

    // Convert DB row to model instance
    template < typename ROW >
    static Candle fromRow(const ROW& row)
    {
        Candle candle;
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
        return candle;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const CandlesTable& t) const
    {
        return insert_into(t).set(t.id        = getIdAsString(),
                                  t.timestamp = timestamp_,
                                  t.open      = open_,
                                  t.close     = close_,
                                  t.high      = high_,
                                  t.low       = low_,
                                  t.volume    = volume_,
                                  t.exchange  = exchange_,
                                  t.symbol    = symbol_,
                                  t.timeframe = timeframe_);
    }

    // Prepare update statement
    auto prepareUpdateStatement(const CandlesTable& t) const
    {
        return update(t)
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
    }

    // Simplified public interface methods that use the generic functions
    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< Candle > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                            const boost::uuids::uuid& id)
    {
        return db::findById< Candle >(conn_ptr, id);
    }

    // Flag for partial candles
    // static constexpr bool is_partial = true;

    // Add these to your existing Candle class declaration in DB.hpp
   public:
    // Query builder for flexible filtering
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withTimestamp(int64_t timestamp)
        {
            timestamp_ = timestamp;
            return *this;
        }

        Filter& withOpen(double open)
        {
            open_ = open;
            return *this;
        }

        Filter& withClose(double close)
        {
            close_ = close;
            return *this;
        }

        Filter& withHigh(double high)
        {
            high_ = high;
            return *this;
        }

        Filter& withLow(double low)
        {
            low_ = low;
            return *this;
        }

        Filter& withVolume(double volume)
        {
            volume_ = volume;
            return *this;
        }

        Filter& withExchange(std::string exchange)
        {
            exchange_ = std::move(exchange);
            return *this;
        }

        Filter& withSymbol(std::string symbol)
        {
            symbol_ = std::move(symbol);
            return *this;
        }

        Filter& withTimeframe(std::string timeframe)
        {
            timeframe_ = std::move(timeframe);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (timestamp_)
            {
                query.where.add(t.timestamp == *timestamp_);
            }
            if (open_)
            {
                query.where.add(t.open == *open_);
            }
            if (close_)
            {
                query.where.add(t.close == *close_);
            }
            if (high_)
            {
                query.where.add(t.high == *high_);
            }
            if (low_)
            {
                query.where.add(t.low == *low_);
            }
            if (volume_)
            {
                query.where.add(t.volume == *volume_);
            }
            if (exchange_)
            {
                query.where.add(t.exchange == *exchange_);
            }
            if (symbol_)
            {
                query.where.add(t.symbol == *symbol_);
            }
            if (timeframe_)
            {
                query.where.add(t.timeframe == *timeframe_);
            }
        }

       private:
        friend class Candle;
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< double > open_;
        std::optional< double > close_;
        std::optional< double > high_;
        std::optional< double > low_;
        std::optional< double > volume_;
        std::optional< std::string > exchange_;
        std::optional< std::string > symbol_;
        std::optional< std::string > timeframe_;
    };

    // Static factory method for creating a filter
    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Candle > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< Candle, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    int64_t timestamp_ = 0;
    double open_       = 0.0;
    double close_      = 0.0;
    double high_       = 0.0;
    double low_        = 0.0;
    double volume_     = 0.0;
    std::string exchange_;
    std::string symbol_;
    std::string timeframe_;
};

namespace closed_trade
{
struct Id
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "id";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T id;
            T& operator()() { return id; }
            const T& operator()() const { return id; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct StrategyName
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "strategy_name";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T strategy_name;
            T& operator()() { return strategy_name; }
            const T& operator()() const { return strategy_name; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Symbol
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "symbol";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T symbol;
            T& operator()() { return symbol; }
            const T& operator()() const { return symbol; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Exchange
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "exchange";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T exchange;
            T& operator()() { return exchange; }
            const T& operator()() const { return exchange; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Type
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "type";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T type;
            T& operator()() { return type; }
            const T& operator()() const { return type; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Timeframe
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "timeframe";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T timeframe;
            T& operator()() { return timeframe; }
            const T& operator()() const { return timeframe; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct OpenedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "opened_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T opened_at;
            T& operator()() { return opened_at; }
            const T& operator()() const { return opened_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};

struct ClosedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "closed_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T closed_at;
            T& operator()() { return closed_at; }
            const T& operator()() const { return closed_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};

struct Leverage
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "leverage";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T leverage;
            T& operator()() { return leverage; }
            const T& operator()() const { return leverage; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::integer >;
};
} // namespace closed_trade

// Define the Table
struct ClosedTradesTable
    : sqlpp::table_t< ClosedTradesTable,
                      closed_trade::Id,
                      closed_trade::StrategyName,
                      closed_trade::Symbol,
                      closed_trade::Exchange,
                      closed_trade::Type,
                      closed_trade::Timeframe,
                      closed_trade::OpenedAt,
                      closed_trade::ClosedAt,
                      closed_trade::Leverage >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "closed_trades";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T closed_trades;
            T& operator()() { return closed_trades; }
            const T& operator()() const { return closed_trades; }
        };
    };
};

class ClosedTrade
{
   public:
    // Constructors
    ClosedTrade();
    explicit ClosedTrade(const std::unordered_map< std::string, std::any >& attributes);

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const;
    void setId(const std::string& id_str);

    const std::string& getStrategyName() const { return strategy_name_; }
    void setStrategyName(const std::string& strategy_name) { strategy_name_ = strategy_name; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const std::string& getExchange() const { return exchange_; }
    void setExchange(const std::string& exchange) { exchange_ = exchange; }

    const std::string& getType() const { return type_; }
    void setType(const std::string& type) { type_ = type; }

    const std::string& getTimeframe() const { return timeframe_; }
    void setTimeframe(const std::string& timeframe) { timeframe_ = timeframe; }

    int64_t getOpenedAt() const { return opened_at_; }
    void setOpenedAt(int64_t opened_at) { opened_at_ = opened_at; }

    int64_t getClosedAt() const { return closed_at_; }
    void setClosedAt(int64_t closed_at) { closed_at_ = closed_at; }

    int getLeverage() const { return leverage_; }
    void setLeverage(int leverage) { leverage_ = leverage; }

    // Order management
    void addBuyOrder(double qty, double price);
    void addSellOrder(double qty, double price);
    void addOrder(const Order& order);

    // Computed properties
    double getQty() const;
    double getEntryPrice() const;
    double getExitPrice() const;
    double getFee() const;
    double getSize() const;
    double getPnl() const;
    double getPnlPercentage() const;
    double getRoi() const; // Alias for getPnlPercentage
    double getTotalCost() const;
    int getHoldingPeriod() const;
    bool isLong() const;
    bool isShort() const;
    bool isOpen() const;

    // JSON conversion
    // TODO: Json?
    std::unordered_map< std::string, std::any > toJson() const;
    std::unordered_map< std::string, std::any > toJsonWithOrders() const;

    static inline auto table() { return ClosedTradesTable{}; }
    static inline std::string modelName() { return "ClosedTrade"; }

    // Static singleton table instance for sqlpp11
    // static const ClosedTradesTable& table()
    // {
    //     static const ClosedTradesTable instance{};
    //     return instance;
    // };

    // Convert DB row to model instance
    template < typename ROW >
    static ClosedTrade fromRow(const ROW& row)
    {
        ClosedTrade closedTrade;

        closedTrade.id_            = boost::uuids::string_generator()(row.id.value());
        closedTrade.strategy_name_ = row.strategy_name;
        closedTrade.symbol_        = row.symbol;
        closedTrade.exchange_      = row.exchange;
        closedTrade.type_          = row.type;
        closedTrade.timeframe_     = row.timeframe;
        closedTrade.opened_at_     = row.opened_at;
        closedTrade.closed_at_     = row.closed_at;
        closedTrade.leverage_      = row.leverage;

        return closedTrade;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const ClosedTradesTable& t) const
    {
        return insert_into(t).set(t.id            = getIdAsString(),
                                  t.strategy_name = strategy_name_,
                                  t.symbol        = symbol_,
                                  t.exchange      = exchange_,
                                  t.type          = type_,
                                  t.timeframe     = timeframe_,
                                  t.opened_at     = opened_at_,
                                  t.closed_at     = closed_at_,
                                  t.leverage      = leverage_);
    }

    // Prepare update statement
    auto prepareUpdateStatement(const ClosedTradesTable& t) const
    {
        return update(t)
            .set(t.strategy_name = strategy_name_,
                 t.symbol        = symbol_,
                 t.exchange      = exchange_,
                 t.type          = type_,
                 t.timeframe     = timeframe_,
                 t.opened_at     = opened_at_,
                 t.closed_at     = closed_at_,
                 t.leverage      = leverage_)
            .where(t.id == parameter(t.id));
    }

    // Simplified public interface methods that use the generic functions
    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< ClosedTrade > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                 const boost::uuids::uuid& id)
    {
        return db::findById< ClosedTrade >(conn_ptr, id);
    }

    // Query builder for flexible filtering
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withStrategyName(std::string strategy_name)
        {
            strategy_name_ = std::move(strategy_name);
            return *this;
        }

        Filter& withSymbol(std::string symbol)
        {
            symbol_ = std::move(symbol);
            return *this;
        }

        Filter& withExchange(std::string exchange)
        {
            exchange_ = std::move(exchange);
            return *this;
        }

        Filter& withType(std::string type)
        {
            type_ = std::move(type);
            return *this;
        }

        Filter& withTimeframe(std::string timeframe)
        {
            timeframe_ = std::move(timeframe);
            return *this;
        }

        Filter& withOpenedAt(int64_t opened_at)
        {
            opened_at_ = opened_at;
            return *this;
        }

        Filter& withClosedAt(int64_t closed_at)
        {
            closed_at_ = closed_at;
            return *this;
        }

        Filter& withLeverage(int leverage)
        {
            leverage_ = leverage;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (strategy_name_)
            {
                query.where.add(t.strategy_name == *strategy_name_);
            }
            if (symbol_)
            {
                query.where.add(t.symbol == *symbol_);
            }
            if (exchange_)
            {
                query.where.add(t.exchange == *exchange_);
            }
            if (type_)
            {
                query.where.add(t.type == *type_);
            }
            if (timeframe_)
            {
                query.where.add(t.timeframe == *timeframe_);
            }
            if (opened_at_)
            {
                query.where.add(t.opened_at == *opened_at_);
            }
            if (closed_at_)
            {
                query.where.add(t.closed_at == *closed_at_);
            }
            if (leverage_)
            {
                query.where.add(t.leverage == *leverage_);
            }
        }

       private:
        friend class ClosedTrade;
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > strategy_name_;
        std::optional< std::string > symbol_;
        std::optional< std::string > exchange_;
        std::optional< std::string > type_;
        std::optional< std::string > timeframe_;
        std::optional< int64_t > opened_at_;
        std::optional< int64_t > closed_at_;
        std::optional< int > leverage_;
    };

    // Static factory method for creating a filter
    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< ClosedTrade > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< ClosedTrade, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    std::string strategy_name_;
    std::string symbol_;
    std::string exchange_;
    std::string type_;
    std::string timeframe_;
    int64_t opened_at_ = 0;
    int64_t closed_at_ = 0;
    int leverage_      = 1;

    // Using Blaze for fast numerical calculations
    CipherDynamicArray::DynamicBlazeArray< double > buy_orders_;
    CipherDynamicArray::DynamicBlazeArray< double > sell_orders_;
    std::vector< Order > orders_;
};
} // namespace CipherDB


#endif
