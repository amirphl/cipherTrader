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
        activeConnections_++;

        // Create a wrapper that returns the connection to the pool when it's destroyed
        return std::shared_ptr< sqlpp::postgresql::connection >(
            conn.get(), [this](sqlpp::postgresql::connection* conn) { this->returnConnection(conn); });
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
        config->host     = host_;
        config->dbname   = dbname_;
        config->user     = username_;
        config->password = password_;
        config->port     = port_;
        config->debug    = false;

        auto conn = std::make_unique< sqlpp::postgresql::connection >(config);
        availableConnections_.push(std::move(conn));
        initialized_ = true;
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

} // namespace db
} // namespace CipherDB

#endif
