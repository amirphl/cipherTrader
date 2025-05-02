#ifndef CIPHER_DB_HPP
#define CIPHER_DB_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <blaze/Math.h>

#include "Config.hpp"
#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Logger.hpp"

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
#include <sqlpp11/null.h>
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/connection_config.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/table.h>
#include <sqlpp11/transaction.h>
#include <sqlpp11/update.h>

namespace ct
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
        config->debug    = false; // TODO: accept as arg
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
            logger::LOG.error("Cannot commit null transaction");
            return false;
        }

        try
        {
            tx->commit();
            return true;
        }
        catch (const std::exception& e)
        {
            std::ostringstream oss;
            oss << "Error committing transaction: " << e.what();
            logger::LOG.error(oss.str());
            return false;
        }
    }

    // Roll back the transaction
    static bool rollbackTransaction(std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx)
    {
        if (!tx)
        {
            std::ostringstream oss;
            oss << "Cannot rollback null transaction";
            logger::LOG.error(oss.str());
            return false;
        }

        try
        {
            tx->rollback();
            return true;
        }
        catch (const std::exception& e)
        {
            std::ostringstream oss;
            oss << "Error rolling back transaction: " << e.what();
            logger::LOG.error(oss.str());
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
                std::ostringstream oss;
                oss << "Error during auto-rollback: " << e.what();
                logger::LOG.error(oss.str());
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
            std::ostringstream oss;
            oss << "Error during commit: " << e.what();
            logger::LOG.error(oss.str());
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
            std::ostringstream oss;
            oss << "Error during rollback: " << e.what();
            logger::LOG.error(oss.str());
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

            std::ostringstream oss;
            oss << "Operation failed, retrying (" << retries << "/" << maxRetries << "): " << e.what();
            logger::LOG.error(oss.str());

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
        std::ostringstream oss;
        oss << "Error finding " << ModelType::modelName() << " by ID: " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
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
                std::ostringstream oss;
                oss << "Error processing row: " << e.what();
                logger::LOG.error(oss.str());

                // Mark the connection for reset
                stateGuard.markForReset();

                throw;
            }
        }

        return results;
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error in findByFilter for " << ModelType::modelName() << ": " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
    }
}

// Generic save implementation
template < typename ModelType >
void save(ModelType& model, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const bool update_on_conflict)
{
    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = ModelType::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    try
    {
        auto select_stmt = model.prepareSelectStatementForConflictCheck(t, conn);
        auto sprep       = conn.prepare(select_stmt);
        auto rows        = conn(sprep);

        std::vector< ModelType > retrieved;

        if (!rows.empty())
        {
            for (const auto& row : rows)
            {
                try
                {
                    retrieved.push_back(std::move(ModelType::fromRow(row)));
                }
                catch (const std::exception& e)
                {
                    std::ostringstream oss;
                    oss << "Error processing row: " << e.what();
                    logger::LOG.error(oss.str());

                    // Mark the connection for reset
                    stateGuard.markForReset();

                    throw;
                }
            }
        }

        // TODO: Support batch update?
        if (retrieved.size() > 1)
        {
            std::ostringstream oss;
            oss << "Conflict with more that one row: " << model;
            logger::LOG.error(oss.str());

            // Mark the connection for reset
            stateGuard.markForReset();

            throw;
        }

        if (retrieved.size() == 1 && update_on_conflict)
        {
            // Update
            auto update_stmt = model.prepareUpdateStatement(t, conn);
            auto uprep       = conn.prepare(update_stmt);
            uprep.params.id  = retrieved[0].getIdAsString();
            conn(uprep);
        }
        else
        {
            // Insert
            auto insert_stmt = model.prepareInsertStatement(t, conn);
            conn(insert_stmt);
        }
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error saving " << ModelType::modelName() << ": " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
    }
}

// TODO: Implement batch save for models.

template < typename ModelType >
void batchSave(const std::vector< ModelType >& models, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
{
    if (models.empty())
    {
        return;
    }

    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = ModelType::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    try
    {
        auto batch_insert_stmt = ModelType::prepareBatchInsertStatement(models, t, conn);
        conn(batch_insert_stmt);
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error in batch saving " << ModelType::modelName() << ": " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
    }
}

namespace order
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

struct TradeId
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "trade_id";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T trade_id;
            T& operator()() { return trade_id; }
            const T& operator()() const { return trade_id; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar, sqlpp::tag::can_be_null >;
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

struct ExchangeId
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "exchange_id";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T exchange_id;
            T& operator()() { return exchange_id; }
            const T& operator()() const { return exchange_id; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar, sqlpp::tag::can_be_null >;
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

struct OrderSide
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "order_side";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T order_side;
            T& operator()() { return order_side; }
            const T& operator()() const { return order_side; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct OrderType
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "order_type";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T order_type;
            T& operator()() { return order_type; }
            const T& operator()() const { return order_type; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct ReduceOnly
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "reduce_only";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T reduce_only;
            T& operator()() { return reduce_only; }
            const T& operator()() const { return reduce_only; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::boolean >;
};

struct Qty
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "qty";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T qty;
            T& operator()() { return qty; }
            const T& operator()() const { return qty; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct FilledQty
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "filled_qty";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T filled_qty;
            T& operator()() { return filled_qty; }
            const T& operator()() const { return filled_qty; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct Price
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "price";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T price;
            T& operator()() { return price; }
            const T& operator()() const { return price; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point, sqlpp::tag::can_be_null >;
};

struct Status
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "status";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T status;
            T& operator()() { return status; }
            const T& operator()() const { return status; }
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

struct ExecutedAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "executed_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T executed_at;
            T& operator()() { return executed_at; }
            const T& operator()() const { return executed_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint, sqlpp::tag::can_be_null >;
};

struct CanceledAt
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "canceled_at";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T canceled_at;
            T& operator()() { return canceled_at; }
            const T& operator()() const { return canceled_at; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::bigint, sqlpp::tag::can_be_null >;
};

struct Vars
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "vars";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T vars;
            T& operator()() { return vars; }
            const T& operator()() const { return vars; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};
} // namespace order

struct OrdersTable
    : sqlpp::table_t< OrdersTable,
                      order::Id,
                      order::TradeId,
                      order::SessionId,
                      order::ExchangeId,
                      order::Symbol,
                      order::ExchangeName,
                      order::OrderSide,
                      order::OrderType,
                      order::ReduceOnly,
                      order::Qty,
                      order::FilledQty,
                      order::Price,
                      order::Status,
                      order::CreatedAt,
                      order::ExecutedAt,
                      order::CanceledAt,
                      order::Vars >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "orders";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T orders;
            T& operator()() { return orders; }
            const T& operator()() const { return orders; }
        };
    };
};

// TODO: Read logic, fix TODOs.
class Order
{
   public:
    // Default constructor with random UUID generation
    Order(bool should_silent = false);

    // Constructor with attribute map
    explicit Order(const std::unordered_map< std::string, std::any >& attributes, bool should_silent = false);

    Order(std::optional< boost::uuids::uuid > trade_id,
          boost::uuids::uuid session_id,
          std::optional< std::string > exchange_id,
          std::string symbol,
          enums::ExchangeName exchange_name,
          enums::OrderSide order_side,
          enums::OrderType order_type,
          bool reduce_only,
          double qty,
          double filled_qty,
          std::optional< double > price,
          enums::OrderStatus status,
          int64_t created_at,
          std::optional< int64_t > executed_at,
          std::optional< int64_t > canceled_at,
          nlohmann::json vars,
          enums::OrderSubmittedVia submitted_via)
        : id_(boost::uuids::random_generator()())
        , trade_id_(trade_id)
        , session_id_(session_id)
        , exchange_id_(exchange_id)
        , symbol_(symbol)
        , exchange_name_(exchange_name)
        , order_side_(order_side)
        , order_type_(order_type)
        , reduce_only_(reduce_only)
        , qty_(qty)
        , filled_qty_(filled_qty)
        , price_(price)
        , status_(status)
        , created_at_(created_at)
        , executed_at_(executed_at)
        , canceled_at_(canceled_at)
        , vars_(vars)
        , submitted_via_(submitted_via)
    {
    }

    // Rule of five
    Order(const Order&)                = default;
    Order(Order&&) noexcept            = default;
    Order& operator=(const Order&)     = default;
    Order& operator=(Order&&) noexcept = default;
    ~Order()                           = default;

    // Getters and setters
    const boost::uuids::uuid& getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }
    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    const std::optional< boost::uuids::uuid >& getTradeId() const { return trade_id_; }
    void setTradeId(const boost::uuids::uuid& id) { trade_id_ = id; }
    void clearTradeId() { trade_id_.reset(); }
    std::string getTradeIdAsString() const
    {
        if (trade_id_)
        {
            return boost::uuids::to_string(*trade_id_);
        }
        return "";
    }
    void setTradeId(const std::string& id_str) { trade_id_ = boost::uuids::string_generator()(id_str); }

    const boost::uuids::uuid& getSessionId() const { return session_id_; }
    void setSessionId(const boost::uuids::uuid& id) { session_id_ = id; }
    std::string getSessionIdAsString() const { return boost::uuids::to_string(session_id_); }
    void setSessionId(const std::string& id_str) { session_id_ = boost::uuids::string_generator()(id_str); }

    const std::optional< std::string >& getExchangeId() const { return exchange_id_; }
    void setExchangeId(const std::string& id) { exchange_id_ = id; }
    void clearExchangeId() { exchange_id_.reset(); }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    const enums::OrderSide& getOrderSide() const { return order_side_; }
    void setOrderSide(const enums::OrderSide& order_side) { order_side_ = order_side; }

    const enums::OrderType& getOrderType() const { return order_type_; }
    void setOrderType(const enums::OrderType& order_type) { order_type_ = order_type; }

    bool isReduceOnly() const { return reduce_only_; }
    void setReduceOnly(bool reduce_only) { reduce_only_ = reduce_only; }

    double getQty() const { return qty_; }
    void setQty(double qty) { qty_ = qty; }

    double getFilledQty() const { return filled_qty_; }
    void setFilledQty(double filled_qty) { filled_qty_ = filled_qty; }

    const std::optional< double >& getPrice() const { return price_; }
    void setPrice(double price) { price_ = price; }
    void clearPrice() { price_.reset(); }

    enums::OrderStatus getStatus() const { return status_; }
    void setStatus(enums::OrderStatus status) { status_ = status; }

    int64_t getCreatedAt() const { return created_at_; }
    void setCreatedAt(int64_t created_at) { created_at_ = created_at; }

    const std::optional< int64_t >& getExecutedAt() const { return executed_at_; }
    void setExecutedAt(int64_t executed_at) { executed_at_ = executed_at; }
    void clearExecutedAt() { executed_at_.reset(); }

    const std::optional< int64_t >& getCanceledAt() const { return canceled_at_; }
    void setCanceledAt(int64_t canceled_at) { canceled_at_ = canceled_at; }
    void clearCanceledAt() { canceled_at_.reset(); }

    const nlohmann::json& getVars() const { return vars_; }
    void setVars(const nlohmann::json& vars) { vars_ = vars; }

    enums::OrderSubmittedVia getSubmittedVia() const { return submitted_via_; }
    void setSubmittedVia(enums::OrderSubmittedVia via) { submitted_via_ = via; }

    // Status checking methods
    bool isActive() const { return status_ == enums::OrderStatus::ACTIVE; }
    bool isQueued() const { return status_ == enums::OrderStatus::QUEUED; }
    bool isCanceled() const { return status_ == enums::OrderStatus::CANCELED; }
    bool isExecuted() const { return status_ == enums::OrderStatus::EXECUTED; }
    bool isPartiallyFilled() const { return status_ == enums::OrderStatus::PARTIALLY_FILLED; }
    bool isCancellable() const { return isActive() || isPartiallyFilled() || isQueued(); }
    bool isNew() const { return isActive(); }
    bool isFilled() const { return isExecuted(); }
    bool isStopLoss() const { return submitted_via_ == enums::OrderSubmittedVia::STOP_LOSS; }
    bool isTakeProfit() const { return submitted_via_ == enums::OrderSubmittedVia::TAKE_PROFIT; }

    // TODO:
    // def position(self):
    //     return selectors.get_position(self.exchange, self.symbol)

    // Calculated properties
    double getValue() const
    {
        if (!price_)
        {
            return 0.0;
        }
        return std::abs(qty_) * (*price_);
    }

    double getRemainingQty() const
    {
        return helper::prepareQty(std::abs(qty_) - std::abs(filled_qty_), enums::toString(order_side_));
    }

    // Order state transitions
    void queueIt()
    {
        status_ = enums::OrderStatus::QUEUED;
        canceled_at_.reset();

        if (helper::isDebuggable("order_submission"))
        {
            std::string txt = "QUEUED order: " + symbol_ + ", " + enums::toString(order_type_) + ", " +
                              enums::toString(order_side_) + ", " + std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(std::round(*price_ * 100) / 100);
            }

            logger::LOG.info(txt);
        }

        notifySubmission();
    }

    void resubmit()
    {
        // Don't allow resubmission if the order is not queued
        if (!isQueued())
        {
            throw std::runtime_error("Cannot resubmit an order that is not queued. Current status: " +
                                     enums::toString(status_));
        }

        // Regenerate the order id to avoid errors on the exchange's side
        id_     = boost::uuids::string_generator()(helper::generateUniqueId());
        status_ = enums::OrderStatus::ACTIVE;
        canceled_at_.reset();

        if (helper::isDebuggable("order_submission"))
        {
            std::string txt = "SUBMITTED order: " + symbol_ + ", " + enums::toString(order_type_) + ", " +
                              enums::toString(order_side_) + ", " + std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(*price_);
            }

            logger::LOG.info(txt);
        }

        notifySubmission();
    }

    void cancel(bool silent = false, const std::string& source = "")
    {
        if (isCanceled() || isExecuted())
        {
            return;
        }

        // Fix for when the cancelled stream's lag causes cancellation of queued orders
        if (source == "stream" && isQueued())
        {
            return;
        }

        canceled_at_ = helper::nowToTimestamp();
        status_      = enums::OrderStatus::CANCELED;

        // TODO:
        // if (helper::isLive())
        // {
        //     save();
        // }

        if (!silent)
        {
            std::string txt = "CANCELED order: " + symbol_ + ", " + enums::toString(order_type_) + ", " +
                              enums::toString(order_side_) + ", " + std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(std::round(*price_ * 100) / 100);
            }

            if (helper::isDebuggable("order_cancellation"))
            {
                logger::LOG.info(txt);
            }

            if (helper::isLive())
            {
                if (config::Config::getInstance().getValue< bool >("env_notifications_events_cancelled_orders"))
                {
                    // TODO:
                    // notify(txt);
                }
            }
        }

        // TODO: Handle exchange balance
        // auto e = selectors.get_exchange(exchange_);
        // e.on_order_cancellation(*this);
    }

    void execute(bool silent = false)
    {
        if (isCanceled() || isExecuted())
        {
            return;
        }

        executed_at_ = helper::nowToTimestamp();
        status_      = enums::OrderStatus::EXECUTED;

        // TODO:
        // if (helper::isLive())
        // {
        //     save();
        // }

        if (!silent)
        {
            std::string txt = "EXECUTED order: " + symbol_ + ", " + enums::toString(order_type_) + ", " +
                              enums::toString(order_side_) + ", " + std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(std::round(*price_ * 100) / 100);
            }

            if (helper::isDebuggable("order_execution"))
            {
                logger::LOG.info(txt);
            }

            if (helper::isLive())
            {
                if (config::Config::getInstance().getValue< bool >("env_notifications_events_executed_orders"))
                {
                    // TODO:
                    // notify(txt);
                }
            }
        }

        // TODO: Log the order of the trade for metrics
        // store.completed_trades.add_executed_order(*this);

        // TODO: Handle exchange balance
        // auto e = selectors.get_exchange(exchange_);
        // e.on_order_execution(*this);

        // TODO: Update position
        // auto p = selectors.get_position(exchange_, symbol_);
        // if (p) {
        //     p._on_executed_order(*this);
        // }
    }

    void executePartially(bool silent = false)
    {
        executed_at_ = helper::nowToTimestamp();
        status_      = enums::OrderStatus::PARTIALLY_FILLED;

        // TODO:
        // if (helper::isLive())
        // {
        //     save();
        // }

        if (!silent)
        {
            std::string txt = "PARTIALLY FILLED: " + symbol_ + ", " + enums::toString(order_type_) + ", " +
                              enums::toString(order_side_) + ", filled qty: " + std::to_string(filled_qty_) +
                              ", remaining qty: " + std::to_string(getRemainingQty());

            if (price_)
            {
                txt += ", price: " + std::to_string(*price_);
            }

            if (helper::isDebuggable("order_execution"))
            {
                logger::LOG.info(txt);
            }

            if (helper::isLive())
            {
                if (config::Config::getInstance().getValue< bool >("env_notifications_events_executed_orders"))
                {
                    // TODO:
                    // notify(txt);
                }
            }
        }

        // TODO: Log the order of the trade for metrics
        // store.completed_trades.add_executed_order(*this);

        // TODO: Update position
        // auto p = selectors.get_position(exchange_, symbol_);
        // if (p) {
        //     p._on_executed_order(*this);
        // }
    }

    // Notification methods
    void notifySubmission() const
    {
        if (config::Config::getInstance().getValue< bool >("env_notifications_events_submitted_orders") &&
            (isActive() || isQueued()))
        {
            std::string txt = (isQueued() ? "QUEUED" : "SUBMITTED") + std::string(" order: ") + symbol_ + ", " +
                              enums::toString(order_type_) + ", " + enums::toString(order_side_) + ", " +
                              std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(*price_);
            }

            // TODO: Use notify function when available
            std::cout << "NOTIFICATION: " << txt << std::endl;
        }
    }

    /**
     * Creates a fake order with optional custom attributes for testing purposes.
     *
     * @param attributes Optional map of attributes to override defaults
     * @return Order A fake order instance
     */
    static Order generateFakeOrder(const std::unordered_map< std::string, std::any >& attributes = {})
    {
        static int64_t first_timestamp = 1552309186171;
        first_timestamp += 60000;

        // Default values
        auto exchange_name        = enums::ExchangeName::SANDBOX;
        std::string symbol        = "BTC-USD";
        auto order_side           = enums::OrderSide::BUY;
        auto order_type           = enums::OrderType::LIMIT;
        double price              = candle::randint(40, 100);
        double qty                = candle::randint(1, 10);
        enums::OrderStatus status = enums::OrderStatus::ACTIVE;
        int64_t created_at        = first_timestamp;

        // Prepare attributes map with defaults
        std::unordered_map< std::string, std::any > order_attrs;

        // Set the ID
        order_attrs["id"] = helper::generateUniqueId();

        // Set attributes with values from the provided map or use defaults
        auto tryGet = [&attributes](const std::string& key, const auto& default_value) -> auto
        {
            auto it = attributes.find(key);
            if (it != attributes.end())
            {
                try
                {
                    return std::any_cast< std::decay_t< decltype(default_value) > >(it->second);
                }
                catch (const std::bad_any_cast&)
                {
                    return default_value;
                }
            }
            return default_value;
        };

        order_attrs["symbol"]        = tryGet("symbol", symbol);
        order_attrs["exchange_name"] = tryGet("exchange_name", exchange_name);
        order_attrs["order_side"]    = tryGet("order_side", order_side);
        order_attrs["order_type"]    = tryGet("order_type", order_type);
        order_attrs["qty"]           = tryGet("qty", qty);
        order_attrs["price"]         = tryGet("price", price);
        order_attrs["status"]        = tryGet("status", status);
        order_attrs["created_at"]    = tryGet("created_at", created_at);

        // Create the order
        return Order(order_attrs);
    }

    // Database operations
    static inline auto table() { return OrdersTable{}; }
    static inline std::string modelName() { return "Order"; }

    template < typename ROW >
    static Order fromRow(const ROW& row)
    {
        Order order;
        order.id_ = boost::uuids::string_generator()(row.id.value());

        if (!row.trade_id.is_null())
            order.trade_id_ = boost::uuids::string_generator()(row.trade_id.value());

        order.session_id_ = boost::uuids::string_generator()(row.session_id.value());

        if (!row.exchange_id.is_null())
            order.exchange_id_ = row.exchange_id.value();

        order.symbol_        = row.symbol;
        order.exchange_name_ = enums::toExchangeName(row.exchange_name);
        order.order_side_    = enums::toOrderSide(row.order_side);
        order.order_type_    = enums::toOrderType(row.order_type);
        order.reduce_only_   = row.reduce_only;
        order.qty_           = row.qty;
        order.filled_qty_    = row.filled_qty;

        if (!row.price.is_null())
            order.price_ = row.price.value();

        order.status_     = enums::toOrderStatus(row.status);
        order.created_at_ = row.created_at;

        if (!row.executed_at.is_null())
            order.executed_at_ = row.executed_at.value();

        if (!row.canceled_at.is_null())
            order.canceled_at_ = row.canceled_at.value();

        // Parse JSON from string
        order.vars_ = nlohmann::json::parse(row.vars.value());

        return order;
    }

    auto prepareSelectStatementForConflictCheck(const OrdersTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter()
                          .withTradeId(trade_id_.value_or(boost::uuids::nil_uuid()))
                          .withExchangeName(exchange_name_)
                          .withSymbol(symbol_)
                          .withStatus(status_)
                          .withCreatedAt(created_at_);

        filter.applyToQuery(query, t);

        return query;
    }

    auto prepareInsertStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt = sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                                    t.session_id    = getSessionIdAsString(),
                                                                    t.symbol        = symbol_,
                                                                    t.exchange_name = enums::toString(exchange_name_),
                                                                    t.order_side    = enums::toString(order_side_),
                                                                    t.order_type    = enums::toString(order_type_),
                                                                    t.reduce_only   = reduce_only_,
                                                                    t.qty           = qty_,
                                                                    t.filled_qty    = filled_qty_,
                                                                    t.status        = enums::toString(status_),
                                                                    t.created_at    = created_at_,
                                                                    t.vars          = vars_.dump());

        // Add optional fields
        if (trade_id_ && *trade_id_ != boost::uuids::nil_uuid())
        {
            stmt.insert_list.add(t.trade_id = getTradeIdAsString());
        }

        if (exchange_id_ && *exchange_id_ != "")
        {
            stmt.insert_list.add(t.exchange_id = *exchange_id_);
        }

        if (price_ && *price_ != std::numeric_limits< double >::quiet_NaN())
        {
            stmt.insert_list.add(t.price = *price_);
        }

        if (executed_at_ && *executed_at_ != std::numeric_limits< int64_t >::min())
        {
            stmt.insert_list.add(t.executed_at = *executed_at_);
        }

        if (canceled_at_ && canceled_at_ != std::numeric_limits< int64_t >::min())
        {
            stmt.insert_list.add(t.canceled_at = *canceled_at_);
        }

        return stmt;
    }

    // TODO: performance.
    auto prepareBatchInsertStatement(const std::vector< Order >& models,
                                     const OrdersTable& t,
                                     sqlpp::postgresql::connection& conn)
    {
        if (models.empty())
        {
            throw std::invalid_argument("Cannot prepare batch insert for empty models vector");
        }

        auto stmt = models[0].prepareInsertStatement(t, conn);

        std::vector< decltype(stmt) > stmts;
        stmts.reserve(models.size());
        stmts.emplace_back(stmt);

        for (size_t i = 1; i < models.size(); ++i)
        {
            stmts.emplace_back(models[i].prepareInsertStatement(t, conn));
        }

        return stmts;
    }

    auto prepareUpdateStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt = sqlpp::dynamic_update(conn, t)
                        .dynamic_set(t.session_id    = getSessionIdAsString(),
                                     t.symbol        = symbol_,
                                     t.exchange_name = enums::toString(exchange_name_),
                                     t.order_side    = enums::toString(order_side_),
                                     t.order_type    = enums::toString(order_type_),
                                     t.reduce_only   = reduce_only_,
                                     t.qty           = qty_,
                                     t.filled_qty    = filled_qty_,
                                     t.status        = enums::toString(status_),
                                     t.created_at    = created_at_,
                                     t.vars          = vars_.dump())
                        .dynamic_where(t.id == parameter(t.id));

        // Add optional fields
        if (trade_id_)
        {
            if (*trade_id_ != boost::uuids::nil_uuid())
            {
                stmt.assignments.add(t.trade_id = getTradeIdAsString());
            }
            else
            {
                stmt.assignments.add(t.trade_id = sqlpp::null);
            }
        }

        if (exchange_id_)
        {
            if (*exchange_id_ != "")
            {
                stmt.assignments.add(t.exchange_id = *exchange_id_);
            }
            else
            {
                stmt.assignments.add(t.exchange_id = sqlpp::null);
            }
        }

        if (price_)
        {
            if (*price_ != std::numeric_limits< double >::quiet_NaN())
            {
                stmt.assignments.add(t.price = *price_);
            }
            else
            {
                stmt.assignments.add(t.price = sqlpp::null);
            }
        }

        if (executed_at_)
        {
            if (*executed_at_ != std::numeric_limits< int64_t >::min())
            {
                stmt.assignments.add(t.executed_at = *executed_at_);
            }
            else
            {
                stmt.assignments.add(t.executed_at = sqlpp::null);
            }
        }

        if (canceled_at_)
        {
            if (*canceled_at_ != std::numeric_limits< int64_t >::min())
            {
                stmt.assignments.add(t.canceled_at = *canceled_at_);
            }
            else
            {
                stmt.assignments.add(t.canceled_at = sqlpp::null);
            }
        }

        return stmt;
    }

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Order > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                           const boost::uuids::uuid& id)
    {
        return db::findById< Order >(conn_ptr, id);
    }

    // Return a dictionary representation of the order
    nlohmann::json toJson() const
    {
        nlohmann::json result;

        result["id"]         = getIdAsString();
        result["trade_id"]   = getTradeIdAsString();
        result["session_id"] = getSessionIdAsString();

        if (exchange_id_)
        {
            result["exchange_id"] = *exchange_id_;
        }
        else
        {
            result["exchange_id"] = nullptr;
        }

        result["symbol"]     = symbol_;
        result["order_side"] = order_side_;
        result["order_type"] = order_type_;
        result["qty"]        = qty_;
        result["filled_qty"] = filled_qty_;

        if (price_)
        {
            result["price"] = *price_;
        }
        else
        {
            result["price"] = nullptr;
        }

        result["status"]     = status_;
        result["created_at"] = created_at_;

        if (canceled_at_)
        {
            result["canceled_at"] = *canceled_at_;
        }
        else
        {
            result["canceled_at"] = nullptr;
        }

        if (executed_at_)
        {
            result["executed_at"] = *executed_at_;
        }
        else
        {
            result["executed_at"] = nullptr;
        }

        return result;
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

        Filter& withTradeId(const boost::uuids::uuid& trade_id)
        {
            trade_id_ = trade_id;
            return *this;
        }

        Filter& withSessionId(const boost::uuids::uuid& session_id)
        {
            session_id_ = session_id;
            return *this;
        }

        Filter& withSymbol(std::string symbol)
        {
            symbol_ = std::move(symbol);
            return *this;
        }

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withOrderSide(enums::OrderSide order_side)
        {
            order_side_ = std::move(order_side);
            return *this;
        }

        Filter& withOrderType(enums::OrderType order_type)
        {
            order_type_ = std::move(order_type);
            return *this;
        }

        Filter& withStatus(enums::OrderStatus status)
        {
            status_ = std::move(status);
            return *this;
        }

        Filter& withCreatedAt(int64_t created_at)
        {
            created_at_ = std::move(created_at);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }

            if (trade_id_)
            {
                if (*trade_id_ != boost::uuids::nil_uuid())
                {
                    query.where.add(t.trade_id == boost::uuids::to_string(*trade_id_));
                }
                else
                {
                    query.where.add(t.trade_id.is_null());
                }
            }

            if (session_id_)
            {
                query.where.add(t.session_id == boost::uuids::to_string(*session_id_));
            }

            if (symbol_)
            {
                query.where.add(t.symbol == *symbol_);
            }

            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }

            if (order_side_)
            {
                query.where.add(t.order_side == enums::toString(*order_side_));
            }

            if (order_type_)
            {
                query.where.add(t.order_type == enums::toString(*order_type_));
            }

            if (status_)
            {
                query.where.add(t.status == enums::toString(*status_));
            }

            if (created_at_)
            {
                query.where.add(t.created_at == *created_at_);
            }
        }


       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< boost::uuids::uuid > trade_id_;
        std::optional< boost::uuids::uuid > session_id_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< enums::OrderSide > order_side_;
        std::optional< enums::OrderType > order_type_;
        std::optional< enums::OrderStatus > status_;
        std::optional< int64_t > created_at_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Order > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                              const Filter& filter)

    {
        return db::findByFilter< Order, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    std::optional< boost::uuids::uuid > trade_id_;
    boost::uuids::uuid session_id_;
    std::optional< std::string > exchange_id_;
    std::string symbol_;
    enums::ExchangeName exchange_name_;
    enums::OrderSide order_side_;
    enums::OrderType order_type_;
    bool reduce_only_  = false;
    double qty_        = 0.0;
    double filled_qty_ = 0.0;
    std::optional< double > price_;
    enums::OrderStatus status_ = enums::OrderStatus::ACTIVE;
    int64_t created_at_        = 0;
    std::optional< int64_t > executed_at_;
    std::optional< int64_t > canceled_at_;
    nlohmann::json vars_ = nlohmann::json::object();
    enums::OrderSubmittedVia submitted_via_;

    friend std::ostream& operator<<(std::ostream& os, const Order& order);
};

inline std::ostream& operator<<(std::ostream& os, const Order& order)
{
    os << "Order { " << "id: " << order.id_
       << ", trade_id: " << (order.trade_id_ ? to_string(*order.trade_id_) : "null")
       << ", session_id: " << order.session_id_
       << ", exchange_id: " << (order.exchange_id_ ? *order.exchange_id_ : "null") << ", symbol: " << order.symbol_
       << ", exchange_name: " << order.exchange_name_ << ", order_side: " << order.order_side_
       << ", order_type: " << order.order_type_ << ", reduce_only: " << std::boolalpha << order.reduce_only_
       << ", qty: " << order.qty_ << ", filled_qty: " << order.filled_qty_
       << ", price: " << (order.price_ ? std::to_string(*order.price_) : "null") << ", status: " << order.status_
       << ", created_at: " << order.created_at_
       << ", executed_at: " << (order.executed_at_ ? std::to_string(*order.executed_at_) : "null")
       << ", canceled_at: " << (order.canceled_at_ ? std::to_string(*order.canceled_at_) : "null")
       << ", vars: " << order.vars_.dump() << ", submitted_via: " << order.submitted_via_ << " }";
    return os;
}

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
                      candle::ExchangeName,
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

    Candle(

        int64_t timestamp,
        double open,
        double close,
        double high,
        double low,
        double volume,
        enums::ExchangeName exchange_name,
        std::string symbol,
        enums::Timeframe timeframe)
        : id_(boost::uuids::random_generator()())
        , timestamp_(timestamp)
        , open_(open)
        , close_(close)
        , high_(high)
        , low_(low)
        , volume_(volume)
        , exchange_name_(exchange_name)
        , symbol_(symbol)
        , timeframe_(timeframe)
    {
    }

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

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const enums::Timeframe& getTimeframe() const { return timeframe_; }
    void setTimeframe(const enums::Timeframe& timeframe) { timeframe_ = timeframe; }

    static inline auto table() { return CandlesTable{}; }
    static inline std::string modelName() { return "Candle"; }

    // Convert DB row to model instance
    template < typename ROW >
    static Candle fromRow(const ROW& row)
    {
        Candle candle;
        candle.id_            = boost::uuids::string_generator()(row.id.value());
        candle.timestamp_     = row.timestamp;
        candle.open_          = row.open;
        candle.close_         = row.close;
        candle.high_          = row.high;
        candle.low_           = row.low;
        candle.volume_        = row.volume;
        candle.exchange_name_ = enums::toExchangeName(row.exchange_name);
        candle.symbol_        = row.symbol;
        candle.timeframe_     = enums::toTimeframe(row.timeframe);
        return candle;
    }

    auto prepareSelectStatementForConflictCheck(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter()
                          .withExchangeName(exchange_name_)
                          .withSymbol(symbol_)
                          .withTimeframe(timeframe_)
                          .withTimestamp(timestamp_);

        filter.applyToQuery(query, t);

        return query;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                               t.timestamp     = timestamp_,
                                                               t.open          = open_,
                                                               t.close         = close_,
                                                               t.high          = high_,
                                                               t.low           = low_,
                                                               t.volume        = volume_,
                                                               t.exchange_name = enums::toString(exchange_name_),
                                                               t.symbol        = symbol_,
                                                               t.timeframe     = enums::toString(timeframe_));
    }

    // Prepare update statement
    auto prepareUpdateStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp     = timestamp_,
                         t.open          = open_,
                         t.close         = close_,
                         t.high          = high_,
                         t.low           = low_,
                         t.volume        = volume_,
                         t.exchange_name = enums::toString(exchange_name_),
                         t.symbol        = symbol_,
                         t.timeframe     = enums::toString(timeframe_))
            .dynamic_where(t.id == parameter(t.id));
    }

    // Simplified public interface methods that use the generic functions
    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Candle > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                            const boost::uuids::uuid& id)
    {
        return db::findById< Candle >(conn_ptr, id);
    }

    // TODO:
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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withSymbol(std::string symbol)
        {
            symbol_ = std::move(symbol);
            return *this;
        }

        Filter& withTimeframe(enums::Timeframe timeframe)
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
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }
            if (symbol_)
            {
                query.where.add(t.symbol == *symbol_);
            }
            if (timeframe_)
            {
                query.where.add(t.timeframe == enums::toString(*timeframe_));
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< double > open_;
        std::optional< double > close_;
        std::optional< double > high_;
        std::optional< double > low_;
        std::optional< double > volume_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< std::string > symbol_;
        std::optional< enums::Timeframe > timeframe_;
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
    enums::ExchangeName exchange_name_;
    std::string symbol_;
    enums::Timeframe timeframe_;

    friend std::ostream& operator<<(std::ostream& os, const Candle& candle);
};

/**
 * @brief Store candles data into the database
 *
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 * @param timeframe Timeframe string
 * @param candles Blaze matrix containing candle data
 */
inline void saveCandles(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                        const enums::ExchangeName& exchange_name,
                        const std::string& symbol,
                        const enums::Timeframe& timeframe,
                        const blaze::DynamicMatrix< double >& candles)
{
    // Make sure the number of candles is more than 0
    if (candles.rows() == 0)
    {
        throw std::runtime_error("No candles to store for " + enums::toString(exchange_name) + "-" + symbol + "-" +
                                 enums::toString(timeframe));
    }

    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = Candle::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    auto stmt = sqlpp::insert_into(t).columns(
        t.id, t.timestamp, t.open, t.close, t.high, t.low, t.volume, t.exchange_name, t.symbol, t.timeframe);

    for (size_t i = 0; i < candles.rows(); ++i)
    {
        auto id = boost::uuids::to_string(boost::uuids::random_generator()());
        stmt.values.add(t.id            = id,
                        t.timestamp     = static_cast< int64_t >(candles(i, 0)),
                        t.open          = candles(i, 1),
                        t.close         = candles(i, 2),
                        t.high          = candles(i, 3),
                        t.low           = candles(i, 4),
                        t.volume        = candles(i, 5),
                        t.exchange_name = enums::toString(exchange_name),
                        t.symbol        = symbol,
                        t.timeframe     = enums::toString(timeframe));
    }

    try
    {
        conn(stmt);
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error saving candles: " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
    }
}

inline std::ostream& operator<<(std::ostream& os, const Candle& candle)
{
    os << "Candle { " << "id: " << candle.id_ << ", timestamp: " << candle.timestamp_ << ", open: " << candle.open_
       << ", close: " << candle.close_ << ", high: " << candle.high_ << ", low: " << candle.low_
       << ", volume: " << candle.volume_ << ", exchange_name: " << candle.exchange_name_
       << ", symbol: " << candle.symbol_ << ", timeframe: " << candle.timeframe_ << " }";
    return os;
}

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

struct PositionType
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "position_type";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T position_type;
            T& operator()() { return position_type; }
            const T& operator()() const { return position_type; }
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
                      closed_trade::ExchangeName,
                      closed_trade::PositionType,
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

    ClosedTrade(std::string strategy_name,
                std::string symbol,
                enums::ExchangeName exchange_name,
                enums::PositionType position_type,
                enums::Timeframe timeframe,
                int64_t opened_at,
                int64_t closed_at,
                int leverage)
        : id_(boost::uuids::random_generator()())
        , strategy_name_(strategy_name)
        , symbol_(symbol)
        , exchange_name_(exchange_name)
        , position_type_(position_type)
        , timeframe_(timeframe)
        , opened_at_(opened_at)
        , closed_at_(closed_at)
        , leverage_(leverage)
        , buy_orders_({10, 2})
        , sell_orders_({10, 2})
        , orders_()
    {
    }

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

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    const enums::PositionType& getPositionType() const { return position_type_; }
    void setPositionType(const enums::PositionType& position_type) { position_type_ = position_type; }

    const enums::Timeframe& getTimeframe() const { return timeframe_; }
    void setTimeframe(const enums::Timeframe& timeframe) { timeframe_ = timeframe; }

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
    nlohmann::json toJson() const;
    nlohmann::json toJsonWithOrders() const;

    static inline auto table() { return ClosedTradesTable{}; }
    static inline std::string modelName() { return "ClosedTrade"; }

    // Convert DB row to model instance
    template < typename ROW >
    static ClosedTrade fromRow(const ROW& row)
    {
        ClosedTrade closedTrade;

        closedTrade.id_            = boost::uuids::string_generator()(row.id.value());
        closedTrade.strategy_name_ = row.strategy_name;
        closedTrade.symbol_        = row.symbol;
        closedTrade.exchange_name_ = enums::toExchangeName(row.exchange_name);
        closedTrade.position_type_ = enums::toPositionType(row.position_type);
        closedTrade.timeframe_     = enums::toTimeframe(row.timeframe);
        closedTrade.opened_at_     = row.opened_at;
        closedTrade.closed_at_     = row.closed_at;
        closedTrade.leverage_      = row.leverage;

        return closedTrade;
    }

    auto prepareSelectStatementForConflictCheck(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withId(id_);

        filter.applyToQuery(query, t);

        return query;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                               t.strategy_name = strategy_name_,
                                                               t.symbol        = symbol_,
                                                               t.exchange_name = enums::toString(exchange_name_),
                                                               t.position_type = enums::toString(position_type_),
                                                               t.timeframe     = enums::toString(timeframe_),
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
                         t.exchange_name = enums::toString(exchange_name_),
                         t.position_type = enums::toString(position_type_),
                         t.timeframe     = enums::toString(timeframe_),
                         t.opened_at     = opened_at_,
                         t.closed_at     = closed_at_,
                         t.leverage      = leverage_)
            .dynamic_where(t.id == parameter(t.id));
    }

    // Simplified public interface methods that use the generic functions
    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withPositionType(enums::PositionType position_type)
        {
            position_type_ = std::move(position_type);
            return *this;
        }

        Filter& withTimeframe(enums::Timeframe timeframe)
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
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }
            if (position_type_)
            {
                query.where.add(t.position_type == enums::toString(*position_type_));
            }
            if (timeframe_)
            {
                query.where.add(t.timeframe == enums::toString(*timeframe_));
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
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > strategy_name_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< enums::PositionType > position_type_;
        std::optional< enums::Timeframe > timeframe_;
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
    enums::ExchangeName exchange_name_;
    enums::PositionType position_type_;
    enums::Timeframe timeframe_;
    int64_t opened_at_ = 0;
    int64_t closed_at_ = 0;
    int leverage_      = 1;

    // Using Blaze for fast numerical calculations
    datastructure::DynamicBlazeArray< double > buy_orders_;
    datastructure::DynamicBlazeArray< double > sell_orders_;
    std::vector< Order > orders_;

    friend std::ostream& operator<<(std::ostream& os, const ClosedTrade& closedTrade);
};

inline std::ostream& operator<<(std::ostream& os, const ClosedTrade& closedTrade)
{
    os << "ClosedTrade { " << "id: " << closedTrade.id_ << ", strategy_name: " << closedTrade.strategy_name_
       << ", symbol: " << closedTrade.symbol_ << ", exchange_name: " << closedTrade.exchange_name_
       << ", position_type: " << closedTrade.position_type_ << ", timeframe: " << closedTrade.timeframe_
       << ", opened_at: " << closedTrade.opened_at_ << ", closed_at: " << closedTrade.closed_at_
       << ", leverage: " << closedTrade.leverage_ << " }";
    return os;
}

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
                      daily_balance::ExchangeName,
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

    DailyBalance(int64_t timestamp,
                 std::optional< std::string > identifier,
                 enums::ExchangeName exchange_name,
                 std::string asset,
                 double balance)
        : id_(boost::uuids::random_generator()())
        , timestamp_(timestamp)
        , identifier_(identifier)
        , exchange_name_(exchange_name)
        , asset_(asset)
        , balance_(balance)
    {
    }

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

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

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

        balance.exchange_name_ = enums::toExchangeName(row.exchange_name);
        balance.asset_         = row.asset;
        balance.balance_       = row.balance;

        return balance;
    }

    auto prepareSelectStatementForConflictCheck(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter()
                          .withIdentifier(identifier_.value_or(""))
                          .withExchangeName(exchange_name_)
                          .withAsset(asset_)
                          .withTimestamp(timestamp_);


        filter.applyToQuery(query, t);

        return query;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt = sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                                    t.timestamp     = timestamp_,
                                                                    t.exchange_name = enums::toString(exchange_name_),
                                                                    t.asset         = asset_,
                                                                    t.balance       = balance_);

        if (identifier_ && *identifier_ != "")
        {
            stmt.insert_list.add(t.identifier = *identifier_);
        }

        return stmt;
    }

    // Prepare update statement
    auto prepareUpdateStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto stmt = sqlpp::dynamic_update(conn, t)
                        .dynamic_set(t.timestamp     = timestamp_,
                                     t.exchange_name = enums::toString(exchange_name_),
                                     t.asset         = asset_,
                                     t.balance       = balance_)
                        .dynamic_where(t.id == parameter(t.id));

        if (identifier_)
        {
            if (*identifier_ != "")
            {
                stmt.assignments.add(t.identifier = *identifier_);
            }
            else
            {
                stmt.assignments.add(t.identifier = sqlpp::null);
            }
        }

        return stmt;
    }

    // Simplified public interface methods that use the generic functions
    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withExchangeName(const enums::ExchangeName& exchange_name)
        {
            exchange_name_ = exchange_name;
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
                if (*identifier_ != "")
                {
                    query.where.add(t.identifier == *identifier_);
                }
                else
                {
                    query.where.add(t.identifier.is_null());
                }
            }
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
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
        std::optional< enums::ExchangeName > exchange_name_;
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
    boost::uuids::uuid id_;
    int64_t timestamp_ = 0;
    std::optional< std::string > identifier_; // Can be null
    enums::ExchangeName exchange_name_;
    std::string asset_;
    double balance_ = 0.0;

    friend std::ostream& operator<<(std::ostream& os, const DailyBalance& dailyBalance);
};

inline std::ostream& operator<<(std::ostream& os, const DailyBalance& dailyBalance)
{
    os << "DailyBalance { " << "id: " << dailyBalance.id_ << ", timestamp: " << dailyBalance.timestamp_
       << ", identifier: " << (dailyBalance.identifier_ ? *dailyBalance.identifier_ : "null")
       << ", exchange_name: " << dailyBalance.exchange_name_ << ", asset: " << dailyBalance.asset_
       << ", balance: " << dailyBalance.balance_ << " }";
    return os;
}

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

    ExchangeApiKeys(enums::ExchangeName exchange_name,
                    std::string name,
                    std::string api_key,
                    std::string api_secret,
                    std::string additional_fields,
                    int64_t created_at)
        : id_(boost::uuids::random_generator()())
        , exchange_name_(exchange_name)
        , name_(name)
        , api_key_(api_key)
        , api_secret_(api_secret)
        , additional_fields_(additional_fields)
        , created_at_(created_at)
    {
    }

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

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

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
        api_keys.exchange_name_     = enums::toExchangeName(row.exchange_name);
        api_keys.name_              = row.name;
        api_keys.api_key_           = row.api_key;
        api_keys.api_secret_        = row.api_secret;
        api_keys.additional_fields_ = row.additional_fields;
        api_keys.created_at_        = row.created_at;
        return api_keys;
    }

    auto prepareSelectStatementForConflictCheck(const ExchangeApiKeysTable& t,
                                                sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withName(name_);


        filter.applyToQuery(query, t);

        return query;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const ExchangeApiKeysTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id                = getIdAsString(),
                                                               t.exchange_name     = enums::toString(exchange_name_),
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
            .dynamic_set(t.exchange_name     = enums::toString(exchange_name_),
                         t.name              = name_,
                         t.api_key           = api_key_,
                         t.api_secret        = api_secret_,
                         t.additional_fields = additional_fields_,
                         t.created_at        = created_at_)
            .dynamic_where(t.id == parameter(t.id));
    }

    // Save to database
    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
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
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }
            if (name_)
            {
                query.where.add(t.name == *name_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< enums::ExchangeName > exchange_name_;
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
    enums::ExchangeName exchange_name_;
    std::string name_;
    std::string api_key_;
    std::string api_secret_;
    std::string additional_fields_ = "{}";
    int64_t created_at_;

    friend std::ostream& operator<<(std::ostream& os, const ExchangeApiKeys& exchangeApiKeys);
};

inline std::ostream& operator<<(std::ostream& os, const ExchangeApiKeys& exchangeApiKeys)
{
    os << "ApiCredential { " << "id: " << exchangeApiKeys.id_ << ", exchange_name: " << exchangeApiKeys.exchange_name_
       << ", name: " << exchangeApiKeys.name_ << ", api_key: " << "[REDACTED]" << ", api_secret: " << "[REDACTED]"
       << ", additional_fields: " << exchangeApiKeys.additional_fields_
       << ", created_at: " << exchangeApiKeys.created_at_ << " }";
    return os;
}

namespace log
{
// Log type enum for type safety
enum class LogLevel : int16_t
{
    INFO    = 1,
    ERROR   = 2,
    WARNING = 3,
    DEBUG   = 4
};

inline std::string toString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, LogLevel logLevel)
{
    return os << toString(logLevel);
}

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

struct Level
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "level";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T level;
            T& operator()() { return level; }
            const T& operator()() const { return level; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::smallint >;
};
} // namespace log

// Define the Log Table
struct LogTable : sqlpp::table_t< LogTable, log::Id, log::SessionId, log::Timestamp, log::Message, log::Level >
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

    Log(boost::uuids::uuid session_id, int64_t timestamp, std::string message, log::LogLevel level)
        : id_(boost::uuids::random_generator()())
        , session_id_(session_id)
        , timestamp_(timestamp)
        , message_(message)
        , level_(level)
    {
    }

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

    log::LogLevel getLevel() const { return level_; }
    void setLevel(log::LogLevel type) { level_ = type; }

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
        int16_t type_value = row.level;
        switch (type_value)
        {
            case static_cast< int16_t >(log::LogLevel::INFO):
                log.level_ = log::LogLevel::INFO;
                break;
            case static_cast< int16_t >(log::LogLevel::ERROR):
                log.level_ = log::LogLevel::ERROR;
                break;
            case static_cast< int16_t >(log::LogLevel::WARNING):
                log.level_ = log::LogLevel::WARNING;
                break;
            case static_cast< int16_t >(log::LogLevel::DEBUG):
                log.level_ = log::LogLevel::DEBUG;
                break;
            default:
                // Fallback to default type if unknown value
                log.level_ = log::LogLevel::INFO;
                break;
        }

        return log;
    }

    auto prepareSelectStatementForConflictCheck(const LogTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withId(id_);


        filter.applyToQuery(query, t);

        return query;
    }

    // Prepare insert statement
    auto prepareInsertStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id         = boost::uuids::to_string(id_),
                                                               t.session_id = boost::uuids::to_string(session_id_),
                                                               t.timestamp  = timestamp_,
                                                               t.message    = message_,
                                                               t.level      = static_cast< int16_t >(level_));
    }

    // Prepare update statement
    auto prepareUpdateStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.session_id = boost::uuids::to_string(session_id_),
                         t.timestamp  = timestamp_,
                         t.message    = message_,
                         t.level      = static_cast< int16_t >(level_))
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

        Filter& withLevel(log::LogLevel level)
        {
            level_ = level;
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
            if (level_)
            {
                query.where.add(t.level == static_cast< int16_t >(*level_));
            }
            if (start_timestamp_ && end_timestamp_)
            {
                query.where.add(t.timestamp >= *start_timestamp_ && t.timestamp <= *end_timestamp_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< boost::uuids::uuid > session_id_;
        std::optional< log::LogLevel > level_;
        std::optional< int64_t > start_timestamp_;
        std::optional< int64_t > end_timestamp_;
    };

    // Static factory method for creating a filter
    static Filter createFilter() { return Filter{}; }

    // Simplified public interface methods
    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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
    log::LogLevel level_{log::LogLevel::INFO};

    friend std::ostream& operator<<(std::ostream& os, const Log& log);
};

inline std::ostream& operator<<(std::ostream& os, const Log& log)
{
    os << "Log { " << "id: " << log.id_ << ", session_id: " << log.session_id_ << ", timestamp: " << log.timestamp_
       << ", level: " << log.level_ << ", message: " << log.message_ << " }";
    return os;
}

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

    NotificationApiKeys(std::string name, std::string driver, std::string fields_json, int64_t created_at)
        : id_(boost::uuids::random_generator()())
        , name_(name)
        , driver_(driver)
        , fields_json_(fields_json)
        , created_at_(created_at)
    {
    }

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

    auto prepareSelectStatementForConflictCheck(const NotificationApiKeysTable& t,
                                                sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withName(name_);


        filter.applyToQuery(query, t);

        return query;
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

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

    friend std::ostream& operator<<(std::ostream& os, const NotificationApiKeys& key);
};

inline std::ostream& operator<<(std::ostream& os, const NotificationApiKeys& key)
{
    os << "NotificationApiKeys { " << "id: " << key.id_ << ", name: " << key.name_ << ", driver: " << key.driver_
       << ", fields_json: " << key.fields_json_ << ", created_at: " << key.created_at_ << " }";
    return os;
}

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

struct OptionType
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "option_type";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T option_type;
            T& operator()() { return option_type; }
            const T& operator()() const { return option_type; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

struct Value
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "value";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T value;
            T& operator()() { return value; }
            const T& operator()() const { return value; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::varchar >;
};

} // namespace option

// Define the table structure
struct OptionsTable : sqlpp::table_t< OptionsTable, option::Id, option::UpdatedAt, option::OptionType, option::Value >
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

    Option(int64_t updated_at, std::string option_type, std::string value)
        : id_(boost::uuids::random_generator()()), updated_at_(updated_at), option_type_(option_type), value_(value)
    {
    }

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

    const std::string& getOptionType() const { return option_type_; }
    void setOptionType(const std::string& option_type) { option_type_ = option_type; }

    // JSON field handling
    nlohmann::json getValue() const
    {
        try
        {
            return nlohmann::json::parse(value_);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return nlohmann::json::object();
        }
    }

    void setValue(const nlohmann::json& value) { value_ = value.dump(); }
    void setValueStr(const std::string& json_str)
    {
        try
        {
            // Validate that the string is valid JSON
            auto j = nlohmann::json::parse(json_str);
            value_ = j.dump();
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
        option.id_          = boost::uuids::string_generator()(row.id.value());
        option.updated_at_  = row.updated_at;
        option.option_type_ = row.option_type;
        option.value_       = row.value;
        return option;
    }

    auto prepareSelectStatementForConflictCheck(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withId(id_);


        filter.applyToQuery(query, t);

        return query;
    }

    auto prepareInsertStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(
            t.id = getIdAsString(), t.updated_at = updated_at_, t.option_type = option_type_, t.value = value_);
    }

    auto prepareUpdateStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.updated_at = updated_at_, t.option_type = option_type_, t.value = value_)
            .dynamic_where(t.id == parameter(t.id));
    }

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withOptionType(std::string option_type)
        {
            option_type_ = std::move(option_type);
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const
        {
            if (id_)
            {
                query.where.add(t.id == boost::uuids::to_string(*id_));
            }
            if (option_type_)
            {
                query.where.add(t.option_type == *option_type_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > option_type_;
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
    std::string option_type_;
    std::string value_ = "{}";

    friend std::ostream& operator<<(std::ostream& os, const Option& option);
};

inline std::ostream& operator<<(std::ostream& os, const Option& option)
{
    os << "Option { " << "id: " << option.id_ << ", updated_at: " << option.updated_at_
       << ", option_type: " << option.option_type_ << ", value: " << option.value_ << " }";
    return os;
}

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
                      orderbook::ExchangeName,
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

    Orderbook(int64_t timestamp, std::string symbol, enums::ExchangeName exchange_name, std::vector< uint8_t > data)
        : id_(boost::uuids::random_generator()())
        , timestamp_(timestamp)
        , symbol_(symbol)
        , exchange_name_(exchange_name)
        , data_(data)
    {
    }

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

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

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
        orderbook.id_            = boost::uuids::string_generator()(row.id.value());
        orderbook.timestamp_     = row.timestamp;
        orderbook.symbol_        = row.symbol;
        orderbook.exchange_name_ = enums::toExchangeName(row.exchange_name);

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

    auto prepareSelectStatementForConflictCheck(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

        filter.applyToQuery(query, t);

        return query;
    }

    auto prepareInsertStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
    {
        // Prepare the dynamic insert statement with placeholders
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                               t.timestamp     = timestamp_,
                                                               t.symbol        = symbol_,
                                                               t.exchange_name = enums::toString(exchange_name_),
                                                               t.data          = data_);
    }

    auto prepareUpdateStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp     = timestamp_,
                         t.symbol        = symbol_,
                         t.exchange_name = enums::toString(exchange_name_),
                         t.data          = data_)
            .dynamic_where(t.id == parameter(t.id));
    }

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
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
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
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
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
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
    enums::ExchangeName exchange_name_;
    std::vector< uint8_t > data_;

    friend std::ostream& operator<<(std::ostream& os, const Orderbook& orderbook);
};

inline std::ostream& operator<<(std::ostream& os, const Orderbook& orderbook)
{
    os << "Orderbook { " << "id: " << orderbook.id_ << ", timestamp: " << orderbook.timestamp_
       << ", symbol: " << orderbook.symbol_ << ", exchange_name: " << orderbook.exchange_name_
       << ", data_size: " << orderbook.data_.size() // Printing the size of data (since it's a vector of uint8_t)
       << " }";
    return os;
}

namespace ticker
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

struct LastPrice
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "last_price";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T last_price;
            T& operator()() { return last_price; }
            const T& operator()() const { return last_price; }
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

struct HighPrice
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "high_price";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T high_price;
            T& operator()() { return high_price; }
            const T& operator()() const { return high_price; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct LowPrice
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "low_price";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T low_price;
            T& operator()() { return low_price; }
            const T& operator()() const { return low_price; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
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
} // namespace ticker

// Define the table structure
struct TickersTable
    : sqlpp::table_t< TickersTable,
                      ticker::Id,
                      ticker::Timestamp,
                      ticker::LastPrice,
                      ticker::Volume,
                      ticker::HighPrice,
                      ticker::LowPrice,
                      ticker::Symbol,
                      ticker::ExchangeName >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "tickers";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T tickers;
            T& operator()() { return tickers; }
            const T& operator()() const { return tickers; }
        };
    };
};

class Ticker
{
   public:
    // Default constructor with random UUID generation
    Ticker();

    // Constructor with attribute map
    explicit Ticker(const std::unordered_map< std::string, std::any >& attributes);

    Ticker(int64_t timestamp,
           double last_price,
           double volume,
           double high_price,
           double low_price,
           std::string symbol,
           enums::ExchangeName exchange_name)
        : id_(boost::uuids::random_generator()())
        , timestamp_(timestamp)
        , last_price_(last_price)
        , volume_(volume)
        , high_price_(high_price)
        , low_price_(low_price)
        , symbol_(symbol)
        , exchange_name_(exchange_name)
    {
    }

    // Rule of five
    Ticker(const Ticker&)                = default;
    Ticker(Ticker&&) noexcept            = default;
    Ticker& operator=(const Ticker&)     = default;
    Ticker& operator=(Ticker&&) noexcept = default;
    ~Ticker()                            = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    double getLastPrice() const { return last_price_; }
    void setLastPrice(double last_price) { last_price_ = last_price; }

    double getVolume() const { return volume_; }
    void setVolume(double volume) { volume_ = volume; }

    double getHighPrice() const { return high_price_; }
    void setHighPrice(double high_price) { high_price_ = high_price; }

    double getLowPrice() const { return low_price_; }
    void setLowPrice(double low_price) { low_price_ = low_price; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    // Database operations
    static inline auto table() { return TickersTable{}; }
    static inline std::string modelName() { return "Ticker"; }

    template < typename ROW >
    static Ticker fromRow(const ROW& row)
    {
        Ticker ticker;
        ticker.id_            = boost::uuids::string_generator()(row.id.value());
        ticker.timestamp_     = row.timestamp;
        ticker.last_price_    = row.last_price;
        ticker.volume_        = row.volume;
        ticker.high_price_    = row.high_price;
        ticker.low_price_     = row.low_price;
        ticker.symbol_        = row.symbol;
        ticker.exchange_name_ = enums::toExchangeName(row.exchange_name);
        return ticker;
    }

    auto prepareSelectStatementForConflictCheck(const TickersTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

        filter.applyToQuery(query, t);

        return query;
    }

    auto prepareInsertStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                               t.timestamp     = timestamp_,
                                                               t.last_price    = last_price_,
                                                               t.volume        = volume_,
                                                               t.high_price    = high_price_,
                                                               t.low_price     = low_price_,
                                                               t.symbol        = symbol_,
                                                               t.exchange_name = enums::toString(exchange_name_));
    }

    auto prepareUpdateStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp     = timestamp_,
                         t.last_price    = last_price_,
                         t.volume        = volume_,
                         t.high_price    = high_price_,
                         t.low_price     = low_price_,
                         t.symbol        = symbol_,
                         t.exchange_name = enums::toString(exchange_name_))
            .dynamic_where(t.id == parameter(t.id));
    }

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Ticker > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                            const boost::uuids::uuid& id)
    {
        return db::findById< Ticker >(conn_ptr, id);
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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withTimestampRange(int64_t start, int64_t end)
        {
            timestamp_start_ = start;
            timestamp_end_   = end;
            return *this;
        }

        Filter& withLastPriceRange(double min, double max)
        {
            last_price_min_ = min;
            last_price_max_ = max;
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
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }

            // Handle timestamp range
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

            // Handle price range
            if (last_price_min_ && last_price_max_)
            {
                query.where.add(t.last_price >= *last_price_min_);
                query.where.add(t.last_price <= *last_price_max_);
            }
            else if (last_price_min_)
            {
                query.where.add(t.last_price >= *last_price_min_);
            }
            else if (last_price_max_)
            {
                query.where.add(t.last_price <= *last_price_max_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        std::optional< double > last_price_min_;
        std::optional< double > last_price_max_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Ticker > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< Ticker, Filter >(conn_ptr, filter);
    }

    // Method to find the latest ticker for a symbol on an exchange
    static std::optional< Ticker > findLatest(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                              const std::string& symbol,
                                              const enums::ExchangeName& exchange_name);

   private:
    boost::uuids::uuid id_;
    int64_t timestamp_{};
    double last_price_{};
    double volume_{};
    double high_price_{};
    double low_price_{};
    std::string symbol_{};
    enums::ExchangeName exchange_name_;

    friend std::ostream& operator<<(std::ostream& os, const Ticker& ticker);
};

/**
 * @brief Store ticker data into the database
 *
 * @param conn_ptr Database connection pointer
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 * @param ticker Blaze matrix containing ticker data
 */
inline void saveTicker(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                       const enums::ExchangeName& exchange_name,
                       const std::string& symbol,
                       const blaze::DynamicMatrix< double >& ticker)
{
    // Make sure the ticker data is valid
    if (ticker.rows() == 0 || ticker.columns() < 5)
    {
        throw std::runtime_error("Invalid ticker data for " + enums::toString(exchange_name) + "-" + symbol);
    }

    // Use the provided connection if available, otherwise get the default connection
    auto& conn    = *(conn_ptr ? conn_ptr : Database::getInstance().getConnection());
    const auto& t = Ticker::table();

    // Create state guard for this connection
    ConnectionStateGuard stateGuard(conn);

    // Create a data structure for the ticker
    auto tickerId     = helper::generateUniqueId();
    int64_t timestamp = static_cast< int64_t >(ticker(0, 0));
    double lastPrice  = ticker(0, 1);
    double highPrice  = ticker(0, 2);
    double lowPrice   = ticker(0, 3);
    double volume     = ticker(0, 4);

    // Prepare the insert statement
    auto stmt = sqlpp::insert_into(t).columns(
        t.id, t.timestamp, t.last_price, t.high_price, t.low_price, t.volume, t.symbol, t.exchange_name);

    stmt.values.add(t.id            = tickerId,
                    t.timestamp     = timestamp,
                    t.last_price    = lastPrice,
                    t.high_price    = highPrice,
                    t.low_price     = lowPrice,
                    t.volume        = volume,
                    t.symbol        = symbol,
                    t.exchange_name = enums::toString(exchange_name));

    try
    {
        // Execute the statement
        conn(stmt);
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error saving ticker: " << e.what();
        logger::LOG.error(oss.str());

        // Mark the connection for reset
        stateGuard.markForReset();

        throw;
    }
}

inline std::ostream& operator<<(std::ostream& os, const Ticker& ticker)
{
    os << "Ticker { " << "id: " << ticker.id_ << ", timestamp: " << ticker.timestamp_
       << ", last_price: " << ticker.last_price_ << ", volume: " << ticker.volume_
       << ", high_price: " << ticker.high_price_ << ", low_price: " << ticker.low_price_
       << ", symbol: " << ticker.symbol_ << ", exchange_name: " << ticker.exchange_name_ << " }";
    return os;
}

namespace trade
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

struct Price
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "price";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T price;
            T& operator()() { return price; }
            const T& operator()() const { return price; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct BuyQty
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "buy_qty";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T buy_qty;
            T& operator()() { return buy_qty; }
            const T& operator()() const { return buy_qty; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct SellQty
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "sell_qty";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T sell_qty;
            T& operator()() { return sell_qty; }
            const T& operator()() const { return sell_qty; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::floating_point >;
};

struct BuyCount
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "buy_count";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T buy_count;
            T& operator()() { return buy_count; }
            const T& operator()() const { return buy_count; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::integer >;
};

struct SellCount
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "sell_count";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T sell_count;
            T& operator()() { return sell_count; }
            const T& operator()() const { return sell_count; }
        };
    };
    using _traits = sqlpp::make_traits< sqlpp::integer >;
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
} // namespace trade

struct TradesTable
    : sqlpp::table_t< TradesTable,
                      trade::Id,
                      trade::Timestamp,
                      trade::Price,
                      trade::BuyQty,
                      trade::SellQty,
                      trade::BuyCount,
                      trade::SellCount,
                      trade::Symbol,
                      trade::ExchangeName >
{
    struct _alias_t
    {
        static constexpr const char _literal[] = "trades";
        using _name_t                          = sqlpp::make_char_sequence< sizeof(_literal), _literal >;
        template < typename T >
        struct _member_t
        {
            T trades;
            T& operator()() { return trades; }
            const T& operator()() const { return trades; }
        };
    };
};

class Trade
{
   public:
    // Default constructor with random UUID generation
    Trade();

    // Constructor with attribute map
    explicit Trade(const std::unordered_map< std::string, std::any >& attributes);

    Trade(int64_t timestamp,
          double price,
          double buy_qty,
          double sell_qty,
          int buy_count,
          int sell_count,
          std::string symbol,
          enums::ExchangeName exchange_name)
        : id_(boost::uuids::random_generator()())
        , timestamp_(timestamp)
        , price_(price)
        , buy_qty_(buy_qty)
        , sell_qty_(sell_qty)
        , buy_count_(buy_count)
        , sell_count_(sell_count)
        , symbol_(symbol)
        , exchange_name_(exchange_name)
    {
    }

    // Rule of five
    Trade(const Trade&)                = default;
    Trade(Trade&&) noexcept            = default;
    Trade& operator=(const Trade&)     = default;
    Trade& operator=(Trade&&) noexcept = default;
    ~Trade()                           = default;

    // Getters and setters
    boost::uuids::uuid getId() const { return id_; }
    void setId(const boost::uuids::uuid& id) { id_ = id; }

    std::string getIdAsString() const { return boost::uuids::to_string(id_); }
    void setId(const std::string& id_str) { id_ = boost::uuids::string_generator()(id_str); }

    int64_t getTimestamp() const { return timestamp_; }
    void setTimestamp(int64_t timestamp) { timestamp_ = timestamp; }

    double getPrice() const { return price_; }
    void setPrice(double price) { price_ = price; }

    double getBuyQty() const { return buy_qty_; }
    void setBuyQty(double buy_qty) { buy_qty_ = buy_qty; }

    double getSellQty() const { return sell_qty_; }
    void setSellQty(double sell_qty) { sell_qty_ = sell_qty; }

    int getBuyCount() const { return buy_count_; }
    void setBuyCount(int buy_count) { buy_count_ = buy_count; }

    int getSellCount() const { return sell_count_; }
    void setSellCount(int sell_count) { sell_count_ = sell_count; }

    const std::string& getSymbol() const { return symbol_; }
    void setSymbol(const std::string& symbol) { symbol_ = symbol; }

    const enums::ExchangeName& getExchangeName() const { return exchange_name_; }
    void setExchangeName(const enums::ExchangeName& exchange_name) { exchange_name_ = exchange_name; }

    // Database operations
    static inline auto table() { return TradesTable{}; }
    static inline std::string modelName() { return "Trade"; }

    template < typename ROW >
    static Trade fromRow(const ROW& row)
    {
        Trade trade;
        trade.id_            = boost::uuids::string_generator()(row.id.value());
        trade.timestamp_     = row.timestamp;
        trade.price_         = row.price;
        trade.buy_qty_       = row.buy_qty;
        trade.sell_qty_      = row.sell_qty;
        trade.buy_count_     = row.buy_count;
        trade.sell_count_    = row.sell_count;
        trade.symbol_        = row.symbol;
        trade.exchange_name_ = enums::toExchangeName(row.exchange_name);
        return trade;
    }

    auto prepareSelectStatementForConflictCheck(const TradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
        auto filter = createFilter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

        filter.applyToQuery(query, t);

        return query;
    }

    auto prepareInsertStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                               t.timestamp     = timestamp_,
                                                               t.price         = price_,
                                                               t.buy_qty       = buy_qty_,
                                                               t.sell_qty      = sell_qty_,
                                                               t.buy_count     = buy_count_,
                                                               t.sell_count    = sell_count_,
                                                               t.symbol        = symbol_,
                                                               t.exchange_name = enums::toString(exchange_name_));
    }

    auto prepareUpdateStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const
    {
        return sqlpp::dynamic_update(conn, t)
            .dynamic_set(t.timestamp     = timestamp_,
                         t.price         = price_,
                         t.buy_qty       = buy_qty_,
                         t.sell_qty      = sell_qty_,
                         t.buy_count     = buy_count_,
                         t.sell_count    = sell_count_,
                         t.symbol        = symbol_,
                         t.exchange_name = enums::toString(exchange_name_))
            .dynamic_where(t.id == parameter(t.id));
    }

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Trade > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                           const boost::uuids::uuid& id)
    {
        return db::findById< Trade >(conn_ptr, id);
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

        Filter& withExchangeName(enums::ExchangeName exchange_name)
        {
            exchange_name_ = std::move(exchange_name);
            return *this;
        }

        Filter& withTimestampRange(int64_t start, int64_t end)
        {
            timestamp_start_ = start;
            timestamp_end_   = end;
            return *this;
        }

        Filter& withPriceRange(double min, double max)
        {
            price_min_ = min;
            price_max_ = max;
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
            if (exchange_name_)
            {
                query.where.add(t.exchange_name == enums::toString(*exchange_name_));
            }

            // Handle timestamp range
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

            // Handle price range
            if (price_min_ && price_max_)
            {
                query.where.add(t.price >= *price_min_);
                query.where.add(t.price <= *price_max_);
            }
            else if (price_min_)
            {
                query.where.add(t.price >= *price_min_);
            }
            else if (price_max_)
            {
                query.where.add(t.price <= *price_max_);
            }
        }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        std::optional< double > price_min_;
        std::optional< double > price_max_;
    };

    static Filter createFilter() { return Filter{}; }

    static std::optional< std::vector< Trade > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                              const Filter& filter)
    {
        return db::findByFilter< Trade, Filter >(conn_ptr, filter);
    }

   private:
    boost::uuids::uuid id_;
    int64_t timestamp_ = 0;
    double price_      = 0.0;
    double buy_qty_    = 0.0;
    double sell_qty_   = 0.0;
    int buy_count_     = 0;
    int sell_count_    = 0;
    std::string symbol_;
    enums::ExchangeName exchange_name_;

    friend std::ostream& operator<<(std::ostream& os, const Trade& trade);
};

inline std::ostream& operator<<(std::ostream& os, const Trade& trade)
{
    os << "Trade { " << "id: " << trade.id_ << ", timestamp: " << trade.timestamp_ << ", price: " << trade.price_
       << ", buy_qty: " << trade.buy_qty_ << ", sell_qty: " << trade.sell_qty_ << ", buy_count: " << trade.buy_count_
       << ", sell_count: " << trade.sell_count_ << ", symbol: " << trade.symbol_
       << ", exchange_name: " << trade.exchange_name_ << " }";
    return os;
}

} // namespace db
} // namespace ct

#endif
