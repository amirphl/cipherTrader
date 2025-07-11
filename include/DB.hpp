#ifndef CIPHER_DB_HPP
#define CIPHER_DB_HPP

#include "DynamicArray.hpp"
#include "Enum.hpp"
#include "Logger.hpp"
#include "Timeframe.hpp"

namespace ct
{
namespace db
{

enum class OrderBy
{
    ASC,
    DESC
};

class DatabaseShutdownManager
{
   public:
    using ShutdownHook           = std::function< void() >;
    using ShutdownCompletionHook = std::function< void() >;

    static DatabaseShutdownManager& getInstance();

    // Register a hook to be called during shutdown
    void registerShutdownHook(ShutdownHook hook);

    // Register a hook to be called after shutdown is complete
    void registerCompletionHook(ShutdownCompletionHook hook);

    // Initialize signal handlers
    void initSignalHandlers();

    // Check if shutdown is in progress
    bool isShuttingDown() const;

    // Wait for shutdown to complete
    void waitForShutdown();

    void shutdown();

   private:
    DatabaseShutdownManager() : shuttingDown_(false) {}
    ~DatabaseShutdownManager() = default;

    DatabaseShutdownManager(const DatabaseShutdownManager&)            = delete;
    DatabaseShutdownManager& operator=(const DatabaseShutdownManager&) = delete;

    static void handleSignal(int signal);

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
    static ConnectionPool& getInstance();

    // Initialize the connection pool
    void init(const std::string& host,
              const std::string& dbname,
              const std::string& username,
              const std::string& password,
              unsigned int port = 5432,
              size_t poolSize   = 10); // Default pool size

    // Get a connection from the pool (or create one if pool is empty)
    std::shared_ptr< sqlpp::postgresql::connection > getConnection();

    // Get a connection from the pool with health check
    std::shared_ptr< sqlpp::postgresql::connection > getConnectionWithHealthCheck();

    // Set maximum number of connections
    void setMaxConnections(size_t maxConnections);

    void waitForConnectionsToClose();

    // Delete copy constructor and assignment operator
    ConnectionPool(const ConnectionPool&)            = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

   private:
    // Private constructor for singleton
    ConnectionPool();

    // Return a connection to the pool
    void returnConnection(sqlpp::postgresql::connection* conn);

    // Create a new database connection
    void createNewConnection();

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
    static Database& getInstance();

    // Initialize the database with connection parameters
    void init(const std::string& host,
              const std::string& dbname,
              const std::string& username,
              const std::string& password,
              unsigned int port = 5432);

    // Get a connection from the pool
    // sqlpp::postgresql::connection& getConnection();

    std::shared_ptr< sqlpp::postgresql::connection > getConnection();

    void shutdown();

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
    static std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > startTransaction();

    // Commit the transaction
    static bool commitTransaction(std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx);

    // Roll back the transaction
    static bool rollbackTransaction(std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx);
};

// RAII-style transaction guard
class TransactionGuard
{
   public:
    TransactionGuard();

    ~TransactionGuard();

    bool commit();

    bool rollback();

    // Get the connection associated with this transaction
    std::shared_ptr< sqlpp::postgresql::connection > getConnection();

    // Delete copy constructor/assignment
    TransactionGuard(const TransactionGuard&)            = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

   private:
    std::shared_ptr< sqlpp::postgresql::connection > conn_;
    std::shared_ptr< sqlpp::transaction_t< sqlpp::postgresql::connection > > tx_;
    bool committed_;
};

template < typename Func >
auto executeWithRetry(Func&& operation, int maxRetries = 3) -> decltype(operation());

// RAII class for handling connection state
class ConnectionStateGuard
{
   public:
    explicit ConnectionStateGuard(sqlpp::postgresql::connection& conn);

    ~ConnectionStateGuard();

    void markForReset();

   private:
    sqlpp::postgresql::connection& conn_;
    bool needs_reset_;
};

// Generic findById implementation
template < typename ModelType >
std::optional< ModelType > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                    const boost::uuids::uuid& id);

// Generic findByFilter implementation
template < typename ModelType, typename FilterType >
std::optional< std::vector< ModelType > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                       const FilterType& filter);

// Generic save implementation
template < typename ModelType >
void save(ModelType& model, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const bool update_on_conflict);

// TODO: Implement batch save for models.

template < typename ModelType >
void batchSave(const std::vector< ModelType >& models, std::shared_ptr< sqlpp::postgresql::connection > conn_ptr);

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
          std::optional< enums::OrderSubmittedVia > submitted_via);

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

    std::optional< enums::OrderSubmittedVia > getSubmittedVia() const { return submitted_via_; }
    void setSubmittedVia(enums::OrderSubmittedVia via) { submitted_via_ = via; }

    // Status checking methods
    bool isActive() const { return status_ == enums::OrderStatus::ACTIVE; }
    /*
     * Used in live mode only: it means the strategy has considered the order as submitted,
     * but the exchange does not accept it because of the distance between the current
     * price and price of the order. Hence it's been queued for later submission.
     */
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
    double getValue() const;

    double getRemainingQty() const;

    // Order state transitions
    void queueIt();

    void resubmit();

    void cancel(bool silent = false, const std::string& source = "");

    void execute(bool silent = false);

    void executePartially(bool silent = false);

    // Notification methods
    void notifySubmission() const;

    /**
     * Creates a fake order with optional custom attributes for testing purposes.
     *
     * @param attributes Optional map of attributes to override defaults
     * @return Order A fake order instance
     */
    static Order generateFakeOrder(const std::unordered_map< std::string, std::any >& attributes = {});

    // Database operations
    static inline auto table() { return OrdersTable{}; }
    static inline std::string modelName() { return "Order"; }

    template < typename ROW, typename Filter >
    static Order fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const OrdersTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareBatchInsertStatement(const std::vector< Order >& models,
                                     const OrdersTable& t,
                                     sqlpp::postgresql::connection& conn);

    auto prepareUpdateStatement(const OrdersTable& t, sqlpp::postgresql::connection& conn) const;

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Order > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                           const boost::uuids::uuid& id)
    {
        return db::findById< Order >(conn_ptr, id);
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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

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
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

    static std::optional< std::vector< Order > > findByFilter(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                                              const Filter& filter)
    {
        return db::findByFilter< Order, Filter >(conn_ptr, filter);
    }

    // Return a dictionary representation of the order
    nlohmann::json toJson() const;

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
    std::optional< enums::OrderSubmittedVia > submitted_via_;

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
       << ", vars: " << order.vars_.dump()
       << ", submitted_via: " << (order.submitted_via_ ? enums::toString(*order.submitted_via_) : "null") << " }";
    return os;
}

// Define the Candle table structure for sqlpp11
namespace candle
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
} // namespace candle

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

    Candle(int64_t timestamp,
           double open,
           double close,
           double high,
           double low,
           double volume,
           enums::ExchangeName exchange_name,
           std::string symbol,
           timeframe::Timeframe timeframe);

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

    const timeframe::Timeframe& getTimeframe() const { return timeframe_; }
    void setTimeframe(const timeframe::Timeframe& timeframe) { timeframe_ = timeframe; }

    static inline auto table() { return CandlesTable{}; }
    static inline std::string modelName() { return "Candle"; }

    template < typename ROW, typename Filter >
    static Candle fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const CandlesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const CandlesTable& t, sqlpp::postgresql::connection& conn) const;

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

    // Query builder for flexible filtering
    class Filter
    {
       public:
        Filter& withId(const boost::uuids::uuid& id)
        {
            id_ = id;
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

        Filter& withTimeframe(timeframe::Timeframe timeframe)
        {
            timeframe_ = std::move(timeframe);
            return *this;
        }

        Filter& withTimeframeOrNull(timeframe::Timeframe timeframe)
        {
            timeframe_or_null_ = std::move(timeframe);
            return *this;
        }

        Filter& withTimestamp(int64_t timestamp)
        {
            timestamp_ = timestamp;
            return *this;
        }

        Filter& withTimestampRange(int64_t start, int64_t end)
        {
            timestamp_start_ = start;
            timestamp_end_   = end;
            return *this;
        }

        Filter& withOrderBy(const std::string& column, OrderBy direction)
        {
            order_by_ = std::make_pair(column, direction);
            return *this;
        }

        Filter& withLimit(uint64_t limit)
        {
            limit_ = limit;
            return *this;
        }

        Filter& withOffset(uint64_t offset)
        {
            offset_ = offset;
            return *this;
        }

        Filter& withGroupByExchangeNameAndSymbol()
        {
            group_by_exchange_name_and_symbol_ = true;
            return *this;
        }

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        std::optional< double > open_;
        std::optional< double > close_;
        std::optional< double > high_;
        std::optional< double > low_;
        std::optional< double > volume_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< std::string > symbol_;
        std::optional< timeframe::Timeframe > timeframe_;
        std::optional< timeframe::Timeframe > timeframe_or_null_;
        std::optional< std::pair< std::string, OrderBy > > order_by_;
        std::optional< uint64_t > limit_;
        std::optional< uint64_t > offset_;
        bool group_by_exchange_name_and_symbol_ = false;
        bool distinct_                          = false;
        std::optional< std::vector< std::string > > columns_;
    };

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
    timeframe::Timeframe timeframe_;

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
extern void saveCandles(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                        const enums::ExchangeName& exchange_name,
                        const std::string& symbol,
                        const timeframe::Timeframe& timeframe,
                        const blaze::DynamicMatrix< double >& candles);

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

// TODO: Store closedTrades as shared pointers?
// A trade is made when a position is opened AND closed.
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
                timeframe::Timeframe timeframe,
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
        , buy_orders_(datastructure::DynamicBlazeArray< double >({10, 2}, std::nullopt))
        , sell_orders_(datastructure::DynamicBlazeArray< double >({10, 2}, std::nullopt))
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

    const timeframe::Timeframe& getTimeframe() const { return timeframe_; }
    void setTimeframe(const timeframe::Timeframe& timeframe) { timeframe_ = timeframe; }

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
    // Alias for getPnlPercentage
    double getRoi() const;
    // How much we paid to open this position (currently does not include fees, should we?!)
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

    template < typename ROW, typename Filter >
    static ClosedTrade fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const ClosedTradesTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withTimeframe(timeframe::Timeframe timeframe)
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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > strategy_name_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< enums::PositionType > position_type_;
        std::optional< timeframe::Timeframe > timeframe_;
        std::optional< int64_t > opened_at_;
        std::optional< int64_t > closed_at_;
        std::optional< int > leverage_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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
    timeframe::Timeframe timeframe_;
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

    static inline auto table() { return DailyBalanceTable{}; }
    static inline std::string modelName() { return "DailyBalances"; }

    template < typename ROW, typename Filter >
    static DailyBalance fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const DailyBalanceTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > identifier_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< std::string > asset_;
        std::optional< double > balance_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    nlohmann::json getAdditionalFields() const
    {
        return additional_fields_.empty() ? nlohmann::json::object() : nlohmann::json::parse(additional_fields_);
    }

    void setAdditionalFields(const nlohmann::json& fields) { additional_fields_ = fields.dump(); }

    static inline auto table() { return ExchangeApiKeysTable{}; }
    static inline std::string modelName() { return "ExchangeApiKeys"; }

    template < typename ROW, typename Filter >
    static ExchangeApiKeys fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const ExchangeApiKeysTable& t,
                                                sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const ExchangeApiKeysTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const ExchangeApiKeysTable& t, sqlpp::postgresql::connection& conn) const;

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< std::string > name_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    template < typename ROW, typename Filter >
    static Log fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const LogTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const LogTable& t, sqlpp::postgresql::connection& conn) const;

    void save(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr = nullptr, bool update_on_conflict = false)
    {
        db::save(*this, conn_ptr, update_on_conflict);
    }

    static std::optional< Log > findById(std::shared_ptr< sqlpp::postgresql::connection > conn_ptr,
                                         const boost::uuids::uuid& id)
    {
        return db::findById< Log >(conn_ptr, id);
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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< boost::uuids::uuid > session_id_;
        std::optional< log::LogLevel > level_;
        std::optional< int64_t > start_timestamp_;
        std::optional< int64_t > end_timestamp_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    static inline auto table() { return NotificationApiKeysTable{}; }
    static inline std::string modelName() { return "NotificationApiKeys"; }

    template < typename ROW, typename Filter >
    static NotificationApiKeys fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const NotificationApiKeysTable& t,
                                                sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const NotificationApiKeysTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const NotificationApiKeysTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > name_;
        std::optional< std::string > driver_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    static inline auto table() { return OptionsTable{}; }
    static inline std::string modelName() { return "Option"; }

    template < typename ROW, typename Filter >
    static Option fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const OptionsTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const OptionsTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< std::string > option_type_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    static inline auto table() { return OrderbooksTable{}; }
    static inline std::string modelName() { return "Orderbook"; }

    template < typename ROW, typename Filter >
    static Orderbook fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const OrderbooksTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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

    static inline auto table() { return TickersTable{}; }
    static inline std::string modelName() { return "Ticker"; }

    template < typename ROW, typename Filter >
    static Ticker fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const TickersTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const TickersTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        std::optional< double > last_price_min_;
        std::optional< double > last_price_max_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

    static std::optional< std::vector< Ticker > > findByFilter(
        std::shared_ptr< sqlpp::postgresql::connection > conn_ptr, const Filter& filter)
    {
        return db::findByFilter< Ticker, Filter >(conn_ptr, filter);
    }

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
    auto tickerId     = boost::uuids::to_string(boost::uuids::random_generator()());
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

    static inline auto table() { return TradesTable{}; }
    static inline std::string modelName() { return "Trade"; }

    template < typename ROW, typename Filter >
    static Trade fromRow(const ROW& row, const Filter& filter);

    auto prepareSelectStatementForConflictCheck(const TradesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareInsertStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const;

    auto prepareUpdateStatement(const TradesTable& t, sqlpp::postgresql::connection& conn) const;

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

        Filter& withDistinct()
        {
            distinct_ = true;
            return *this;
        }

        Filter& withColumns(const std::vector< std::string >& columns)
        {
            columns_ = columns;
            return *this;
        }

        template < typename Query, typename Table >
        void applyToQuery(Query& query, const Table& t) const;

        template < typename Query, typename Table >
        void applyToColumns(Query& query, const Table& t) const;

        bool isDistinct() const { return distinct_; }

        /**
         * @brief Get the columns that were selected for this filter
         * @return Optional vector of column names
         */
        const std::optional< std::vector< std::string > >& getColumns() const { return columns_; }

       private:
        std::optional< boost::uuids::uuid > id_;
        std::optional< int64_t > timestamp_;
        std::optional< std::string > symbol_;
        std::optional< enums::ExchangeName > exchange_name_;
        std::optional< int64_t > timestamp_start_;
        std::optional< int64_t > timestamp_end_;
        std::optional< double > price_min_;
        std::optional< double > price_max_;
        bool distinct_ = false;
        std::optional< std::vector< std::string > > columns_;
    };

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
