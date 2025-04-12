#ifndef CIPHER_DB_HPP
#define CIPHER_DB_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "DynamicArray.hpp"
#include <blaze/Math.h>
#include <boost/uuid.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>
#include <sqlpp11/char_sequence.h>
#include <sqlpp11/data_types.h>
#include <sqlpp11/data_types/blob/data_type.h>
#include <sqlpp11/insert.h>
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/connection_config.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/table.h>
#include <sqlpp11/transaction.h>
#include <sqlpp11/update.h>

namespace CipherDB
{
namespace db
{

class DatabaseShutdownManager
{
   public:
    using ShutdownHook           = std::function< void() >;
    using ShutdownCompletionHook = std::function< void() >;

    static DatabaseShutdownManager& getInstance()
    {
        static DatabaseShutdownManager instance;
        return instance;
    }

    // Register a hook to be called during shutdown
    void registerShutdownHook(ShutdownHook hook)
    {
        std::lock_guard< std::mutex > lock(hooksMutex_);
        shutdownHooks_.push_back(std::move(hook));
    }

    // Register a hook to be called after shutdown is complete
    void registerCompletionHook(ShutdownCompletionHook hook)
    {
        std::lock_guard< std::mutex > lock(completionHooksMutex_);
        completionHooks_.push_back(std::move(hook));
    }

    // Initialize signal handlers
    void initSignalHandlers()
    {
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);

        // Ignore SIGPIPE to prevent crashes on broken connections
        std::signal(SIGPIPE, SIG_IGN);
    }

    // Check if shutdown is in progress
    bool isShuttingDown() const { return shuttingDown_.load(std::memory_order_acquire); }

    // Wait for shutdown to complete
    void waitForShutdown()
    {
        if (shutdownFuture_.valid())
        {
            shutdownFuture_.wait();
        }
    }

    void shutdown()
    {
        // Only start shutdown once
        bool expected = false;
        if (!shuttingDown_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        // Launch shutdown process asynchronously
        shutdownFuture_ = std::async(std::launch::async, [this] { performShutdown(); });
    }

   private:
    DatabaseShutdownManager() : shuttingDown_(false) {}
    ~DatabaseShutdownManager() = default;

    DatabaseShutdownManager(const DatabaseShutdownManager&)            = delete;
    DatabaseShutdownManager& operator=(const DatabaseShutdownManager&) = delete;

    static void handleSignal(int signal) { getInstance().shutdown(); }

    void performShutdown();

    std::atomic< bool > shuttingDown_;
    std::mutex hooksMutex_;
    std::mutex completionHooksMutex_;
    std::vector< ShutdownHook > shutdownHooks_;
    std::vector< ShutdownCompletionHook > completionHooks_;
    std::future< void > shutdownFuture_;
};

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

        // Check if we're shutting down
        if (DatabaseShutdownManager::getInstance().isShuttingDown())
        {
            throw std::runtime_error("Database is shutting down");
        }

        // Wait for a connection to become available
        while (availableConnections_.empty())
        {
            // If we've reached max connections, wait for one to be returned
            if (activeConnections_ >= maxConnections_)
            {
                // TODO: Read timeout from config.
                if (!connectionAvailable_.wait_for(
                        lock, std::chrono::seconds(5), [this] { return !availableConnections_.empty(); }))
                {
                    throw std::runtime_error("Timeout waiting for connection");
                }
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

    // Get a connection from the pool with health check
    std::shared_ptr< sqlpp::postgresql::connection > getConnectionWithHealthCheck()
    {
        auto conn = getConnection();

        // Test if the connection is still valid
        try
        {
            conn->execute("SELECT 1");
        }
        catch (const std::exception& e)
        {
            // Connection is dead, get a new one
            std::cerr << "Detected dead connection, getting a new one: " << e.what() << std::endl;

            // This will release the dead connection and get a new one
            // The connection pool's returnConnection will handle creating a replacement
            conn = getConnection();
        }

        return conn;
    }

    // Set maximum number of connections
    void setMaxConnections(size_t maxConnections)
    {
        std::lock_guard< std::mutex > lock(mutex_);
        maxConnections_ = maxConnections;
    }

    void waitForConnectionsToClose()
    {
        std::unique_lock< std::mutex > lock(mutex_);

        // Wait until all connections are returned
        while (activeConnections_ > 0)
        {
            connectionReturned_.wait(lock);
        }
    }

    // Delete copy constructor and assignment operator
    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

   private:
    // Private constructor for singleton
    ConnectionPool() : activeConnections_(0), maxConnections_(20), initialized_(false) {}

    // Return a connection to the pool
    void returnConnection(sqlpp::postgresql::connection* conn)
    {
        std::lock_guard< std::mutex > lock(mutex_);

        // Check if connection is still valid before trying to clean it up
        bool connectionValid = true;
        try
        {
            // A simple ping to test if connection is alive
            conn->execute("SELECT 1");
        }
        catch (const std::exception& e)
        {
            // Connection is dead, mark it as invalid
            connectionValid = false;
            std::cerr << "Detected dead connection during return: " << e.what() << std::endl;
        }

        if (connectionValid)
        {
            // Only try to clean up if connection is still valid
            try
            {
                conn->execute("DEALLOCATE ALL");
            }
            catch (const std::exception& e)
            {
                // Log the error but continue
                std::cerr << "Error cleaning connection during return: " << e.what() << std::endl;
                connectionValid = false;
            }
        }

        // Find the connection in our managed connections
        auto it = std::find_if(managedConnections_.begin(),
                               managedConnections_.end(),
                               [conn](const auto& managed) { return managed.get() == conn; });

        if (it != managedConnections_.end())
        {
            if (!DatabaseShutdownManager::getInstance().isShuttingDown())
            {
                if (connectionValid)
                {
                    // Return to the pool only if valid
                    availableConnections_.push(std::move(*it));
                }
                else
                {
                    // Connection is invalid, create a new one to replace it
                    createNewConnection();
                }
            }

            managedConnections_.erase(it);
            activeConnections_--;

            // Notify both waiting threads and shutdown manager
            connectionAvailable_.notify_one();
            connectionReturned_.notify_all();
        }
    }

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
    std::condition_variable connectionReturned_;
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
        std::lock_guard< std::mutex > lock(mutex_);

        // Initialize connection pool
        ConnectionPool::getInstance().init(host, dbname, username, password, port);

        // Initialize shutdown manager
        auto& shutdownManager = DatabaseShutdownManager::getInstance();
        shutdownManager.initSignalHandlers();

        // Register shutdown hooks
        shutdownManager.registerShutdownHook([] { std::cout << "Database shutdown initiated..." << std::endl; });

        shutdownManager.registerCompletionHook([] { std::cout << "Database shutdown completed." << std::endl; });
    }

    // // Get a connection from the pool
    // sqlpp::postgresql::connection& getConnection()
    // {
    // // Get connection from pool and cache it in thread_local storage
    // thread_local std::shared_ptr< sqlpp::postgresql::connection > conn =
    //     ConnectionPool::getInstance().getConnection();

    // return *conn;
    // }

    std::shared_ptr< sqlpp::postgresql::connection > getConnection()
    {
        return ConnectionPool::getInstance().getConnection();
    }

    void shutdown()
    {
        DatabaseShutdownManager::getInstance().shutdown();
        DatabaseShutdownManager::getInstance().waitForShutdown();
    }

    // Delete copy constructor and assignment operator
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

   private:
    // Private constructor for singleton
    Database()  = default;
    ~Database() = default;

    std::mutex mutex_;
};

class TransactionManager
{
   public:
    // Start a new transaction
    static std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > startTransaction()
    {
        auto db = db::Database::getInstance().getConnection();
        return std::make_shared< sqlpp::transaction_t< sqlpp::postgresql::connection > >(start_transaction(*db));
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
        // Check for shutdown before starting transaction
        if (DatabaseShutdownManager::getInstance().isShuttingDown())
        {
            throw std::runtime_error("Cannot start new transaction during shutdown");
        }

        conn_ = ConnectionPool::getInstance().getConnection();

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

template < typename Func >
auto executeWithRetry(Func&& operation, int maxRetries = 3) -> decltype(operation())
{
    int retries = 0;
    while (true)
    {
        try
        {
            return operation();
        }
        catch (const std::exception& e)
        {
            if (++retries > maxRetries)
            {
                throw; // Re-throw after max retries
            }

            // TODO: LOG
            std::cerr << "Operation failed, retrying (" << retries << "/" << maxRetries << "): " << e.what()
                      << std::endl;

            // Exponential backoff
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * retries));
        }
    }
}

// RAII class for handling connection state
class ConnectionStateGuard
{
   public:
    explicit ConnectionStateGuard(sqlpp::postgresql::connection& conn) : conn_(conn), needs_reset_(false) {}

    ~ConnectionStateGuard()
    {
        if (needs_reset_)
        {
            try
            {
                // Deallocate all prepared statements
                conn_.execute("DEALLOCATE ALL");

                // Execute a harmless query to reset the connection state
                conn_.execute("SELECT 1");
            }
            catch (const std::exception& e)
            {
                // Nothing we can do in the destructor
            }
        }
    }

    void markForReset() { needs_reset_ = true; }

   private:
    sqlpp::postgresql::connection& conn_;
    bool needs_reset_;
};

// Generic findById implementation
template < typename ModelType >
std::optional< ModelType > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                    const boost::uuids::uuid& id)
{
    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = ModelType::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    try
    {
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

        // Mark the connection for reset
        stateGuard.markForReset();

        return std::nullopt;
    }
}

// Generic findByFilter implementation
template < typename ModelType, typename FilterType >
std::optional< std::vector< ModelType > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                       const FilterType& filter)
{
    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = ModelType::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    try
    {
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

        // Mark the connection for reset
        stateGuard.markForReset();

        return std::nullopt;
    }
}

// Generic save implementation
template < typename ModelType >
bool save(ModelType& model, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
{
    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = ModelType::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    try
    {
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
            auto insert_stmt = model.prepareInsertStatement(t, conn);
            conn(insert_stmt);
        }
        else
        {
            // Update
            auto update_stmt = model.prepareUpdateStatement(t, conn);
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

        // Mark the connection for reset
        stateGuard.markForReset();

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

    // Rule of five
    Candle(const Candle&)                = default;
    Candle(Candle&&) noexcept            = default;
    Candle& operator=(const Candle&)     = default;
    Candle& operator=(Candle&&) noexcept = default;
    ~Candle()                            = default;

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
    auto prepareInsertStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id        = getIdAsString(),
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
    auto prepareUpdateStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp = timestamp_,
                         t.open      = open_,
                         t.close     = close_,
                         t.high      = high_,
                         t.low       = low_,
                         t.volume    = volume_,
                         t.exchange  = exchange_,
                         t.symbol    = symbol_,
                         t.timeframe = timeframe_)
            .dynamic_where(t.id == parameter(t.id));
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

    // Rule of five
    ClosedTrade(const ClosedTrade&)                = default;
    ClosedTrade(ClosedTrade&&) noexcept            = default;
    ClosedTrade& operator=(const ClosedTrade&)     = default;
    ClosedTrade& operator=(ClosedTrade&&) noexcept = default;
    ~ClosedTrade()                                 = default;

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
    auto prepareInsertStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
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
    auto prepareUpdateStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.strategy_name = strategy_name_,
                         t.symbol        = symbol_,
                         t.exchange      = exchange_,
                         t.type          = type_,
                         t.timeframe     = timeframe_,
                         t.opened_at     = opened_at_,
                         t.closed_at     = closed_at_,
                         t.leverage      = leverage_)
            .dynamic_where(t.id == parameter(t.id));
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

// Define table structure for sqlpp11
namespace daily_balance
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
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};

struct Identifier
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "identifier";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T identifier;
            T& operator()() { return identifier; }
            const T& operator()() const { return identifier; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar, sqlpp::tag::can_be_null >;
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

struct Asset
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "asset";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T asset;
            T& operator()() { return asset; }
            const T& operator()() const { return asset; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Balance
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "balance";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T balance;
            T& operator()() { return balance; }
            const T& operator()() const { return balance; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};
} // namespace daily_balance

// Define the Table
struct DailyBalanceTable
    : sqlpp::table_t< DailyBalanceTable,
                      daily_balance::Id,
                      daily_balance::Timestamp,
                      daily_balance::Identifier,
                      daily_balance::Exchange,
                      daily_balance::Asset,
                      daily_balance::Balance >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "daily_balances";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T daily_balances;
            T& operator()() { return daily_balances; }
            const T& operator()() const { return daily_balances; }
        };
    };
};

class DailyBalance
{
   public:
    // Default constructor generates a random UUID
    DailyBalance();

    // Constructor with attribute map
    explicit DailyBalance(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five
    DailyBalance(const DailyBalance&)                = default;
    DailyBalance(DailyBalance&&) noexcept            = default;
    DailyBalance& operator=(const DailyBalance&)     = default;
    DailyBalance& operator=(DailyBalance&&) noexcept = default;
    ~DailyBalance()                                  = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    const std::optional< std::string >& getIdentifier() const { return identifier_; }
    void setIdentifier(const std::string& identifier) { identifier_ = identifier; }
    void clearIdentifier() { identifier_ = std::nullopt; }

    const std::string& getExchange() const { return exchange_; }
    void setExchange(const std::string& exchange) { exchange_ = exchange; }

    const std::string& getAsset() const { return asset_; }
    void setAsset(const std::string& asset) { asset_ = asset; }

    double getBalance() const { return balance_; }
    void setBalance(double balance) { balance_ = balance; }

    // Static methods for DB operations
    static inline auto table() { return DailyBalanceTable{}; }
    static inline std::string modelName() { return "DailyBalances"; }

    // Convert DB row to model instance
    template < typename ROW >
    static DailyBalance fromRow(const ROW& row)
    {
        DailyBalance balance;
        balance.id_        = boost::uuids::string_generator()(row.id.value());
        balance.timestamp_ = row.timestamp;

        if (!row.identifier.is_null())
            balance.identifier_ = row.identifier.value();
        else
            balance.identifier_ = std::nullopt;

        balance.exchange_ = row.exchange;
        balance.asset_    = row.asset;
        balance.balance_  = row.balance;

        return balance;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt = sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id        = getIdAsString(),
                                                                    t.timestamp = timestamp_,
                                                                    t.exchange  = exchange_,
                                                                    t.asset     = asset_,
                                                                    t.balance   = balance_);

        if (identifier_)
        {
            stmt.insert_list.add(t.identifier = *identifier_);
        }
        else
        {
            stmt.insert_list.add(t.identifier = sqlpp::null);
        }

        return stmt;
    }

    // Prepare update statement
    auto prepareUpdateStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt =
            sqlpp::dynamic_update(conn, t)
                .dynamic_set(t.timestamp = timestamp_, t.exchange = exchange_, t.asset = asset_, t.balance = balance_)
                .dynamic_where(t.id == parameter(t.id));

        if (identifier_)
        {
            stmt.assignments.add(t.identifier = *identifier_);
        }
        else
        {
            stmt.assignments.add(t.identifier = sqlpp::null);
        }

        return stmt;
    }

    // Simplified public interface methods that use the generic functions
    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< DailyBalance > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                  const boost::uuids::uuid& id)
    {
        return db::findById< DailyBalance >(conn_ptr, id);
    }

    // Filter class for complex queries
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

        Filter& withIdentifier(const std::string& identifier)
        {
            identifier_ = identifier;
            return *this;
        }

        Filter& withExchange(const std::string& exchange)
        {
            exchange_ = exchange;
            return *this;
        }

        Filter& withAsset(const std::string& asset)
        {
            asset_ = asset;
            return *this;
        }

        Filter& withBalance(double balance)
        {
            balance_ = balance;
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
            if (identifier_)
            {
                query.where.add(t.identifier == *identifier_);
            }
            if (exchange_)
            {
                query.where.add(t.exchange == *exchange_);
            }
            if (asset_)
            {
                query.where.add(t.asset == *asset_);
            }
            if (balance_)
            {
                query.where.add(t.balance == *balance_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > identifier_;
        std::optional< std::string > exchange_;
        std::optional< std::string > asset_;
        std::optional< double > balance_;
    };

    // Static factory method for creating a filter
    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< DailyBalance > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< DailyBalance, Filter >(conn_ptr, filter);
    }

   private:
    friend class DailyBalance;
    boost::uuids::uuid id_;
    int64_t timestamp_ = 0;
    std::optional< std::string > identifier_; // Can be null
    std::string exchange_;
    std::string asset_;
    double balance_ = 0.0;
};

// Define table structure for sqlpp11
namespace exchange_api_keys
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

struct ExchangeName
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "exchange_name";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T exchange_name;
            T& operator()() { return exchange_name; }
            const T& operator()() const { return exchange_name; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Name
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "name";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T name;
            T& operator()() { return name; }
            const T& operator()() const { return name; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct ApiKey
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "api_key";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T api_key;
            T& operator()() { return api_key; }
            const T& operator()() const { return api_key; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct ApiSecret
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "api_secret";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T api_secret;
            T& operator()() { return api_secret; }
            const T& operator()() const { return api_secret; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct AdditionalFields
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "additional_fields";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T additional_fields;
            T& operator()() { return additional_fields; }
            const T& operator()() const { return additional_fields; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct CreatedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "created_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T created_at;
            T& operator()() { return created_at; }
            const T& operator()() const { return created_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};
} // namespace exchange_api_keys

// Define the Table
struct ExchangeApiKeysTable
    : sqlpp::table_t< ExchangeApiKeysTable,
                      exchange_api_keys::Id,
                      exchange_api_keys::ExchangeName,
                      exchange_api_keys::Name,
                      exchange_api_keys::ApiKey,
                      exchange_api_keys::ApiSecret,
                      exchange_api_keys::AdditionalFields,
                      exchange_api_keys::CreatedAt >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "exchange_api_keys";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T exchange_api_keys;
            T& operator()() { return exchange_api_keys; }
            const T& operator()() const { return exchange_api_keys; }
        };
    };
};

class ExchangeApiKeys
{
   public:
    ExchangeApiKeys();

    explicit ExchangeApiKeys(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five
    ExchangeApiKeys(const ExchangeApiKeys&)                = default;
    ExchangeApiKeys(ExchangeApiKeys&&) noexcept            = default;
    ExchangeApiKeys& operator=(const ExchangeApiKeys&)     = default;
    ExchangeApiKeys& operator=(ExchangeApiKeys&&) noexcept = default;
    ~ExchangeApiKeys()                                     = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    const std::string& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const std::string& exchange_name) { exchange_name_ = exchange_name; }

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::string& getApiKey() const { return api_key_; }
    void setApiKey(const std::string& api_key) { api_key_ = api_key; }

    const std::string& getApiSecret() const { return api_secret_; }
    void setApiSecret(const std::string& api_secret) { api_secret_ = api_secret; }

    const std::string& getAdditionalFieldsJson() const { return additional_fields_; }
    void setAdditionalFieldsJson(const std::string& additional_fields)
    {
        try
        {
            // Attempt to parse the JSON string
            nlohmann::json parsedJson = nlohmann::json::parse(additional_fields);

            // Validate that the parsed JSON is an object, array, or null
            if (!parsedJson.is_object() && !parsedJson.is_array() && !parsedJson.is_null())
            {
                throw std::invalid_argument("Additional fields must be a JSON object, array, or null");
            }

            // If valid, store the JSON string
            additional_fields_ = additional_fields;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            // Rethrow with a more descriptive error message
            throw std::invalid_argument("Invalid JSON string: " + std::string(e.what()));
        }
    }

    int64_t getCreatedAt() const { return created_at_; }
    void setCreatedAt(int64_t created_at) { created_at_ = created_at; }

    // Get additional fields as a structured object
    nlohmann::json getAdditionalFields() const
    {
        return additional_fields_.empty() ? nlohmann::json::object() : nlohmann::json::parse(additional_fields_);
    }

    // Set additional fields from a structured object
    void setAdditionalFields(const nlohmann::json& fields) { additional_fields_ = fields.dump(); }

    // Static methods for database operations
    static inline auto table() { return ExchangeApiKeysTable{}; }
    static inline std::string modelName() { return "ExchangeApiKeys"; }

    // Convert DB row to model instance
    template < typename ROW >
    static ExchangeApiKeys fromRow(const ROW& row)
    {
        ExchangeApiKeys api_keys;
        api_keys.id_                = boost::uuids::string_generator()(row.id.value());
        api_keys.exchange_name_     = row.exchange_name;
        api_keys.name_              = row.name;
        api_keys.api_key_           = row.api_key;
        api_keys.api_secret_        = row.api_secret;
        api_keys.additional_fields_ = row.additional_fields;
        api_keys.created_at_        = row.created_at;
        return api_keys;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const ExchangeApiKeysTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id                = getIdAsString(),
                                                               t.exchange_name     = exchange_name_,
                                                               t.name              = name_,
                                                               t.api_key           = api_key_,
                                                               t.api_secret        = api_secret_,
                                                               t.additional_fields = additional_fields_,
                                                               t.created_at        = created_at_);
    }

    // Prepare update statement
    auto prepareUpdateStatement(const ExchangeApiKeysTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.exchange_name     = exchange_name_,
                         t.name              = name_,
                         t.api_key           = api_key_,
                         t.api_secret        = api_secret_,
                         t.additional_fields = additional_fields_,
                         t.created_at        = created_at_)
            .dynamic_where(t.id == parameter(t.id));
    }

    // Save to database
    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    // Find by ID
    static std::optional< ExchangeApiKeys > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                     const boost::uuids::uuid& id)
    {
        return db::findById< ExchangeApiKeys >(conn_ptr, id);
    }

    // Query builder for filtering
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withExchangeName(std::string exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withName(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == *exchange_name_);
            }
            if (name_)
            {
                query.where.add(t.name == *name_);
            }
        }

       private:
        friend class ExchangeApiKeys;
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > exchange_name_;
        std::optional< std::string > name_;
    };

    // Create a filter
    static Filter createFilter() { return Filter{}; }

    // Find by filter
    static std::optional< std::vector< ExchangeApiKeys > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< ExchangeApiKeys, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    std::string exchange_name_;
    std::string name_;
    std::string api_key_;
    std::string api_secret_;
    std::string additional_fields_ = "{}";
    int64_t created_at_;
};

namespace log
{
// Log type enum for type safety
enum class LogType : int16_t
{
    INFO    = 1,
    ERROR   = 2,
    WARNING = 3,
    DEBUG   = 4
};

// Column definitions using sqlpp11 modern style
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

struct SessionId
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "session_id";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T session_id;
            T& operator()() { return session_id; }
            const T& operator()() const { return session_id; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

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
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};

struct Message
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "message";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T message;
            T& operator()() { return message; }
            const T& operator()() const { return message; }
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
    using _traits = sqlpp::make_traits< sqlpp::smallint >;
};
} // namespace log

// Define the Log Table
struct LogTable : sqlpp::table_t< LogTable, log::Id, log::SessionId, log::Timestamp, log::Message, log::Type >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "logs";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T logs;
            T& operator()() { return logs; }
            const T& operator()() const { return logs; }
        };
    };
};

class Log
{
   public:
    // Constructors
    Log();
    explicit Log(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five: Default move and copy constructors/assignment
    Log(const Log&)                = default;
    Log(Log&&) noexcept            = default;
    Log& operator=(const Log&)     = default;
    Log& operator=(Log&&) noexcept = default;
    ~Log()                         = default;

    // Getters and Setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    boost::uuids::uuid getSessionId() const { return session_id_; }
    void setSessionId(const boost::uuids::uuid& session_id) { session_id_ = session_id; }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    const std::string& getMessage() const { return message_; }
    void setMessage(const std::string& message) { message_ = message; }

    log::LogType getType() const { return type_; }
    void setType(log::LogType type) { type_ = type; }

    // Static methods for database operations
    static inline auto table() { return LogTable{}; }
    static inline std::string modelName() { return "Log"; }

    // Convert DB row to model instance
    template < typename ROW >
    static Log fromRow(const ROW& row)
    {
        Log log;
        log.id_            = boost::uuids::string_generator()(row.id.value());
        log.session_id_    = boost::uuids::string_generator()(row.session_id.value());
        log.timestamp_     = row.timestamp;
        log.message_       = row.message;
        int16_t type_value = row.type;
        switch (type_value)
        {
            case static_cast< int16_t >(log::LogType::INFO):
                log.type_ = log::LogType::INFO;
                break;
            case static_cast< int16_t >(log::LogType::ERROR):
                log.type_ = log::LogType::ERROR;
                break;
            case static_cast< int16_t >(log::LogType::WARNING):
                log.type_ = log::LogType::WARNING;
                break;
            case static_cast< int16_t >(log::LogType::DEBUG):
                log.type_ = log::LogType::DEBUG;
                break;
            default:
                // Fallback to default type if unknown value
                log.type_ = log::LogType::INFO;
                break;
        }

        return log;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id         = boost::uuids::to_string(id_),
                                                               t.session_id = boost::uuids::to_string(session_id_),
                                                               t.timestamp  = timestamp_,
                                                               t.message    = message_,
                                                               t.type       = static_cast< int16_t >(type_));
    }

    // Prepare update statement
    auto prepareUpdateStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.session_id = boost::uuids::to_string(session_id_),
                         t.timestamp  = timestamp_,
                         t.message    = message_,
                         t.type       = static_cast< int16_t >(type_))
            .dynamic_where(t.id == parameter(t.id));
    }

    // Query builder for filtering
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withSessionId(const boost::uuids::uuid& session_id)
        {
            session_id_ = session_id;
            return *this;
        }

        Filter& withType(log::LogType type)
        {
            type_ = type;
            return *this;
        }

        Filter& withTimestampRange(int64_t start, int64_t end)
        {
            start_timestamp_ = start;
            end_timestamp_   = end;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (session_id_)
            {
                query.where.add(t.session_id == boost::uuids::to_string(*session_id_));
            }
            if (type_)
            {
                query.where.add(t.type == static_cast< int16_t >(*type_));
            }
            if (start_timestamp_ && end_timestamp_)
            {
                query.where.add(t.timestamp >= *start_timestamp_ && t.timestamp <= *end_timestamp_);
            }
        }

       private:
        friend class Log;
        std::optional< boost::uuids::uuid > id_;
        std::optional< boost::uuids::uuid > session_id_;
        std::optional< log::LogType > type_;
        std::optional< int64_t > start_timestamp_;
        std::optional< int64_t > end_timestamp_;
    };

    // Static factory method for creating a filter
    static Filter createFilter() { return Filter{}; }

    // Simplified public interface methods
    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< Log > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                         const boost::uuids::uuid& id)
    {
        return db::findById< Log >(conn_ptr, id);
    }

    static std::optional< std::vector< Log > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                            const Filter& filter)
    {
        return db::findByFilter< Log, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_{boost::uuids::random_generator()()};
    boost::uuids::uuid session_id_;
    int64_t timestamp_{0};
    std::string message_;
    log::LogType type_{log::LogType::INFO};
};

namespace notification_api_keys
{

// Column definitions for notification_api_keys table
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

struct Name
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "name";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T name;
            T& operator()() { return name; }
            const T& operator()() const { return name; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Driver
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "driver";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T driver;
            T& operator()() { return driver; }
            const T& operator()() const { return driver; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Fields
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "fields";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T fields;
            T& operator()() { return fields; }
            const T& operator()() const { return fields; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct CreatedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "created_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T created_at;
            T& operator()() { return created_at; }
            const T& operator()() const { return created_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
};
} // namespace notification_api_keys

struct NotificationApiKeysTable
    : sqlpp::table_t< NotificationApiKeysTable,
                      notification_api_keys::Id,
                      notification_api_keys::Name,
                      notification_api_keys::Driver,
                      notification_api_keys::Fields,
                      notification_api_keys::CreatedAt >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "notification_api_keys";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T notification_api_keys;
            T& operator()() { return notification_api_keys; }
            const T& operator()() const { return notification_api_keys; }
        };
    };
};

class NotificationApiKeys
{
   public:
    // Constructors
    NotificationApiKeys();
    explicit NotificationApiKeys(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five
    NotificationApiKeys(const NotificationApiKeys&)                = default;
    NotificationApiKeys(NotificationApiKeys&&) noexcept            = default;
    NotificationApiKeys& operator=(const NotificationApiKeys&)     = default;
    NotificationApiKeys& operator=(NotificationApiKeys&&) noexcept = default;
    ~NotificationApiKeys()                                         = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::string& getDriver() const { return driver_; }
    void setDriver(const std::string& driver) { driver_ = driver; }

    // JSON field handling
    nlohmann::json getFields() const
    {
        try
        {
            return nlohmann::json::parse(fields_json_);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return nlohmann::json::object();
        }
    }

    void setFields(const nlohmann::json& fields) { fields_json_ = fields.dump(); }
    void setFieldsJson(const std::string& fields_json)
    {
        try
        {
            // Validate that the string is valid JSON
            auto json    = nlohmann::json::parse(fields_json);
            fields_json_ = json.dump();
        }
        catch (const nlohmann::json::parse_error& e)
        {
            throw std::invalid_argument(std::string("Invalid JSON: ") + e.what());
        }
    }

    int64_t getCreatedAt() const { return created_at_; }
    void setCreatedAt(int64_t created_at) { created_at_ = created_at; }

    // Database operations
    static inline auto table() { return NotificationApiKeysTable{}; }
    static inline std::string modelName() { return "NotificationApiKeys"; }

    template < typename ROW >
    static NotificationApiKeys fromRow(const ROW& row)
    {
        NotificationApiKeys apiKey;
        apiKey.id_          = boost::uuids::string_generator()(row.id.value());
        apiKey.name_        = row.name;
        apiKey.driver_      = row.driver;
        apiKey.fields_json_ = row.fields;
        apiKey.created_at_  = row.created_at;
        return apiKey;
    }

    auto prepareInsertStatement(const NotificationApiKeysTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id         = getIdAsString(),
                                                               t.name       = name_,
                                                               t.driver     = driver_,
                                                               t.fields     = fields_json_,
                                                               t.created_at = created_at_);
    }

    auto prepareUpdateStatement(const NotificationApiKeysTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.name = name_, t.driver = driver_, t.fields = fields_json_, t.created_at = created_at_)
            .dynamic_where(t.id == parameter(t.id));
    }

    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< NotificationApiKeys > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                         const boost::uuids::uuid& id)
    {
        return db::findById< NotificationApiKeys >(conn_ptr, id);
    }

    // Filtering for queries
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withName(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        Filter& withDriver(std::string driver)
        {
            driver_ = std::move(driver);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (name_)
            {
                query.where.add(t.name == *name_);
            }
            if (driver_)
            {
                query.where.add(t.driver == *driver_);
            }
        }

       private:
        friend class NotificationApiKeys;
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > name_;
        std::optional< std::string > driver_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< NotificationApiKeys > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< NotificationApiKeys, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    std::string name_;
    std::string driver_;
    std::string fields_json_ = "{}";
    int64_t created_at_;
};

namespace option
{

// Column definitions for options table
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

struct UpdatedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "updated_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T updated_at;
            T& operator()() { return updated_at; }
            const T& operator()() const { return updated_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
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

struct Json
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "json";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T json;
            T& operator()() { return json; }
            const T& operator()() const { return json; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

} // namespace option

// Define the table structure
struct OptionsTable : sqlpp::table_t< OptionsTable, option::Id, option::UpdatedAt, option::Type, option::Json >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "options";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T options;
            T& operator()() { return options; }
            const T& operator()() const { return options; }
        };
    };
};

class Option
{
   public:
    // Constructors
    Option();
    explicit Option(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five
    Option(const Option&)                = default;
    Option(Option&&) noexcept            = default;
    Option& operator=(const Option&)     = default;
    Option& operator=(Option&&) noexcept = default;
    ~Option()                            = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); };
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getUpdatedAt() const { return updated_at_; }
    void setUpdatedAt(int64_t updated_at) { updated_at_ = updated_at; }

    const std::string& getType() const { return type_; }
    void setType(const std::string& type) { type_ = type; }

    // JSON field handling
    nlohmann::json getJson() const
    {
        try
        {
            return nlohmann::json::parse(json_str_);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return nlohmann::json::object();
        }
    }

    void setJson(const nlohmann::json& json) { json_str_ = json.dump(); }
    void setJsonStr(const std::string& json_str)
    {
        try
        {
            // Validate that the string is valid JSON
            auto json = nlohmann::json::parse(json_str);
            json_str_ = json.dump();
        }
        catch (const nlohmann::json::parse_error& e)
        {
            throw std::invalid_argument(std::string("Invalid JSON: ") + e.what());
        }
    }

    // Update the updated_at timestamp to current time
    void updateTimestamp()
    {
        updated_at_ =
            std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
                .count();
    }

    // Database operations
    static inline auto table() { return OptionsTable{}; }
    static inline std::string modelName() { return "Option"; }

    template < typename ROW >
    static Option fromRow(const ROW& row)
    {
        Option option;
        option.id_         = boost::uuids::string_generator()(row.id.value());
        option.updated_at_ = row.updated_at;
        option.type_       = row.type;
        option.json_str_   = row.json;
        return option;
    }

    auto prepareInsertStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(
            t.id = getIdAsString(), t.updated_at = updated_at_, t.type = type_, t.json = json_str_);
    }

    auto prepareUpdateStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.updated_at = updated_at_, t.type = type_, t.json = json_str_)
            .dynamic_where(t.id == parameter(t.id));
    }

    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< Option > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                            const boost::uuids::uuid& id)
    {
        return db::findById< Option >(conn_ptr, id);
    }

    // Filtering for queries
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
            return *this;
        }

        Filter& withType(std::string type)
        {
            type_ = std::move(type);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (type_)
            {
                query.where.add(t.type == *type_);
            }
        }

       private:
        friend class Option;
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > type_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Option > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< Option, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    int64_t updated_at_;
    std::string type_;
    std::string json_str_ = "{}";
};

namespace orderbook
{
// Column definitions for orderbook table
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
    using _traits = sqlpp::make_traits< sqlpp::bigint >;
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

struct Data
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "data";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T data;
            T& operator()() { return data; }
            const T& operator()() const { return data; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::blob >;
};
} // namespace orderbook

// Define the table structure
struct OrderbooksTable
    : sqlpp::table_t< OrderbooksTable,
                      orderbook::Id,
                      orderbook::Timestamp,
                      orderbook::Symbol,
                      orderbook::Exchange,
                      orderbook::Data >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "orderbooks";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T orderbooks;
            T& operator()() { return orderbooks; }
            const T& operator()() const { return orderbooks; }
        };
    };
};

class Orderbook
{
   public:
    // Default constructor with random UUID generation
    Orderbook();

    // Constructor with attribute map
    explicit Orderbook(const std::unordered_map< std::string, std::any >& attributes);

    // Rule of five
    Orderbook(const Orderbook&)                = default;
    Orderbook(Orderbook&&) noexcept            = default;
    Orderbook& operator=(const Orderbook&)     = default;
    Orderbook& operator=(Orderbook&&) noexcept = default;
    ~Orderbook()                               = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); };
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const std::string& getExchange() const { return exchange_; }
    void setExchange(const std::string& exchange) { exchange_ = exchange; }

    const std::vector< uint8_t >& getData() const { return data_; }
    void setData(const std::vector< uint8_t >& data) { data_ = data; }

    // Compress and decompress data
    void setDataFromString(const std::string& data_str)
    {
        data_.clear();
        data_.reserve(data_str.size());
        std::copy(data_str.begin(), data_str.end(), std::back_inserter(data_));
    }
    std::string getDataAsString() const { return std::string(data_.begin(), data_.end()); }

    // Database operations
    static inline auto table() { return OrderbooksTable{}; }
    static inline std::string modelName() { return "Orderbook"; }

    template < typename ROW >
    static Orderbook fromRow(const ROW& row)
    {
        Orderbook orderbook;
        orderbook.id_        = boost::uuids::string_generator()(row.id.value());
        orderbook.timestamp_ = row.timestamp;
        orderbook.symbol_    = row.symbol;
        orderbook.exchange_  = row.exchange;

        // Convert BLOB to vector<uint8_t>
        const auto& blob = row.data; // Access the blob column
        if (!blob.is_null())         // Check if the blob is not null
        {
            // Use value() to access the data directly
            const auto& blob_data = blob.value();
            orderbook.data_.assign(blob_data.begin(), blob_data.end()); // Assign to std::vector<uint8_t>
        }
        return orderbook;
    }

    auto prepareInsertStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
    {
        // Prepare the dynamic insert statement with placeholders
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id        = getIdAsString(),
                                                               t.timestamp = timestamp_,
                                                               t.symbol    = symbol_,
                                                               t.exchange  = exchange_,
                                                               t.data      = data_);
    }

    auto prepareUpdateStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp = timestamp_, t.symbol = symbol_, t.exchange = exchange_, t.data = data_)
            .dynamic_where(t.id == parameter(t.id));
    }

    bool save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr) { return db::save(*this, conn_ptr); }

    static std::optional< Orderbook > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                               const boost::uuids::uuid& id)
    {
        return db::findById< Orderbook >(conn_ptr, id);
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

        Filter& withTimestamp(int64_t timestamp)
        {
            timestamp_ = timestamp;
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

        Filter& withTimestampRange(int64_t start, int64_t end)
        {
            timestamp_start_ = start;
            if (end)
            {
                timestamp_end_ = end;
            }
            else
            {
                timestamp_end_ = std::nullopt;
            }
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
            if (symbol_)
            {
                query.where.add(t.symbol == *symbol_);
            }
            if (exchange_)
            {
                query.where.add(t.exchange == *exchange_);
            }
            if (timestamp_start_ && timestamp_end_)
            {
                query.where.add(t.timestamp >= *timestamp_start_);
                query.where.add(t.timestamp <= *timestamp_end_);
            }
            else if (timestamp_start_)
            {
                query.where.add(t.timestamp >= *timestamp_start_);
            }
            else if (timestamp_end_)
            {
                query.where.add(t.timestamp <= *timestamp_end_);
            }
        }

       private:
        friend class Orderbook;
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< std::string > exchange_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Orderbook > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< Orderbook, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    int64_t timestamp_ = 0;
    std::string symbol_;
    std::string exchange_;
    std::vector< uint8_t > data_;
};

} // namespace CipherDB

#endif
