#ifndef CIPHER_DB_HPP
#define CIPHER_DB_HPP

#include <any>
#include <memory>
#include <string>
#include <boost/uuid.hpp>
#include <sqlpp11/char_sequence.h>
#include <sqlpp11/data_types.h>
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/connection_config.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/table.h>

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
        config->debug    = true; // TODO:
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

} // namespace db
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

    // Database operations
    bool save();
    static std::optional< Candle > findById(const boost::uuids::uuid& id);

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

    // New findByFilter that takes a Filter object
    static std::optional< std::vector< Candle > > findByFilter(const Filter& filter);

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

    // TODO: Is it a good idea to make it singleton?
    // Static singleton table instance for sqlpp11
    static const CandlesTable& table()
    {
        static const CandlesTable instance{};
        return instance;
    }
};
} // namespace CipherDB

#endif
