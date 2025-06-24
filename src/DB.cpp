#include "DynamicArray.hpp"

#include "Candle.hpp"
#include "Config.hpp"
#include "DB.hpp"
#include "Enum.hpp"
#include "Helper.hpp"
#include "Logger.hpp"
#include "Timeframe.hpp"

ct::db::DatabaseShutdownManager& ct::db::DatabaseShutdownManager::getInstance()
{
    static DatabaseShutdownManager instance;
    return instance;
}

// Register a hook to be called during shutdown
void ct::db::DatabaseShutdownManager::registerShutdownHook(ShutdownHook hook)
{
    std::lock_guard< std::mutex > lock(hooksMutex_);
    shutdownHooks_.push_back(std::move(hook));
}

// Register a hook to be called after shutdown is complete
void ct::db::DatabaseShutdownManager::registerCompletionHook(ShutdownCompletionHook hook)
{
    std::lock_guard< std::mutex > lock(completionHooksMutex_);
    completionHooks_.push_back(std::move(hook));
}

// Initialize signal handlers
void ct::db::DatabaseShutdownManager::initSignalHandlers()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // Ignore SIGPIPE to prevent crashes on broken connections
    std::signal(SIGPIPE, SIG_IGN);
}

// Check if shutdown is in progress
bool ct::db::DatabaseShutdownManager::isShuttingDown() const
{
    return shuttingDown_.load(std::memory_order_acquire);
}

// Wait for shutdown to complete
void ct::db::DatabaseShutdownManager::waitForShutdown()
{
    if (shutdownFuture_.valid())
    {
        shutdownFuture_.wait();
    }
}

void ct::db::DatabaseShutdownManager::shutdown()
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

void ct::db::DatabaseShutdownManager::handleSignal([[maybe_unused]] int signal)
{
    // TODO: Remove getInstance()?
    getInstance().shutdown();
}

void ct::db::DatabaseShutdownManager::performShutdown()
{
    // Execute shutdown hooks
    {
        std::lock_guard< std::mutex > lock(hooksMutex_);
        for (const auto& hook : shutdownHooks_)
        {
            try
            {
                hook();
            }
            catch (const std::exception& e)
            {
                std::ostringstream oss;
                oss << "Error in shutdown hook: " << e.what();
                logger::LOG.error(oss.str());
            }
        }
    }

    // Wait for all connections to be released
    auto& pool = ConnectionPool::getInstance();
    pool.waitForConnectionsToClose();

    // Execute completion hooks
    {
        std::lock_guard< std::mutex > lock(completionHooksMutex_);
        for (const auto& hook : completionHooks_)
        {
            try
            {
                hook();
            }
            catch (const std::exception& e)
            {
                std::ostringstream oss;
                oss << "Error in completion hook: " << e.what();
                logger::LOG.error(oss.str());
            }
        }
    }
}

// Get singleton instance
ct::db::ConnectionPool& ct::db::ConnectionPool::getInstance()
{
    static ConnectionPool instance;
    return instance;
}

// Initialize the connection pool
void ct::db::ConnectionPool::init(const std::string& host,
                                  const std::string& dbname,
                                  const std::string& username,
                                  const std::string& password,
                                  unsigned int port,
                                  size_t poolSize) // Default pool size
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
std::shared_ptr< sqlpp::postgresql::connection > ct::db::ConnectionPool::getConnection()
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
std::shared_ptr< sqlpp::postgresql::connection > ct::db::ConnectionPool::getConnectionWithHealthCheck()
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
void ct::db::ConnectionPool::setMaxConnections(size_t maxConnections)
{
    std::lock_guard< std::mutex > lock(mutex_);
    maxConnections_ = maxConnections;
}

void ct::db::ConnectionPool::waitForConnectionsToClose()
{
    std::unique_lock< std::mutex > lock(mutex_);

    // Wait until all connections are returned
    while (activeConnections_ > 0)
    {
        connectionReturned_.wait(lock);
    }
}

// Private constructor for singleton
ct::db::ConnectionPool::ConnectionPool() : activeConnections_(0), maxConnections_(20), initialized_(false) {}

// Return a connection to the pool
void ct::db::ConnectionPool::returnConnection(sqlpp::postgresql::connection* conn)
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
void ct::db::ConnectionPool::createNewConnection()
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

// Get singleton instance
ct::db::Database& ct::db::Database::getInstance()
{
    static Database instance;
    return instance;
}

// Initialize the database with connection parameters
void ct::db::Database::init(const std::string& host,
                            const std::string& dbname,
                            const std::string& username,
                            const std::string& password,
                            unsigned int port)
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

// Get a connection from the pool
// sqlpp::postgresql::connection& ct::db::Database::getConnection()
// {
//     // Get connection from pool and cache it in thread_local storage
//     thread_local std::shared_ptr< sqlpp::postgresql::connection > conn =
//     ConnectionPool::getInstance().getConnection();

// return *conn;
// }

std::shared_ptr< sqlpp::postgresql::connection > ct::db::Database::getConnection()
{
    return ConnectionPool::getInstance().getConnection();
}

void ct::db::Database::shutdown()
{
    DatabaseShutdownManager::getInstance().shutdown();
    DatabaseShutdownManager::getInstance().waitForShutdown();
}

// Start a new transaction
std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > ct::db::TransactionManager::startTransaction()
{
    auto db = db::Database::getInstance().getConnection();
    return std::make_shared< sqlpp::transaction_t< sqlpp::postgresql::connection > >(start_transaction(*db));
}

// Commit the transaction
bool ct::db::TransactionManager::commitTransaction(
    std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx)
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
bool ct::db::TransactionManager::rollbackTransaction(
    std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx)
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

ct::db::TransactionGuard::TransactionGuard() : committed_(false)
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

ct::db::TransactionGuard::~TransactionGuard()
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

bool ct::db::TransactionGuard::commit()
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

bool ct::db::TransactionGuard::rollback()
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
std::shared_ptr< sqlpp::postgresql::connection > ct::db::TransactionGuard::getConnection()
{
    return conn_;
}

template < typename Func >
auto ct::db::executeWithRetry(Func&& operation, int maxRetries) -> decltype(operation())
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

ct::db::ConnectionStateGuard::ConnectionStateGuard(sqlpp::postgresql::connection& conn)
    : conn_(conn), needs_reset_(false)
{
}

ct::db::ConnectionStateGuard::~ConnectionStateGuard()
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

void ct::db::ConnectionStateGuard::markForReset()
{
    needs_reset_ = true;
}

// Generic findById implementation
template < typename ModelType >
std::optional< ModelType > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
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
std::optional< std::vector< ModelType > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const FilterType& filter)
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
void ct::db::save(ModelType& model,
                  std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                  const bool update_on_conflict)
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

template < typename ModelType >
void ct::db::batchSave(const std::vector< ModelType >& models,
                       std::shared_ptr< sqlpp::postgresql::connection > conn_ptr)
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

// Default constructor
ct::db::Order::Order(bool should_silent)
    : id_(boost::uuids::random_generator()()), session_id_(boost::uuids::random_generator()())
{
    created_at_ = helper::nowToTimestamp();

    if (helper::isLive())
    {
        // TODO: Handle live trading session ID
        // Get from store.app.session_id
    }

    if (!should_silent)
    {
        if (helper::isLive())
        {
            notifySubmission();
        }

        if (helper::isDebuggable("order_submission") && (isActive() || isQueued()))
        {
            std::string txt = (isQueued() ? "QUEUED" : "SUBMITTED") + std::string(" order: ") + symbol_ + ", " +
                              enums::toString(order_type_) + ", " + enums::toString(order_side_) + ", " +
                              std::to_string(qty_);

            if (price_)
            {
                txt += ", $" + std::to_string(*price_);
            }

            logger::LOG.info(txt);
        }
    }

    // TODO: Handle exchange balance for ordered asset
    // e = selectors.get_exchange(exchange_);
    // e.on_order_submission(*this);
}

// Constructor with attributes
ct::db::Order::Order(const std::unordered_map< std::string, std::any >& attributes, bool should_silent)
    : Order(should_silent)
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

        if (attributes.count("trade_id"))
        {
            if (attributes.at("trade_id").type() == typeid(std::string))
            {
                trade_id_ = boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("trade_id")));
            }
            else if (attributes.at("trade_id").type() == typeid(boost::uuids::uuid))
            {
                trade_id_ = std::any_cast< boost::uuids::uuid >(attributes.at("trade_id"));
            }
        }

        if (attributes.count("session_id"))
        {
            if (attributes.at("session_id").type() == typeid(std::string))
            {
                session_id_ =
                    boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("session_id")));
            }
            else if (attributes.at("session_id").type() == typeid(boost::uuids::uuid))
            {
                session_id_ = std::any_cast< boost::uuids::uuid >(attributes.at("session_id"));
            }
        }

        if (attributes.count("exchange_id"))
            exchange_id_ = std::any_cast< std::string >(attributes.at("exchange_id"));

        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));

        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));

        if (attributes.count("order_side"))
            order_side_ = std::any_cast< enums::OrderSide >(attributes.at("order_side"));

        if (attributes.count("order_type"))
            order_type_ = std::any_cast< enums::OrderType >(attributes.at("order_type"));

        if (attributes.count("reduce_only"))
            reduce_only_ = std::any_cast< bool >(attributes.at("reduce_only"));

        if (attributes.count("qty"))
            qty_ = std::any_cast< double >(attributes.at("qty"));

        if (attributes.count("filled_qty"))
            filled_qty_ = std::any_cast< double >(attributes.at("filled_qty"));

        if (attributes.count("price"))
            price_ = std::any_cast< double >(attributes.at("price"));

        if (attributes.count("status"))
        {
            status_ = std::any_cast< enums::OrderStatus >(attributes.at("status"));
        }

        if (attributes.count("created_at"))
            created_at_ = std::any_cast< int64_t >(attributes.at("created_at"));

        if (attributes.count("executed_at"))
            executed_at_ = std::any_cast< int64_t >(attributes.at("executed_at"));

        if (attributes.count("canceled_at"))
            canceled_at_ = std::any_cast< int64_t >(attributes.at("canceled_at"));

        if (attributes.count("vars"))
        {
            if (attributes.at("vars").type() == typeid(std::string))
            {
                vars_ = nlohmann::json::parse(std::any_cast< std::string >(attributes.at("vars")));
            }
            else if (attributes.at("vars").type() == typeid(nlohmann::json))
            {
                vars_ = std::any_cast< nlohmann::json >(attributes.at("vars"));
            }
        }

        // Handle submitted_via (not stored in database)
        if (attributes.count("submitted_via"))
        {
            submitted_via_ = std::any_cast< enums::OrderSubmittedVia >(attributes.at("submitted_via"));
        }
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Order: ") + e.what());
    }

    // Notify if needed (already handled in the default constructor this one calls)
}

ct::db::Order::Order(std::optional< boost::uuids::uuid > trade_id,
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

// Calculated properties
double ct::db::Order::getValue() const
{
    if (!price_)
    {
        return 0.0;
    }
    return std::abs(qty_) * (*price_);
}

double ct::db::Order::getRemainingQty() const
{
    return helper::prepareQty(std::abs(qty_) - std::abs(filled_qty_), enums::toString(order_side_));
}

// Order state transitions
void ct::db::Order::queueIt()
{
    // NOTE: Precondition?
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

void ct::db::Order::resubmit()
{
    // Don't allow resubmission if the order is not queued
    if (!isQueued())
    {
        throw std::runtime_error("Cannot resubmit an order that is not queued. Current status: " +
                                 enums::toString(status_));
    }

    // Regenerate the order id to avoid errors on the exchange's side
    id_     = boost::uuids::random_generator()();
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

void ct::db::Order::cancel(bool silent, const std::string& source)
{
    if (isCanceled() || isExecuted()) // NOTE: Add isRejected? isLiqu?
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

void ct::db::Order::execute(bool silent)
{
    if (isCanceled() || isExecuted()) // NOTE: Other conditions?
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

void ct::db::Order::executePartially(bool silent)
{
    // NOTE: preconditions?
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
void ct::db::Order::notifySubmission() const
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
ct::db::Order ct::db::Order::generateFakeOrder(const std::unordered_map< std::string, std::any >& attributes)
{
    static int64_t first_timestamp = 1552309186171;
    first_timestamp += 60000;

    // Default values
    auto exchange_name = enums::ExchangeName::SANDBOX;
    auto symbol        = "BTC-USD";
    auto order_side    = enums::OrderSide::BUY;
    auto order_type    = enums::OrderType::LIMIT;
    auto price         = ct::candle::RandomGenerator::getInstance().randint(40, 100);
    auto qty           = ct::candle::RandomGenerator::getInstance().randint(1, 10);
    auto status        = enums::OrderStatus::ACTIVE;
    auto created_at    = first_timestamp;

    // Prepare attributes map with defaults
    std::unordered_map< std::string, std::any > order_attrs;

    // Set the ID
    order_attrs["id"] = boost::uuids::random_generator()();

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

    // NOTE: trade_id and session_id and exchange_id missing.
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

template < typename ROW >
ct::db::Order ct::db::Order::fromRow(const ROW& row)
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

auto ct::db::Order::prepareSelectStatementForConflictCheck(const OrdersTable& t,
                                                           sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter()
                      .withTradeId(trade_id_.value_or(boost::uuids::nil_uuid()))
                      .withExchangeName(exchange_name_)
                      .withSymbol(symbol_)
                      .withStatus(status_)
                      .withCreatedAt(created_at_);

    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Order::prepareInsertStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const
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

auto ct::db::Order::prepareBatchInsertStatement(const std::vector< Order >& models,
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

auto ct::db::Order::prepareUpdateStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const
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

template < typename Query, typename Table >
void ct::db::Order::Filter::applyToQuery(Query& query, const Table& t) const
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

// Return a dictionary representation of the order
nlohmann::json ct::db::Order::toJson() const
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

// Default constructor
ct::db::Candle::Candle() : id_(boost::uuids::random_generator()()) {}

// Constructor with attributes
ct::db::Candle::Candle(const std::unordered_map< std::string, std::any >& attributes) : Candle()
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
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("timeframe"))
            timeframe_ = std::any_cast< timeframe::Timeframe >(attributes.at("timeframe"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Candle: ") + e.what());
    }
}

ct::db::Candle::Candle(

    int64_t timestamp,
    double open,
    double close,
    double high,
    double low,
    double volume,
    enums::ExchangeName exchange_name,
    std::string symbol,
    timeframe::Timeframe timeframe)
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

// Convert DB row to model instance
template < typename ROW >
ct::db::Candle ct::db::Candle::fromRow(const ROW& row)
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
    candle.timeframe_     = timeframe::toTimeframe(row.timeframe);
    return candle;
}

auto ct::db::Candle::prepareSelectStatementForConflictCheck(const CandlesTable& t,
                                                            sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter()
                      .withExchangeName(exchange_name_)
                      .withSymbol(symbol_)
                      .withTimeframe(timeframe_)
                      .withTimestamp(timestamp_);

    filter.applyToQuery(query, t);

    return query;
}

// Prepare insert statement
auto ct::db::Candle::prepareInsertStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
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
                                                           t.timeframe     = timeframe::toString(timeframe_));
}

// Prepare update statement
auto ct::db::Candle::prepareUpdateStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const
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
                     t.timeframe     = timeframe::toString(timeframe_))
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::Candle::Filter::applyToQuery(Query& query, const Table& t) const
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
        query.where.add(t.timeframe == timeframe::toString(*timeframe_));
    }
}

/**
 * @brief Store candles data into the database
 *
 * @param exchange_name Exchange name
 * @param symbol Trading symbol
 * @param timeframe Timeframe string
 * @param candles Blaze matrix containing candle data
 */
void ct::db::saveCandles(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                         const enums::ExchangeName& exchange_name,
                         const std::string& symbol,
                         const timeframe::Timeframe& timeframe,
                         const blaze::DynamicMatrix< double >& candles)
{
    // Make sure the number of candles is more than 0
    if (candles.rows() == 0)
    {
        throw std::runtime_error("No candles to store for " + enums::toString(exchange_name) + "-" + symbol + "-" +
                                 timeframe::toString(timeframe));
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
                        t.timeframe     = timeframe::toString(timeframe));
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

// Default constructor
ct::db::ClosedTrade::ClosedTrade()
    : id_(boost::uuids::random_generator()())
    , buy_orders_(datastructure::DynamicBlazeArray< double >({10, 2}, false))
    , sell_orders_(datastructure::DynamicBlazeArray< double >({10, 2}, false))
{
}

// Constructor with attributes
ct::db::ClosedTrade::ClosedTrade(const std::unordered_map< std::string, std::any >& attributes) : ClosedTrade()
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
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
        if (attributes.count("position_type"))
            position_type_ = std::any_cast< enums::PositionType >(attributes.at("position_type"));
        if (attributes.count("timeframe"))
            timeframe_ = std::any_cast< timeframe::Timeframe >(attributes.at("timeframe"));
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

std::string ct::db::ClosedTrade::getIdAsString() const
{
    return boost::uuids::to_string(id_);
}

void ct::db::ClosedTrade::setId(const std::string& id_str)
{
    id_ = boost::uuids::string_generator()(id_str);
}

void ct::db::ClosedTrade::addBuyOrder(double qty, double price)
{
    blaze::DynamicVector< double, blaze::rowVector > order(2);
    order[0] = qty;
    order[1] = price;
    buy_orders_.append(order);
}

void ct::db::ClosedTrade::addSellOrder(double qty, double price)
{
    blaze::DynamicVector< double, blaze::rowVector > order(2);
    order[0] = qty;
    order[1] = price;
    sell_orders_.append(order);
}

void ct::db::ClosedTrade::addOrder(const Order& order)
{
    orders_.push_back(order);

    if (order.getOrderSide() == enums::OrderSide::BUY)
    {
        addBuyOrder(order.getQty(), order.getPrice().value_or(0));
    }
    else if (order.getOrderSide() == enums::OrderSide::SELL)
    {
        addSellOrder(order.getQty(), order.getPrice().value_or(0));
    }
}

double ct::db::ClosedTrade::getQty() const
{
    if (isLong())
    {
        // double total = 0.0;
        // for (size_t i = 0; i < buy_orders_.size(); ++i)
        // {
        //     total += buy_orders_[static_cast< int >(i)][0];
        // }
        // return total;

        return buy_orders_.sum(0);
    }
    else if (isShort())
    {
        // double total = 0.0;
        // for (size_t i = 0; i < sell_orders_.size(); ++i)
        // {
        //     total += sell_orders_[static_cast< int >(i)][0];
        // }
        // return total;

        return sell_orders_.sum(0);
    }
    return 0.0;
}

double ct::db::ClosedTrade::getEntryPrice() const
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

    // NOTE: Return Nan?
    return qty_sum != 0.0 ? price_sum / qty_sum : std::numeric_limits< double >::quiet_NaN();
}

double ct::db::ClosedTrade::getExitPrice() const
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

    // NOTE: Return Nan?
    return qty_sum != 0.0 ? price_sum / qty_sum : std::numeric_limits< double >::quiet_NaN();
}

double ct::db::ClosedTrade::getFee() const
{
    std::stringstream key;
    key << "env_exchanges_" << enums::toString(exchange_name_) << "_fee";

    auto trading_fee = config::Config::getInstance().getValue< int >(key.str());

    return trading_fee * getQty() * (getEntryPrice() + getExitPrice());
}

double ct::db::ClosedTrade::getSize() const
{
    return getQty() * getEntryPrice();
}

double ct::db::ClosedTrade::getPnl() const
{
    std::stringstream keys;
    keys << "env_exchanges_" << enums::toString(exchange_name_) << "_fee";

    auto fee           = config::Config::getInstance().getValue< int >(keys.str());
    double qty         = getQty();
    double entry_price = getEntryPrice();
    double exit_price  = getExitPrice();

    return helper::estimatePNL(qty, entry_price, exit_price, position_type_, fee);
}

double ct::db::ClosedTrade::getPnlPercentage() const
{
    return getRoi();
}

double ct::db::ClosedTrade::getRoi() const
{
    double total_cost = getTotalCost();
    return total_cost != 0.0 ? (getPnl() / total_cost) * 100.0 : 0.0;
}

double ct::db::ClosedTrade::getTotalCost() const
{
    return getEntryPrice() * std::abs(getQty()) / leverage_;
}

int ct::db::ClosedTrade::getHoldingPeriod() const
{
    return static_cast< int >((closed_at_ - opened_at_) / 1000);
}

bool ct::db::ClosedTrade::isLong() const
{
    return position_type_ == enums::PositionType::LONG;
}

bool ct::db::ClosedTrade::isShort() const
{
    return position_type_ == enums::PositionType::SHORT;
}

bool ct::db::ClosedTrade::isOpen() const
{
    return opened_at_ != 0;
}

nlohmann::json ct::db::ClosedTrade::toJson() const
{
    nlohmann::json result;

    result["id"]             = getIdAsString();
    result["strategy_name"]  = strategy_name_;
    result["symbol"]         = symbol_;
    result["exchange_name"]  = exchange_name_;
    result["position_type"]  = position_type_;
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

nlohmann::json ct::db::ClosedTrade::toJsonWithOrders() const
{
    auto result = toJson();

    std::vector< nlohmann::json > orders;
    for (const auto& order : orders_)
    {
        orders.push_back(order.toJson());
    }

    result["orders"] = orders;
    return result;
}

template < typename ROW >
ct::db::ClosedTrade ct::db::ClosedTrade::fromRow(const ROW& row)
{
    ClosedTrade closedTrade;

    closedTrade.id_            = boost::uuids::string_generator()(row.id.value());
    closedTrade.strategy_name_ = row.strategy_name;
    closedTrade.symbol_        = row.symbol;
    closedTrade.exchange_name_ = enums::toExchangeName(row.exchange_name);
    closedTrade.position_type_ = enums::toPositionType(row.position_type);
    closedTrade.timeframe_     = timeframe::toTimeframe(row.timeframe);
    closedTrade.opened_at_     = row.opened_at;
    closedTrade.closed_at_     = row.closed_at;
    closedTrade.leverage_      = row.leverage;

    return closedTrade;
}

auto ct::db::ClosedTrade::prepareSelectStatementForConflictCheck(const ClosedTradesTable& t,
                                                                 sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withId(id_);

    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::ClosedTrade::prepareInsertStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                           t.strategy_name = strategy_name_,
                                                           t.symbol        = symbol_,
                                                           t.exchange_name = enums::toString(exchange_name_),
                                                           t.position_type = enums::toString(position_type_),
                                                           t.timeframe     = timeframe::toString(timeframe_),
                                                           t.opened_at     = opened_at_,
                                                           t.closed_at     = closed_at_,
                                                           t.leverage      = leverage_);
}

auto ct::db::ClosedTrade::prepareUpdateStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_update(conn, t)
        .dynamic_set(t.strategy_name = strategy_name_,
                     t.symbol        = symbol_,
                     t.exchange_name = enums::toString(exchange_name_),
                     t.position_type = enums::toString(position_type_),
                     t.timeframe     = timeframe::toString(timeframe_),
                     t.opened_at     = opened_at_,
                     t.closed_at     = closed_at_,
                     t.leverage      = leverage_)
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::ClosedTrade::Filter::applyToQuery(Query& query, const Table& t) const
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
        query.where.add(t.timeframe == timeframe::toString(*timeframe_));
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

ct::db::DailyBalance::DailyBalance() : id_(boost::uuids::random_generator()()) {}

ct::db::DailyBalance::DailyBalance(const std::unordered_map< std::string, std::any >& attributes) : DailyBalance()
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
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
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

template < typename ROW >
ct::db::DailyBalance ct::db::DailyBalance::fromRow(const ROW& row)
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

auto ct::db::DailyBalance::prepareSelectStatementForConflictCheck(const DailyBalanceTable& t,
                                                                  sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter()
                      .withIdentifier(identifier_.value_or(""))
                      .withExchangeName(exchange_name_)
                      .withAsset(asset_)
                      .withTimestamp(timestamp_);


    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::DailyBalance::prepareInsertStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
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

auto ct::db::DailyBalance::prepareUpdateStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const
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

template < typename Query, typename Table >
void ct::db::DailyBalance::Filter::applyToQuery(Query& query, const Table& t) const
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

// Default constructor generates a random UUID
ct::db::ExchangeApiKeys::ExchangeApiKeys()
    : id_(boost::uuids::random_generator()())
    , created_at_(
          std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
              .count()) // TODO: Use Helper
{
}

// Constructor with attribute map
ct::db::ExchangeApiKeys::ExchangeApiKeys(const std::unordered_map< std::string, std::any >& attributes)
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
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
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

template < typename ROW >
ct::db::ExchangeApiKeys ct::db::ExchangeApiKeys::fromRow(const ROW& row)
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

auto ct::db::ExchangeApiKeys::prepareSelectStatementForConflictCheck(const ExchangeApiKeysTable& t,
                                                                     sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withName(name_);


    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::ExchangeApiKeys::prepareInsertStatement(const ExchangeApiKeysTable& t,
                                                     sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id                = getIdAsString(),
                                                           t.exchange_name     = enums::toString(exchange_name_),
                                                           t.name              = name_,
                                                           t.api_key           = api_key_,
                                                           t.api_secret        = api_secret_,
                                                           t.additional_fields = additional_fields_,
                                                           t.created_at        = created_at_);
}

auto ct::db::ExchangeApiKeys::prepareUpdateStatement(const ExchangeApiKeysTable& t,
                                                     sqlpp::postgresql::connection& conn) const
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

template < typename Query, typename Table >
void ct::db::ExchangeApiKeys::Filter::applyToQuery(Query& query, const Table& t) const
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

ct::db::Log::Log() : id_(boost::uuids::random_generator()()), timestamp_(0), level_(log::LogLevel::INFO) {}

ct::db::Log::Log(const std::unordered_map< std::string, std::any >& attributes)
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

        if (attributes.count("session_id"))
        {
            if (attributes.at("session_id").type() == typeid(std::string))
            {
                session_id_ =
                    boost::uuids::string_generator()(std::any_cast< std::string >(attributes.at("session_id")));
            }
            else if (attributes.at("session_id").type() == typeid(boost::uuids::uuid))
            {
                session_id_ = std::any_cast< boost::uuids::uuid >(attributes.at("session_id"));
            }
        }
        if (attributes.count("timestamp"))
            timestamp_ = std::any_cast< int64_t >(attributes.at("timestamp"));
        if (attributes.count("message"))
            message_ = std::any_cast< std::string >(attributes.at("message"));
        if (attributes.count("level"))
            level_ = std::any_cast< log::LogLevel >(attributes.at("level"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Log: ") + e.what());
    }
}

template < typename ROW >
ct::db::Log ct::db::Log::fromRow(const ROW& row)
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

auto ct::db::Log::prepareSelectStatementForConflictCheck(const LogTable& t, sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withId(id_);


    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Log::prepareInsertStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id         = boost::uuids::to_string(id_),
                                                           t.session_id = boost::uuids::to_string(session_id_),
                                                           t.timestamp  = timestamp_,
                                                           t.message    = message_,
                                                           t.level      = static_cast< int16_t >(level_));
}

auto ct::db::Log::prepareUpdateStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_update(conn, t)
        .dynamic_set(t.session_id = boost::uuids::to_string(session_id_),
                     t.timestamp  = timestamp_,
                     t.message    = message_,
                     t.level      = static_cast< int16_t >(level_))
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::Log::Filter::applyToQuery(Query& query, const Table& t) const
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

ct::db::NotificationApiKeys::NotificationApiKeys()
    : id_(boost::uuids::random_generator()())
    , created_at_(
          std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
              .count())
{
}

ct::db::NotificationApiKeys::NotificationApiKeys(const std::unordered_map< std::string, std::any >& attributes)
    : NotificationApiKeys()
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

        if (attributes.count("name"))
            name_ = std::any_cast< std::string >(attributes.at("name"));
        if (attributes.count("driver"))
            driver_ = std::any_cast< std::string >(attributes.at("driver"));
        if (attributes.count("fields_json"))
            fields_json_ = std::any_cast< std::string >(attributes.at("fields_json"));
        if (attributes.count("created_at"))
            created_at_ = std::any_cast< int64_t >(attributes.at("created_at"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing NotificationApiKeys: ") + e.what());
    }
}

template < typename ROW >
ct::db::NotificationApiKeys ct::db::NotificationApiKeys::fromRow(const ROW& row)
{
    NotificationApiKeys apiKey;
    apiKey.id_          = boost::uuids::string_generator()(row.id.value());
    apiKey.name_        = row.name;
    apiKey.driver_      = row.driver;
    apiKey.fields_json_ = row.fields;
    apiKey.created_at_  = row.created_at;
    return apiKey;
}

auto ct::db::NotificationApiKeys::prepareSelectStatementForConflictCheck(const NotificationApiKeysTable& t,
                                                                         sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withName(name_);


    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::NotificationApiKeys::prepareInsertStatement(const NotificationApiKeysTable& t,
                                                         sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id         = getIdAsString(),
                                                           t.name       = name_,
                                                           t.driver     = driver_,
                                                           t.fields     = fields_json_,
                                                           t.created_at = created_at_);
}

auto ct::db::NotificationApiKeys::prepareUpdateStatement(const NotificationApiKeysTable& t,
                                                         sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_update(conn, t)
        .dynamic_set(t.name = name_, t.driver = driver_, t.fields = fields_json_, t.created_at = created_at_)
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::NotificationApiKeys::Filter::applyToQuery(Query& query, const Table& t) const
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

ct::db::Option::Option()
    : id_(boost::uuids::random_generator()())
    , updated_at_(
          std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
              .count())
{
}

ct::db::Option::Option(const std::unordered_map< std::string, std::any >& attributes) : Option()
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

        if (attributes.count("updated_at"))
            updated_at_ = std::any_cast< int64_t >(attributes.at("updated_at"));
        if (attributes.count("option_type"))
            option_type_ = std::any_cast< std::string >(attributes.at("option_type"));
        if (attributes.count("value"))
        {
            // If json is provided as a string
            if (attributes.at("value").type() == typeid(std::string))
            {
                setValueStr(std::any_cast< std::string >(attributes.at("value")));
            }
            // If json is provided as a nlohmann::json object
            else if (attributes.at("value").type() == typeid(nlohmann::json))
            {
                setValue(std::any_cast< nlohmann::json >(attributes.at("value")));
            }
        }
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Option: ") + e.what());
    }
}

template < typename ROW >
ct::db::Option ct::db::Option::fromRow(const ROW& row)
{
    Option option;
    option.id_          = boost::uuids::string_generator()(row.id.value());
    option.updated_at_  = row.updated_at;
    option.option_type_ = row.option_type;
    option.value_       = row.value;
    return option;
}

auto ct::db::Option::prepareSelectStatementForConflictCheck(const OptionsTable& t,
                                                            sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withId(id_);


    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Option::prepareInsertStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(
        t.id = getIdAsString(), t.updated_at = updated_at_, t.option_type = option_type_, t.value = value_);
}

auto ct::db::Option::prepareUpdateStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_update(conn, t)
        .dynamic_set(t.updated_at = updated_at_, t.option_type = option_type_, t.value = value_)
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::Option::Filter::applyToQuery(Query& query, const Table& t) const
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

ct::db::Orderbook::Orderbook() : id_(boost::uuids::random_generator()()) {}

ct::db::Orderbook::Orderbook(const std::unordered_map< std::string, std::any >& attributes) : Orderbook()
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
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
        if (attributes.count("data"))
        {
            if (attributes.at("data").type() == typeid(std::vector< uint8_t >))
            {
                data_ = std::any_cast< std::vector< uint8_t > >(attributes.at("data"));
            }
            else if (attributes.at("data").type() == typeid(std::string))
            {
                setDataFromString(std::any_cast< std::string >(attributes.at("data")));
            }
        }
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Orderbook: ") + e.what());
    }
}

template < typename ROW >
ct::db::Orderbook ct::db::Orderbook::fromRow(const ROW& row)
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

auto ct::db::Orderbook::prepareSelectStatementForConflictCheck(const OrderbooksTable& t,
                                                               sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Orderbook::prepareInsertStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
{
    // Prepare the dynamic insert statement with placeholders
    return sqlpp::dynamic_insert_into(conn, t).dynamic_set(t.id            = getIdAsString(),
                                                           t.timestamp     = timestamp_,
                                                           t.symbol        = symbol_,
                                                           t.exchange_name = enums::toString(exchange_name_),
                                                           t.data          = data_);
}

auto ct::db::Orderbook::prepareUpdateStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const
{
    return sqlpp::dynamic_update(conn, t)
        .dynamic_set(t.timestamp     = timestamp_,
                     t.symbol        = symbol_,
                     t.exchange_name = enums::toString(exchange_name_),
                     t.data          = data_)
        .dynamic_where(t.id == parameter(t.id));
}

template < typename Query, typename Table >
void ct::db::Orderbook::Filter::applyToQuery(Query& query, const Table& t) const
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

ct::db::Ticker::Ticker() : id_(boost::uuids::random_generator()()) {}

ct::db::Ticker::Ticker(const std::unordered_map< std::string, std::any >& attributes) : Ticker()
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
        if (attributes.count("last_price"))
            last_price_ = std::any_cast< double >(attributes.at("last_price"));
        if (attributes.count("volume"))
            volume_ = std::any_cast< double >(attributes.at("volume"));
        if (attributes.count("high_price"))
            high_price_ = std::any_cast< double >(attributes.at("high_price"));
        if (attributes.count("low_price"))
            low_price_ = std::any_cast< double >(attributes.at("low_price"));
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Ticker: ") + e.what());
    }
}

template < typename ROW >
ct::db::Ticker ct::db::Ticker::fromRow(const ROW& row)
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

auto ct::db::Ticker::prepareSelectStatementForConflictCheck(const TickersTable& t,
                                                            sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Ticker::prepareInsertStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const
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

auto ct::db::Ticker::prepareUpdateStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const
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

template < typename Query, typename Table >
void ct::db::Ticker::Filter::applyToQuery(Query& query, const Table& t) const
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

// Default constructor
ct::db::Trade::Trade() : id_(boost::uuids::random_generator()()) {}

// Constructor with attributes
ct::db::Trade::Trade(const std::unordered_map< std::string, std::any >& attributes) : Trade()
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
        if (attributes.count("price"))
            price_ = std::any_cast< double >(attributes.at("price"));
        if (attributes.count("buy_qty"))
            buy_qty_ = std::any_cast< double >(attributes.at("buy_qty"));
        if (attributes.count("sell_qty"))
            sell_qty_ = std::any_cast< double >(attributes.at("sell_qty"));
        if (attributes.count("buy_count"))
            buy_count_ = std::any_cast< int >(attributes.at("buy_count"));
        if (attributes.count("sell_count"))
            sell_count_ = std::any_cast< int >(attributes.at("sell_count"));
        if (attributes.count("symbol"))
            symbol_ = std::any_cast< std::string >(attributes.at("symbol"));
        if (attributes.count("exchange_name"))
            exchange_name_ = std::any_cast< enums::ExchangeName >(attributes.at("exchange_name"));
    }
    catch (const std::bad_any_cast& e)
    {
        throw std::runtime_error(std::string("Error initializing Trade: ") + e.what());
    }
}

template < typename ROW >
ct::db::Trade ct::db::Trade::fromRow(const ROW& row)
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

auto ct::db::Trade::prepareSelectStatementForConflictCheck(const TradesTable& t,
                                                           sqlpp::postgresql::connection& conn) const
{
    auto query  = dynamic_select(conn, all_of(t)).from(t).dynamic_where();
    auto filter = Filter().withExchangeName(exchange_name_).withSymbol(symbol_).withTimestamp(timestamp_);

    filter.applyToQuery(query, t);

    return query;
}

auto ct::db::Trade::prepareInsertStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const
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

auto ct::db::Trade::prepareUpdateStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const
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

template < typename Query, typename Table >
void ct::db::Trade::Filter::applyToQuery(Query& query, const Table& t) const
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

template std::optional< ct::db::Candle > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                          const boost::uuids::uuid& id);

template std::optional< ct::db::ClosedTrade > ct::db::findById(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const boost::uuids::uuid& id);

template std::optional< ct::db::DailyBalance > ct::db::findById(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const boost::uuids::uuid& id);

template std::optional< ct::db::ExchangeApiKeys > ct::db::findById(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const boost::uuids::uuid& id);

template std::optional< ct::db::Log > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                       const boost::uuids::uuid& id);

template std::optional< ct::db::NotificationApiKeys > ct::db::findById(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const boost::uuids::uuid& id);

template std::optional< ct::db::Option > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                          const boost::uuids::uuid& id);

template std::optional< ct::db::Orderbook > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                             const boost::uuids::uuid& id);

template std::optional< ct::db::Ticker > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                          const boost::uuids::uuid& id);

template std::optional< ct::db::Trade > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                         const boost::uuids::uuid& id);

template std::optional< ct::db::Order > ct::db::findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                         const boost::uuids::uuid& id);

template std::optional< std::vector< ct::db::Candle > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Candle::Filter& filter);

template std::optional< std::vector< ct::db::ClosedTrade > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::ClosedTrade::Filter& filter);

template std::optional< std::vector< ct::db::DailyBalance > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::DailyBalance::Filter& filter);

template std::optional< std::vector< ct::db::ExchangeApiKeys > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::ExchangeApiKeys::Filter& filter);

template std::optional< std::vector< ct::db::Log > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Log::Filter& filter);

template std::optional< std::vector< ct::db::NotificationApiKeys > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::NotificationApiKeys::Filter& filter);

template std::optional< std::vector< ct::db::Option > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Option::Filter& filter);

template std::optional< std::vector< ct::db::Orderbook > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Orderbook::Filter& filter);

template std::optional< std::vector< ct::db::Ticker > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Ticker::Filter& filter);

template std::optional< std::vector< ct::db::Trade > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Trade::Filter& filter);

template std::optional< std::vector< ct::db::Order > > ct::db::findByFilter(
    std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const ct::db::Order::Filter& filter);

template void ct::db::save(ct::db::Candle& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::ClosedTrade& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::DailyBalance& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::ExchangeApiKeys& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Log& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::NotificationApiKeys& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Option& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Orderbook& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Ticker& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Trade& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);

template void ct::db::save(ct::db::Order& model,
                           std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                           const bool update_on_conflict);
