#include "DB.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "Config.hpp"
#include "DB.hpp"
#include <blaze/Math.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sqlpp11/null.h>
#include <sqlpp11/postgresql/postgresql.h>
#include <sqlpp11/sqlpp11.h>

// Test fixture for database tests
class DBTest : public ::testing::Test
{
   protected:
    // Access the shared database name
    static std::string& GetDBName()
    {
        static std::string dbName;
        return dbName;
    }

    // Static test suite setup - runs once before all tests
    static void SetUpTestSuite()
    {
        std::cout << "Setting up test suite - creating database..." << std::endl;

        // Connect to PostgreSQL
        std::string host     = "localhost";
        std::string username = "postgres";
        std::string password = "postgres";
        uint16_t port        = 5432;

        // Create a temporary test database with unique name
        std::string tempDbName =
            "cipher_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        GetDBName() = tempDbName;

        // Connect to default database first to create our test DB
        sqlpp::postgresql::connection_config adminConfig;
        // adminConfig.debug    = true;
        adminConfig.host     = host;
        adminConfig.dbname   = "postgres"; // Connect to default DB first
        adminConfig.user     = username;
        adminConfig.password = password;
        adminConfig.port     = port;

        sqlpp::postgresql::connection adminConn(adminConfig);

        // Create test database
        adminConn.execute("CREATE DATABASE " + tempDbName);

        // Initialize our connection pool with the test database
        CipherDB::db::Database::getInstance().init(host, tempDbName, username, password, port);

        // Apply migrations from the migrations directory
        ApplyMigrations("up");
    }

    // Static test suite teardown - runs once after all tests
    static void TearDownTestSuite()
    {
        std::cout << "Tearing down test suite - dropping database..." << std::endl;

        // Apply down migrations to clean up tables
        ApplyMigrations("down");

        // Drop the test database
        sqlpp::postgresql::connection_config adminConfig;
        // adminConfig.debug    = true;
        adminConfig.host     = "localhost";
        adminConfig.dbname   = "postgres"; // Connect to default DB
        adminConfig.user     = "postgres";
        adminConfig.password = "postgres";
        adminConfig.port     = 5432;

        sqlpp::postgresql::connection adminConn(adminConfig);

        // Terminate all connections to our test database
        adminConn.execute("SELECT pg_terminate_backend(pg_stat_activity.pid) "
                          "FROM pg_stat_activity "
                          "WHERE pg_stat_activity.datname = '" +
                          GetDBName() +
                          "' "
                          "AND pid <> pg_backend_pid()");

        // Drop the test database
        adminConn.execute("DROP DATABASE IF EXISTS " + GetDBName());
    }

    // Make the apply migrations function static
    static void ApplyMigrations(const std::string& direction)
    {
        auto& conn = CipherDB::db::Database::getInstance().getConnection();

        // Get project root directory
        std::filesystem::path projectRoot = std::filesystem::current_path();
        // Navigate up until we find the migrations directory or hit the filesystem root
        while (!std::filesystem::exists(projectRoot / "migrations") && projectRoot.has_parent_path())
        {
            projectRoot = projectRoot.parent_path();
        }

        if (!std::filesystem::exists(projectRoot / "migrations"))
        {
            throw std::runtime_error("Migrations directory not found");
        }

        std::filesystem::path migrationsDir = projectRoot / "migrations";
        std::vector< std::filesystem::path > migrationFiles;

        // Collect migration files
        for (const auto& entry : std::filesystem::directory_iterator(migrationsDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".sql")
            {
                // Check if file matches our direction (up or down)
                if (entry.path().filename().string().find("_" + direction + ".sql") != std::string::npos)
                {
                    migrationFiles.push_back(entry.path());
                }
            }
        }

        // Sort migration files based on direction
        if (direction == "up")
        {
            // For "up" migrations: sort in ascending order (001, 002, 003...)
            std::sort(migrationFiles.begin(), migrationFiles.end());
            std::cout << "Applying UP migrations in ascending order" << std::endl;
        }
        else
        {
            // For "down" migrations: sort in descending order (003, 002, 001...)
            std::sort(migrationFiles.begin(), migrationFiles.end(), std::greater<>());
            std::cout << "Applying DOWN migrations in descending order" << std::endl;
        }

        // Apply migrations in order
        for (const auto& migrationFile : migrationFiles)
        {
            std::cout << "Applying migration: " << migrationFile.filename() << std::endl;

            // Read migration file
            std::ifstream file(migrationFile);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open migration file: " + migrationFile.string());
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string sqlContent = buffer.str();

            // Remove comments from SQL (lines starting with --)
            std::string sql;
            std::istringstream iss(sqlContent);
            std::string line;
            while (std::getline(iss, line))
            {
                // Skip comment lines and empty lines
                size_t commentPos = line.find("--");
                if (commentPos != std::string::npos)
                {
                    line = line.substr(0, commentPos);
                }
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                if (!line.empty())
                {
                    sql += line + " ";
                }
            }

            // Split the SQL by semicolons to execute each statement separately
            std::vector< std::string > statements;
            size_t pos     = 0;
            size_t prevPos = 0;

            while ((pos = sql.find(';', prevPos)) != std::string::npos)
            {
                std::string statement = sql.substr(prevPos, pos - prevPos);
                // Trim whitespace
                statement.erase(0, statement.find_first_not_of(" \t\r\n"));
                statement.erase(statement.find_last_not_of(" \t\r\n") + 1);

                if (!statement.empty())
                {
                    statements.push_back(statement);
                }
                prevPos = pos + 1;
            }

            // Check for any remaining SQL after the last semicolon
            std::string finalStatement = sql.substr(prevPos);
            finalStatement.erase(0, finalStatement.find_first_not_of(" \t\r\n"));
            finalStatement.erase(finalStatement.find_last_not_of(" \t\r\n") + 1);
            if (!finalStatement.empty())
            {
                statements.push_back(finalStatement);
            }

            // Execute each statement
            for (const auto& statement : statements)
            {
                if (!statement.empty())
                {
                    try
                    {
                        std::cout << "==================\n";
                        std::cout << "Statement: " << statement << std::endl;
                        conn.execute(statement);
                        std::cout << "==================\n";
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Failed statement: " << statement << std::endl;
                        throw std::runtime_error("Failed to apply migration " + migrationFile.string() + ": " +
                                                 e.what());
                    }
                }
            }
        }
    }

    // Helper method to create a test trade
    CipherDB::ClosedTrade createTestTrade()
    {
        CipherDB::ClosedTrade trade;
        trade.setStrategyName("test_strategy");
        trade.setSymbol("BTC/USD");
        trade.setExchange("binance");
        trade.setType("long"); // Use CipherEnum::LONG in production
        trade.setTimeframe("1h");
        trade.setOpenedAt(1625184000000); // 2021-07-02 00:00:00 UTC
        trade.setClosedAt(1625270400000); // 2021-07-03 00:00:00 UTC (24h later)
        trade.setLeverage(3);

        // Add buy orders
        trade.addBuyOrder(1.0, 35000.0); // 1 BTC at $35,000
        trade.addBuyOrder(0.5, 34500.0); // 0.5 BTC at $34,500

        // Add sell orders
        trade.addSellOrder(0.8, 36000.0); // 0.8 BTC at $36,000
        trade.addSellOrder(0.7, 36500.0); // 0.7 BTC at $36,500

        return trade;
    }

    // Helper method to add a generic order
    void addGenericOrder(
        CipherDB::ClosedTrade& trade, const std::string& side, double qty, double price, int64_t timestamp)
    {
        CipherDB::Order order;
        order.side      = side;
        order.qty       = qty;
        order.price     = price;
        order.timestamp = timestamp;
        trade.addOrder(order);
    }

    // Helper method to create a test daily balance entry
    CipherDB::DailyBalance createTestDailyBalance()
    {
        CipherDB::DailyBalance balance;
        balance.setTimestamp(1625184000000); // 2021-07-02 00:00:00 UTC
        balance.setIdentifier("test_strategy");
        balance.setExchange("binance");
        balance.setAsset("BTC");
        balance.setBalance(1.5);
        return balance;
    }

    CipherDB::ExchangeApiKeys createTestApiKey()
    {
        CipherDB::ExchangeApiKeys apiKey;
        apiKey.setExchangeName("binance");
        apiKey.setName("test_key");
        apiKey.setApiKey("api123456789");
        apiKey.setApiSecret("secret987654321");

        // Set additional fields using JSON
        nlohmann::json additionalFields;
        additionalFields["passphrase"] = "test_passphrase";
        additionalFields["is_testnet"] = false;
        apiKey.setAdditionalFields(additionalFields);

        // Set creation timestamp
        apiKey.setCreatedAt(1625184000000); // 2021-07-02 00:00:00 UTC

        return apiKey;
    }
};

// Test ConnectionPool basics
TEST_F(DBTest, ConnectionPoolBasics)
{
    auto& pool = CipherDB::db::ConnectionPool::getInstance();

    // Test getting a connection
    auto conn = pool.getConnection();
    ASSERT_NE(conn, nullptr);

    // Test that the connection works
    ASSERT_NO_THROW({ conn->execute("SELECT 1"); });

    // Test setting max connections
    pool.setMaxConnections(30);

    // Get another connection
    auto conn2 = pool.getConnection();
    ASSERT_NE(conn2, nullptr);
    ASSERT_NE(conn, conn2); // Should be different connections
}

// Test connection pool edge cases
TEST_F(DBTest, ConnectionPoolEdgeCases)
{
    auto& pool = CipherDB::db::ConnectionPool::getInstance();

    // Set a small max connections
    pool.setMaxConnections(3);

    // Get multiple connections
    auto conn1 = pool.getConnection();
    auto conn2 = pool.getConnection();
    auto conn3 = pool.getConnection();

    // This should not deadlock even with max connections
    ASSERT_NE(conn1, nullptr);
    ASSERT_NE(conn2, nullptr);
    ASSERT_NE(conn3, nullptr);

    // Test returning connections to the pool
    // When these go out of scope, they should be returned to the pool
    conn1.reset();
    conn2.reset();
    conn3.reset();

    // Should be able to get connections again
    auto conn4 = pool.getConnection();
    ASSERT_NE(conn4, nullptr);
}

// Test multithreaded connection pool
TEST_F(DBTest, ConnectionPoolMultithreaded)
{
    auto& pool = CipherDB::db::ConnectionPool::getInstance();
    pool.setMaxConnections(10);

    constexpr int numThreads = 20; // More than max connections to test waiting
    std::atomic< int > successCount{0};

    auto threadFunc = [&]()
    {
        try
        {
            // Get a connection from the pool
            auto conn = pool.getConnection();

            // Do some simple query
            conn->execute("SELECT 1");

            // Sleep to simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            // Connection is returned to the pool when it goes out of scope
            successCount++;
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    };

    // Launch threads
    std::vector< std::future< bool > > futures;
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, threadFunc));
    }

    // Wait for all threads to complete
    for (auto& future : futures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all threads completed successfully
    ASSERT_EQ(successCount, numThreads);
}

TEST_F(DBTest, CandleBasicOperations)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new candle
    CipherDB::Candle candle;
    candle.setTimestamp(1625184000000); // 2021-07-02 00:00:00 UTC
    candle.setOpen(35000.0);
    candle.setClose(35500.0);
    candle.setHigh(36000.0);
    candle.setLow(34800.0);
    candle.setVolume(1000.0);
    candle.setExchange("binance");
    candle.setSymbol("BTC/USD");
    candle.setTimeframe("1h");

    // Save the candle with the transaction's connection
    ASSERT_TRUE(candle.save(conn));

    // Get the ID to find it later
    boost::uuids::uuid id = candle.getId();

    // Find the candle by ID
    auto foundCandle = CipherDB::Candle::findById(conn, id);

    // Verify candle was found
    ASSERT_TRUE(foundCandle.has_value());

    // Verify candle properties
    ASSERT_EQ(foundCandle->getTimestamp(), 1625184000000);
    ASSERT_DOUBLE_EQ(foundCandle->getOpen(), 35000.0);
    ASSERT_DOUBLE_EQ(foundCandle->getClose(), 35500.0);
    ASSERT_DOUBLE_EQ(foundCandle->getHigh(), 36000.0);
    ASSERT_DOUBLE_EQ(foundCandle->getLow(), 34800.0);
    ASSERT_DOUBLE_EQ(foundCandle->getVolume(), 1000.0);
    ASSERT_EQ(foundCandle->getExchange(), "binance");
    ASSERT_EQ(foundCandle->getSymbol(), "BTC/USD");
    ASSERT_EQ(foundCandle->getTimeframe(), "1h");

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test updating an existing candle
TEST_F(DBTest, CandleUpdate)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new candle
    CipherDB::Candle candle;
    candle.setTimestamp(1625184000000);
    candle.setOpen(35000.0);
    candle.setClose(35500.0);
    candle.setHigh(36000.0);
    candle.setLow(34800.0);
    candle.setVolume(1000.0);
    candle.setExchange("binance");
    candle.setSymbol("BTC/USD");
    candle.setTimeframe("1h");

    // Save the candle with the transaction's connection
    ASSERT_TRUE(candle.save(conn));

    // Get the ID
    boost::uuids::uuid id = candle.getId();

    // Update the candle
    candle.setClose(36000.0);
    candle.setHigh(36500.0);
    candle.setVolume(1200.0);

    // Save the updated candle with the same transaction
    ASSERT_TRUE(candle.save(conn));

    // Find the candle by ID
    auto foundCandle = CipherDB::Candle::findById(conn, id);

    // Verify candle was updated
    ASSERT_TRUE(foundCandle.has_value());
    ASSERT_DOUBLE_EQ(foundCandle->getClose(), 36000.0);
    ASSERT_DOUBLE_EQ(foundCandle->getHigh(), 36500.0);
    ASSERT_DOUBLE_EQ(foundCandle->getVolume(), 1200.0);

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test Candle::findByFilter
TEST_F(DBTest, CandleFindByFilter)
{
    // Create a transaction guard for batch operations
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create several candles with different properties
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::Candle candle;
        candle.setTimestamp(1625184000000 + i * 3600000); // 1 hour increments
        candle.setOpen(35000.0 + i * 100);
        candle.setClose(35500.0 + i * 100);
        candle.setHigh(36000.0 + i * 100);
        candle.setLow(34800.0 + i * 100);
        candle.setVolume(1000.0 + i * 10);
        candle.setExchange("CandleFindByFilter:binance");
        candle.setSymbol("BTC/USD");
        candle.setTimeframe("1h");
        ASSERT_TRUE(candle.save(conn));
    }

    // Create candles with different exchange in the same transaction
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::Candle candle;
        candle.setTimestamp(1625184000000 + i * 3600000);
        candle.setOpen(35000.0 + i * 100);
        candle.setClose(35500.0 + i * 100);
        candle.setHigh(36000.0 + i * 100);
        candle.setLow(34800.0 + i * 100);
        candle.setVolume(1000.0 + i * 10);
        candle.setExchange("CandleFindByFilter:kraken");
        candle.setSymbol("BTC/USD");
        candle.setTimeframe("1h");
        ASSERT_TRUE(candle.save(conn));
    }

    // Commit all candles at once
    ASSERT_TRUE(txGuard.commit());

    // Find all binance BTC/USD 1h candles
    auto result = CipherDB::Candle::findByFilter(conn,
                                                 CipherDB::Candle::createFilter()
                                                     .withExchange("CandleFindByFilter:binance")
                                                     .withSymbol("BTC/USD")
                                                     .withTimeframe("1h"));

    // Verify we found the right candles
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Find all kraken BTC/USD 1h candles
    result = CipherDB::Candle::findByFilter(conn,
                                            CipherDB::Candle::createFilter()
                                                .withExchange("CandleFindByFilter:kraken")
                                                .withSymbol("BTC/USD")
                                                .withTimeframe("1h"));

    // Verify we found the right candles
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Find candle with specific timestamp
    result = CipherDB::Candle::findByFilter(conn,
                                            CipherDB::Candle::createFilter()
                                                .withExchange("CandleFindByFilter:binance")
                                                .withSymbol("BTC/USD")
                                                .withTimeframe("1h")
                                                .withTimestamp(1625184000000));

    // Verify we found exactly one candle
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1);
    ASSERT_EQ((*result)[0].getTimestamp(), 1625184000000);

    // Test with non-existent parameters
    result = CipherDB::Candle::findByFilter(conn,
                                            CipherDB::Candle::createFilter()
                                                .withExchange("CandleFindByFilter:unknown")
                                                .withSymbol("BTC/USD")
                                                .withTimeframe("1h"));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

// New test for transaction rollback
TEST_F(DBTest, CandleTransactionRollback)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new candle
    CipherDB::Candle candle;
    candle.setTimestamp(1625184000000);
    candle.setOpen(35000.0);
    candle.setClose(35500.0);
    candle.setHigh(36000.0);
    candle.setLow(34800.0);
    candle.setVolume(1000.0);
    candle.setExchange("binance");
    candle.setSymbol("BTC/USD");
    candle.setTimeframe("1h");

    // Save the candle with the transaction's connection
    ASSERT_TRUE(candle.save(conn));

    // Get the ID before rolling back
    boost::uuids::uuid id = candle.getId();

    // Rollback the transaction instead of committing
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the candle by ID - should not exist after rollback
    auto foundCandle = CipherDB::Candle::findById(conn, id);

    // Verify candle was not found due to rollback
    ASSERT_FALSE(foundCandle.has_value());
}

// New test for multiple operations in a single transaction
TEST_F(DBTest, CandleMultipleOperationsInTransaction)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple candles in the same transaction
    std::vector< boost::uuids::uuid > ids;

    for (int i = 0; i < 5; ++i)
    {
        CipherDB::Candle candle;
        candle.setTimestamp(1625184000000 + i * 3600000);
        candle.setOpen(35000.0 + i * 100);
        candle.setClose(35500.0 + i * 100);
        candle.setHigh(36000.0 + i * 100);
        candle.setLow(34800.0 + i * 100);
        candle.setVolume(1000.0 + i * 10);
        candle.setExchange("CandleMultipleOperationsInTransaction:test_exchange");
        candle.setSymbol("TEST/USD");
        candle.setTimeframe("1h");

        // Save each candle within the same transaction
        ASSERT_TRUE(candle.save(conn));
        ids.push_back(candle.getId());
    }

    // Commit all changes at once
    ASSERT_TRUE(txGuard.commit());

    // Verify all candles were saved
    for (const auto& id : ids)
    {
        auto foundCandle = CipherDB::Candle::findById(conn, id);
        ASSERT_TRUE(foundCandle.has_value());
        ASSERT_EQ(foundCandle->getExchange(), "CandleMultipleOperationsInTransaction:test_exchange");
        ASSERT_EQ(foundCandle->getSymbol(), "TEST/USD");
    }

    // Find all candles with the test exchange
    auto result =
        CipherDB::Candle::findByFilter(nullptr,
                                       CipherDB::Candle::createFilter()
                                           .withExchange("CandleMultipleOperationsInTransaction:test_exchange")
                                           .withSymbol("TEST/USD")
                                           .withTimeframe("1h"));

    // Verify we found all 5 candles
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);
}

// Test edge cases for Candle
TEST_F(DBTest, CandleEdgeCases)
{
    // Test with minimum values
    CipherDB::Candle minCandle;
    minCandle.setTimestamp(0);
    minCandle.setOpen(0.0);
    minCandle.setClose(0.0);
    minCandle.setHigh(0.0);
    minCandle.setLow(0.0);
    minCandle.setVolume(0.0);
    minCandle.setExchange("");
    minCandle.setSymbol("");
    minCandle.setTimeframe("");

    // Save should still work
    ASSERT_TRUE(minCandle.save(nullptr));

    // Test with extreme values
    CipherDB::Candle extremeCandle;
    extremeCandle.setTimestamp(std::numeric_limits< int64_t >::max());
    extremeCandle.setOpen(std::numeric_limits< double >::max());
    extremeCandle.setClose(std::numeric_limits< double >::lowest());
    extremeCandle.setHigh(std::numeric_limits< double >::max());
    extremeCandle.setLow(std::numeric_limits< double >::lowest());
    extremeCandle.setVolume(std::numeric_limits< double >::max());
    // Create a very long string that should still be acceptable for varchar
    std::string longString(1000, 'a');
    extremeCandle.setExchange(longString);
    extremeCandle.setSymbol(longString);
    extremeCandle.setTimeframe(longString);

    // Save should still work
    ASSERT_TRUE(extremeCandle.save(nullptr));

    // Verify extreme candle can be retrieved
    auto foundCandle = CipherDB::Candle::findById(nullptr, extremeCandle.getId());
    ASSERT_TRUE(foundCandle.has_value());
    ASSERT_EQ(foundCandle->getTimestamp(), std::numeric_limits< int64_t >::max());
    ASSERT_DOUBLE_EQ(foundCandle->getOpen(), std::numeric_limits< double >::max());
    ASSERT_DOUBLE_EQ(foundCandle->getClose(), std::numeric_limits< double >::lowest());
}

// Test FindById with non-existent ID
TEST_F(DBTest, CandleFindByIdNonExistent)
{
    // Generate a random UUID that shouldn't exist in the database
    boost::uuids::uuid nonExistentId = boost::uuids::random_generator()();

    // Try to find a candle with this ID
    auto result = CipherDB::Candle::findById(nullptr, nonExistentId);

    // Should return nullopt
    ASSERT_FALSE(result.has_value());
}

// Test multithreaded candle operations
TEST_F(DBTest, CandleMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > candleIds(numThreads);
    std::atomic< int > successCount{0};

    // NOTE: DON'T USE SHARED TX!
    // Create a transaction guard
    // CipherDB::db::TransactionGuard txGuard;
    // auto conn = txGuard.getConnection();

    // Create candles in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create a transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::Candle candle;
            candle.setTimestamp(1625184000000 + index * 3600000);
            candle.setOpen(35000.0 + index * 100);
            candle.setClose(35500.0 + index * 100);
            candle.setHigh(36000.0 + index * 100);
            candle.setLow(34800.0 + index * 100);
            candle.setVolume(1000.0 + index * 10);
            candle.setExchange("CandleMultithreadedOperations:thread_test");
            candle.setSymbol("BTC/USD");
            candle.setTimeframe("1h");

            if (candle.save(conn))
            {
                candleIds[index] = candle.getId();
                successCount++;
                txGuard.commit();

                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads for creating candles
    std::vector< std::future< bool > > createFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        createFutures.push_back(std::async(std::launch::async, createFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : createFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all creations were successful
    ASSERT_EQ(successCount, numThreads);

    // Reset success count for query test
    successCount = 0;

    // Query candles in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::Candle::findById(nullptr, candleIds[index]);
            if (result.has_value())
            {
                // Verify the candle has the correct properties
                if (result->getTimestamp() == 1625184000000 + index * 3600000 &&
                    result->getExchange() == "CandleMultithreadedOperations:thread_test")
                {
                    successCount++;
                }
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads for querying candles
    std::vector< std::future< bool > > queryFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        queryFutures.push_back(std::async(std::launch::async, queryFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : queryFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all queries were successful
    ASSERT_EQ(successCount, numThreads);

    // Test concurrent filter queries
    auto filterFunc = [&]()
    {
        try
        {
            auto filter = CipherDB::Candle::createFilter()
                              .withExchange("CandleMultithreadedOperations:thread_test")
                              .withSymbol("BTC/USD")
                              .withTimeframe("1h");
            auto result = CipherDB::Candle::findByFilter(nullptr, filter);
            return result.has_value() && result->size() == numThreads;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch concurrent filter queries
    std::vector< std::future< bool > > filterFutures;
    for (int i = 0; i < 5; ++i)
    {
        filterFutures.push_back(std::async(std::launch::async, filterFunc));
    }

    // All filter queries should return the correct results
    for (auto& future : filterFutures)
    {
        ASSERT_TRUE(future.get());
    }
}

// Test connection pooling with high concurrency
TEST_F(DBTest, HighConcurrencyConnectionPool)
{
    constexpr int numThreads = 50;
    std::atomic< int > successCount{0};
    std::atomic< int > failureCount{0};

    // Set a relatively small pool size
    CipherDB::db::ConnectionPool::getInstance().setMaxConnections(10);

    auto threadFunc = [&]()
    {
        try
        {
            // Get a connection
            auto conn = CipherDB::db::ConnectionPool::getInstance().getConnection();

            // Simulate some work
            conn->execute("SELECT pg_sleep(0.05)");

            // Increment success counter
            successCount++;
            return true;
        }
        catch (const std::exception& e)
        {
            failureCount++;
            return false;
        }
    };

    // Launch many threads simultaneously
    std::vector< std::future< bool > > futures;
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, threadFunc));
    }

    // Wait for all threads
    for (auto& future : futures)
    {
        future.wait();
    }

    // All operations should succeed eventually, even with a small pool
    ASSERT_EQ(successCount, numThreads);
    ASSERT_EQ(failureCount, 0);
}

// Test basic CRUD operations
TEST_F(DBTest, ClosedTradeBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new trade
    auto trade = createTestTrade();

    // Save the trade
    ASSERT_TRUE(trade.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = trade.getId();

    // Find the trade by ID
    auto foundTrade = CipherDB::ClosedTrade::findById(conn, id);

    // Verify trade was found
    ASSERT_TRUE(foundTrade.has_value());

    // Verify trade properties
    ASSERT_EQ(foundTrade->getStrategyName(), "test_strategy");
    ASSERT_EQ(foundTrade->getSymbol(), "BTC/USD");
    ASSERT_EQ(foundTrade->getExchange(), "binance");
    ASSERT_EQ(foundTrade->getType(), "long");
    ASSERT_EQ(foundTrade->getTimeframe(), "1h");
    ASSERT_EQ(foundTrade->getOpenedAt(), 1625184000000);
    ASSERT_EQ(foundTrade->getClosedAt(), 1625270400000);
    ASSERT_EQ(foundTrade->getLeverage(), 3);

    // Modify the trade
    foundTrade->setLeverage(5);
    foundTrade->setSymbol("ETH/USD");

    // Save the updated trade
    ASSERT_TRUE(foundTrade->save(conn));

    // Retrieve it again
    auto updatedTrade = CipherDB::ClosedTrade::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedTrade.has_value());
    ASSERT_EQ(updatedTrade->getLeverage(), 5);
    ASSERT_EQ(updatedTrade->getSymbol(), "ETH/USD");

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test filtering trades
TEST_F(DBTest, ClosedTradeFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple trades
    std::vector< boost::uuids::uuid > tradeIds;

    // Create 5 trades for binance
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::ClosedTrade trade;
        trade.setStrategyName("ClosedTradeFindByFilter:filter_test");
        trade.setSymbol("BTC/USD");
        trade.setExchange("ClosedTradeFindByFilter:binance_filter_test");
        trade.setType(i % 2 == 0 ? "long" : "short");
        trade.setTimeframe("1h");
        trade.setOpenedAt(1625184000000 + i * 3600000);
        trade.setClosedAt(1625270400000 + i * 3600000);
        trade.setLeverage(3);

        ASSERT_TRUE(trade.save(conn));
        tradeIds.push_back(trade.getId());
    }

    // Create 3 trades for kraken
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::ClosedTrade trade;
        trade.setStrategyName("ClosedTradeFindByFilter:filter_test");
        trade.setSymbol("ETH/USD");
        trade.setExchange("ClosedTradeFindByFilter:kraken_filter_test");
        trade.setType("long");
        trade.setTimeframe("1h");
        trade.setOpenedAt(1625184000000 + i * 3600000);
        trade.setClosedAt(1625270400000 + i * 3600000);
        trade.setLeverage(5);

        ASSERT_TRUE(trade.save(conn));
        tradeIds.push_back(trade.getId());
    }

    // Commit all trades at once
    ASSERT_TRUE(txGuard.commit());

    // Test filtering by exchange
    auto result = CipherDB::ClosedTrade::findByFilter(
        conn, CipherDB::ClosedTrade::createFilter().withExchange("ClosedTradeFindByFilter:binance_filter_test"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Test filtering by exchange and type
    result = CipherDB::ClosedTrade::findByFilter(conn,
                                                 CipherDB::ClosedTrade::createFilter()
                                                     .withExchange("ClosedTradeFindByFilter:binance_filter_test")
                                                     .withType("long"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3); // 3 out of 5 are "long"

    // Test filtering by exchange and symbol
    result = CipherDB::ClosedTrade::findByFilter(conn,
                                                 CipherDB::ClosedTrade::createFilter()
                                                     .withExchange("ClosedTradeFindByFilter:kraken_filter_test")
                                                     .withSymbol("ETH/USD"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Test filtering by strategy name
    result = CipherDB::ClosedTrade::findByFilter(
        conn, CipherDB::ClosedTrade::createFilter().withStrategyName("ClosedTradeFindByFilter:filter_test"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 8); // All trades
}

// Test trade orders and calculated properties
TEST_F(DBTest, ClosedTradeOrdersAndCalculations)
{
    CipherDB::ClosedTrade trade;
    trade.setStrategyName("calculations_test");
    trade.setSymbol("BTC/USD");
    trade.setExchange("binance");
    trade.setType("long");
    trade.setTimeframe("1h");
    trade.setOpenedAt(1625184000000);
    trade.setClosedAt(1625270400000);
    trade.setLeverage(2);

    // Add buy orders with known values for easy calculation
    trade.addBuyOrder(2.0, 10000.0); // 2 BTC at $10,000
    trade.addBuyOrder(3.0, 11000.0); // 3 BTC at $11,000

    // Add sell orders with known values
    trade.addSellOrder(5.0, 12000.0); // 5 BTC at $12,000

    // Test qty calculation
    ASSERT_DOUBLE_EQ(trade.getQty(), 5.0); // 2.0 + 3.0

    // Test entry price calculation
    // (2.0 * 10000.0 + 3.0 * 11000.0) / 5.0 = 10600.0
    ASSERT_DOUBLE_EQ(trade.getEntryPrice(), 10600.0);

    // Test exit price calculation
    ASSERT_DOUBLE_EQ(trade.getExitPrice(), 12000.0);

    // Test size calculation
    ASSERT_DOUBLE_EQ(trade.getSize(), 5.0 * 10600.0);

    // Test holding period
    ASSERT_EQ(trade.getHoldingPeriod(), 86400); // 24 hours in seconds

    // Test type checking
    ASSERT_TRUE(trade.isLong());
    ASSERT_FALSE(trade.isShort());

    // Test ROI calculation - simplified for testing
    double entryValue  = 5.0 * 10600.0;
    double exitValue   = 5.0 * 12000.0;
    double profit      = exitValue - entryValue;
    double expectedRoi = (profit / (entryValue / 2.0)) * 100.0; // Account for leverage

    setenv("ENV_EXCHANGES_BINANCE_FEE", "0", 1);

    ASSERT_NEAR(trade.getRoi(), expectedRoi, 0.01);
    ASSERT_NEAR(trade.getPnlPercentage(), expectedRoi, 0.01);

    // Test JSON conversion
    auto json = trade.toJson();
    ASSERT_EQ(std::any_cast< std::string >(json["strategy_name"]), "calculations_test");
    ASSERT_EQ(std::any_cast< std::string >(json["symbol"]), "BTC/USD");
    ASSERT_DOUBLE_EQ(std::any_cast< double >(json["entry_price"]), 10600.0);
    ASSERT_DOUBLE_EQ(std::any_cast< double >(json["exit_price"]), 12000.0);
    ASSERT_DOUBLE_EQ(std::any_cast< double >(json["qty"]), 5.0);

    CipherConfig::Config::getInstance().reload(true);
    unsetenv("ENV_EXCHANGES_BINANCE_FEE");
}

// Test short trades
TEST_F(DBTest, ClosedTradeShortTrades)
{
    CipherDB::ClosedTrade trade;
    trade.setStrategyName("short_test");
    trade.setSymbol("BTC/USD");
    trade.setExchange("binance");
    trade.setType("short"); // Use CipherEnum::SHORT in production
    trade.setTimeframe("1h");
    trade.setOpenedAt(1625184000000);
    trade.setClosedAt(1625270400000);
    trade.setLeverage(3);

    // For short trades, sell orders are entries
    trade.addSellOrder(2.0, 12000.0); // 2 BTC at $12,000
    trade.addSellOrder(1.0, 11500.0); // 1 BTC at $11,500

    // For short trades, buy orders are exits
    trade.addBuyOrder(3.0, 10000.0); // 3 BTC at $10,000

    // Test type checking
    ASSERT_FALSE(trade.isLong());
    ASSERT_TRUE(trade.isShort());

    // Test qty calculation
    ASSERT_DOUBLE_EQ(trade.getQty(), 3.0); // 2.0 + 1.0

    // Test entry price calculation
    // (2.0 * 12000.0 + 1.0 * 11500.0) / 3.0 = 11833.33...
    ASSERT_NEAR(trade.getEntryPrice(), 11833.33, 0.01);

    // Test exit price calculation
    ASSERT_DOUBLE_EQ(trade.getExitPrice(), 10000.0);

    // Test PNL for short trade (entry - exit) * qty
    // For short trade, profit is made when exit price is lower than entry
    double expectedProfit = (11833.33 - 10000.0) * 3.0;

    setenv("ENV_EXCHANGES_BINANCE_FEE", "0", 1);

    ASSERT_NEAR(trade.getPnl(), expectedProfit, 10.0); // Allow some precision error

    CipherConfig::Config::getInstance().reload(true);
    unsetenv("ENV_EXCHANGES_BINANCE_FEE");
}

// Test transaction safety
TEST_F(DBTest, ClosedTradeTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new trade
    auto trade = createTestTrade();

    // Save the trade
    ASSERT_TRUE(trade.save(conn));

    // Get the ID for later check
    boost::uuids::uuid id = trade.getId();

    // Roll back the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the trade - should not exist after rollback
    auto foundTrade = CipherDB::ClosedTrade::findById(nullptr, id);
    ASSERT_FALSE(foundTrade.has_value());

    // Create another transaction
    CipherDB::db::TransactionGuard txGuard2;
    auto conn2 = txGuard2.getConnection();

    // Save the trade again
    ASSERT_TRUE(trade.save(conn2));

    // Commit this time
    ASSERT_TRUE(txGuard2.commit());

    // Now the trade should exist
    foundTrade = CipherDB::ClosedTrade::findById(nullptr, id);
    ASSERT_TRUE(foundTrade.has_value());
}

// Test concurrent operations
TEST_F(DBTest, ClosedTradeConcurrentOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > tradeIds(numThreads);
    std::atomic< int > successCount{0};

    // Create trades concurrently
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::ClosedTrade trade;
            trade.setStrategyName("concurrent_test_" + std::to_string(index));
            trade.setSymbol("BTC/USD");
            trade.setExchange("concurrent_test");
            trade.setType(index % 2 == 0 ? "long" : "short");
            trade.setTimeframe("1h");
            trade.setOpenedAt(1625184000000 + index * 3600000);
            trade.setClosedAt(1625270400000 + index * 3600000);
            trade.setLeverage(index + 1);

            trade.addBuyOrder(1.0, 35000.0 + index * 100.0);
            trade.addSellOrder(1.0, 36000.0 + index * 100.0);

            if (trade.save(conn))
            {
                tradeIds[index] = trade.getId();
                successCount++;
                txGuard.commit();
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads
    std::vector< std::future< bool > > futures;
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, createFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : futures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all creations were successful
    ASSERT_EQ(successCount, numThreads);

    // Find all trades with the concurrent_test exchange
    auto result = CipherDB::ClosedTrade::findByFilter(
        nullptr, CipherDB::ClosedTrade::createFilter().withExchange("concurrent_test"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), numThreads);
}

// Test edge cases
TEST_F(DBTest, ClosedTradeEdgeCases)
{
    // Edge case 1: Empty trade with no orders
    CipherDB::ClosedTrade emptyTrade;
    emptyTrade.setStrategyName("empty_test");
    emptyTrade.setSymbol("BTC/USD");
    emptyTrade.setExchange("test");
    emptyTrade.setType("long");
    emptyTrade.setTimeframe("1h");
    emptyTrade.setOpenedAt(1625184000000);
    emptyTrade.setClosedAt(1625270400000);
    emptyTrade.setLeverage(1);

    // Save should work without orders
    ASSERT_TRUE(emptyTrade.save());

    // Test calculations with no orders
    ASSERT_DOUBLE_EQ(emptyTrade.getQty(), 0.0);
    ASSERT_TRUE(std::isnan(emptyTrade.getEntryPrice()));
    ASSERT_TRUE(std::isnan(emptyTrade.getExitPrice()));

    // Edge case 2: Extremely large values
    CipherDB::ClosedTrade extremeTrade;
    extremeTrade.setStrategyName("extreme_test");
    extremeTrade.setSymbol("BTC/USD");
    extremeTrade.setExchange("test");
    extremeTrade.setType("long");
    extremeTrade.setTimeframe("1h");
    extremeTrade.setOpenedAt(std::numeric_limits< int64_t >::max() - 1000);
    extremeTrade.setClosedAt(std::numeric_limits< int64_t >::max());
    extremeTrade.setLeverage(std::numeric_limits< int >::max());

    // Add extreme orders
    extremeTrade.addBuyOrder(std::numeric_limits< double >::max() / 1e10, 1e10);
    extremeTrade.addSellOrder(std::numeric_limits< double >::max() / 1e10, 2e10);

    // Save should still work
    ASSERT_TRUE(extremeTrade.save());

    // Edge case 3: Zero leverage
    CipherDB::ClosedTrade zeroLeverageTrade;
    zeroLeverageTrade.setStrategyName("zero_leverage_test");
    zeroLeverageTrade.setSymbol("BTC/USD");
    zeroLeverageTrade.setExchange("test");
    zeroLeverageTrade.setType("long");
    zeroLeverageTrade.setTimeframe("1h");
    zeroLeverageTrade.setOpenedAt(1625184000000);
    zeroLeverageTrade.setClosedAt(1625270400000);
    zeroLeverageTrade.setLeverage(0); // This should probably be validated in a real app

    zeroLeverageTrade.addBuyOrder(1.0, 10000.0);
    zeroLeverageTrade.addSellOrder(1.0, 11000.0);

    // Save should work
    ASSERT_TRUE(zeroLeverageTrade.save());

    // Edge case 4: Long string fields
    CipherDB::ClosedTrade longStringTrade;
    std::string longString(1000, 'a');
    longStringTrade.setStrategyName(longString);
    longStringTrade.setSymbol(longString);
    longStringTrade.setExchange(longString);
    longStringTrade.setType(longString);
    longStringTrade.setTimeframe(longString);
    longStringTrade.setOpenedAt(1625184000000);
    longStringTrade.setClosedAt(1625270400000);
    longStringTrade.setLeverage(1);

    // Save should still work with long strings
    ASSERT_TRUE(longStringTrade.save());
}

// Test basic CRUD operations
TEST_F(DBTest, DailyBalanceBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new daily balance
    auto balance = createTestDailyBalance();

    // Save the daily balance
    ASSERT_TRUE(balance.save(conn));

    // Get the ID to find it later
    boost::uuids::uuid id = balance.getId();

    // Find the balance by ID
    auto foundBalance = CipherDB::DailyBalance::findById(conn, id);

    // Verify balance was found
    ASSERT_TRUE(foundBalance.has_value());

    // Verify balance properties
    ASSERT_EQ(foundBalance->getTimestamp(), 1625184000000);
    ASSERT_EQ(foundBalance->getIdentifier().value(), "test_strategy");
    ASSERT_EQ(foundBalance->getExchange(), "binance");
    ASSERT_EQ(foundBalance->getAsset(), "BTC");
    ASSERT_DOUBLE_EQ(foundBalance->getBalance(), 1.5);

    // Modify the balance
    foundBalance->setBalance(2.0);
    foundBalance->setAsset("ETH");

    // Save the updated balance
    ASSERT_TRUE(foundBalance->save(conn));

    // Retrieve it again
    auto updatedBalance = CipherDB::DailyBalance::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedBalance.has_value());
    ASSERT_DOUBLE_EQ(updatedBalance->getBalance(), 2.0);
    ASSERT_EQ(updatedBalance->getAsset(), "ETH");

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test null identifier handling
TEST_F(DBTest, DailyBalanceNullIdentifier)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new daily balance with null identifier
    auto balance = createTestDailyBalance();
    balance.clearIdentifier();

    // Save the balance
    ASSERT_TRUE(balance.save(conn));

    // Get the ID
    boost::uuids::uuid id = balance.getId();

    // Find the balance by ID
    auto foundBalance = CipherDB::DailyBalance::findById(conn, id);

    // Verify balance was found
    ASSERT_TRUE(foundBalance.has_value());

    // Verify identifier is nullopt
    ASSERT_FALSE(foundBalance->getIdentifier().has_value());

    // Update the identifier
    foundBalance->setIdentifier("new_strategy");
    ASSERT_TRUE(foundBalance->save(conn));

    // Clear it again
    foundBalance->clearIdentifier();
    ASSERT_TRUE(foundBalance->save(conn));

    // Retrieve it again
    auto finalBalance = CipherDB::DailyBalance::findById(conn, id);
    ASSERT_FALSE(finalBalance->getIdentifier().has_value());

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test filtering
TEST_F(DBTest, DailyBalanceFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create several daily balances with different properties
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::DailyBalance balance;
        balance.setTimestamp(1625184000000 + i * 86400000); // Daily increments
        balance.setIdentifier("DailyBalanceFindByFilter:strategy_" + std::to_string(i));
        balance.setExchange("DailyBalanceFindByFilter:binance_filter_test");
        balance.setAsset("DailyBalanceFindByFilter:BTC");
        balance.setBalance(1.5 + i * 0.5);
        ASSERT_TRUE(balance.save(conn));
    }

    // Create daily balances with different exchange
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::DailyBalance balance;
        balance.setTimestamp(1625184000000 + i * 86400000);
        balance.setIdentifier("DailyBalanceFindByFilter:strategy_" + std::to_string(i));
        balance.setExchange("DailyBalanceFindByFilter:kraken_filter_test");
        balance.setAsset("DailyBalanceFindByFilter:ETH");
        balance.setBalance(0.5 + i * 0.2);
        ASSERT_TRUE(balance.save(conn));
    }

    // Commit all balances at once
    ASSERT_TRUE(txGuard.commit());

    // Find all binance balances
    auto result = CipherDB::DailyBalance::findByFilter(
        conn, CipherDB::DailyBalance::createFilter().withExchange("DailyBalanceFindByFilter:binance_filter_test"));

    // Verify we found the right balances
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Find all kraken balances
    result = CipherDB::DailyBalance::findByFilter(
        conn, CipherDB::DailyBalance::createFilter().withExchange("DailyBalanceFindByFilter:kraken_filter_test"));

    // Verify we found the right balances
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Find balance with specific timestamp and exchange
    result = CipherDB::DailyBalance::findByFilter(conn,
                                                  CipherDB::DailyBalance::createFilter()
                                                      .withExchange("DailyBalanceFindByFilter:binance_filter_test")
                                                      .withTimestamp(1625184000000));

    // Verify we found exactly one balance
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1);
    ASSERT_EQ((*result)[0].getTimestamp(), 1625184000000);

    // Find balances with specific asset
    result = CipherDB::DailyBalance::findByFilter(
        conn, CipherDB::DailyBalance::createFilter().withAsset("DailyBalanceFindByFilter:ETH"));

    // Verify we found all ETH balances
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Find balances with specific identifier
    result = CipherDB::DailyBalance::findByFilter(
        conn, CipherDB::DailyBalance::createFilter().withIdentifier("DailyBalanceFindByFilter:strategy_1"));

    // Verify we found balances with the right identifier
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2); // One from binance, one from kraken

    // Test with non-existent parameters
    result = CipherDB::DailyBalance::findByFilter(
        conn, CipherDB::DailyBalance::createFilter().withExchange("DailyBalanceFindByFilter:nonexistent_exchange"));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

// Test transaction rollback
TEST_F(DBTest, DailyBalanceTransactionRollback)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new daily balance
    auto balance = createTestDailyBalance();

    // Save the balance
    ASSERT_TRUE(balance.save(conn));

    // Get the ID
    boost::uuids::uuid id = balance.getId();

    // Rollback the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the balance - should not exist
    auto foundBalance = CipherDB::DailyBalance::findById(conn, id);
    ASSERT_FALSE(foundBalance.has_value());
}

// Test multiple operations in a single transaction
TEST_F(DBTest, DailyBalanceMultipleOperationsInTransaction)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple daily balances in the same transaction
    std::vector< boost::uuids::uuid > ids;

    for (int i = 0; i < 5; ++i)
    {
        CipherDB::DailyBalance balance;
        balance.setTimestamp(1625184000000 + i * 86400000);
        balance.setIdentifier("batch_strategy");
        balance.setExchange("DailyBalanceMultipleOperationsInTransaction:batch_exchange");
        balance.setAsset("BTC");
        balance.setBalance(1.0 + i * 0.1);

        // Save each balance
        ASSERT_TRUE(balance.save(conn));
        ids.push_back(balance.getId());
    }

    // Commit all changes at once
    ASSERT_TRUE(txGuard.commit());

    // Verify all balances were saved
    for (const auto& id : ids)
    {
        auto foundBalance = CipherDB::DailyBalance::findById(conn, id);
        ASSERT_TRUE(foundBalance.has_value());
        ASSERT_EQ(foundBalance->getExchange(), "DailyBalanceMultipleOperationsInTransaction:batch_exchange");
    }

    // Find all balances from the batch exchange
    auto result =
        CipherDB::DailyBalance::findByFilter(conn,
                                             CipherDB::DailyBalance::createFilter().withExchange(
                                                 "DailyBalanceMultipleOperationsInTransaction:batch_exchange"));

    // Verify we found all 5 balances
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);
}

// Test edge cases
TEST_F(DBTest, DailyBalanceEdgeCases)
{
    // Test with minimum values
    CipherDB::DailyBalance minBalance;
    minBalance.setTimestamp(0);
    minBalance.clearIdentifier();
    minBalance.setExchange("");
    minBalance.setAsset("");
    minBalance.setBalance(0.0);

    // Save should still work
    ASSERT_TRUE(minBalance.save(nullptr));

    // Test with extreme values
    CipherDB::DailyBalance extremeBalance;
    extremeBalance.setTimestamp(std::numeric_limits< int64_t >::max());
    std::string longString(1000, 'a');
    extremeBalance.setIdentifier(longString);
    extremeBalance.setExchange(longString);
    extremeBalance.setAsset(longString);
    extremeBalance.setBalance(std::numeric_limits< double >::max());

    // Save should still work
    ASSERT_TRUE(extremeBalance.save(nullptr));

    // Verify extreme balance can be retrieved
    auto foundBalance = CipherDB::DailyBalance::findById(nullptr, extremeBalance.getId());
    ASSERT_TRUE(foundBalance.has_value());
    ASSERT_EQ(foundBalance->getTimestamp(), std::numeric_limits< int64_t >::max());
    ASSERT_EQ(foundBalance->getIdentifier().value(), longString);
    ASSERT_DOUBLE_EQ(foundBalance->getBalance(), std::numeric_limits< double >::max());

    // Test with negative balance
    CipherDB::DailyBalance negativeBalance;
    negativeBalance.setTimestamp(1625184000000);
    negativeBalance.setIdentifier("negative_test");
    negativeBalance.setExchange("test_exchange");
    negativeBalance.setAsset("BTC");
    negativeBalance.setBalance(-1000.0);

    // Save should work with negative balance
    ASSERT_TRUE(negativeBalance.save(nullptr));

    // Verify negative balance is preserved
    auto foundNegativeBalance = CipherDB::DailyBalance::findById(nullptr, negativeBalance.getId());
    ASSERT_TRUE(foundNegativeBalance.has_value());
    ASSERT_DOUBLE_EQ(foundNegativeBalance->getBalance(), -1000.0);
}

// Test non-existent IDs
TEST_F(DBTest, DailyBalanceFindByIdNonExistent)
{
    // Generate a random UUID that shouldn't exist in the database
    boost::uuids::uuid nonExistentId = boost::uuids::random_generator()();

    // Try to find a balance with this ID
    auto result = CipherDB::DailyBalance::findById(nullptr, nonExistentId);

    // Should return nullopt
    ASSERT_FALSE(result.has_value());
}

// Test multithreaded operations
TEST_F(DBTest, DailyBalanceMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > balanceIds(numThreads);
    std::atomic< int > successCount{0};

    // Create balances in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::DailyBalance balance;
            balance.setTimestamp(1625184000000 + index * 86400000);
            balance.setIdentifier("thread_" + std::to_string(index));
            balance.setExchange("DailyBalanceMultithreadedOperations:concurrent_test");
            balance.setAsset("BTC");
            balance.setBalance(1.0 + index * 0.1);

            if (balance.save(conn))
            {
                balanceIds[index] = balance.getId();
                successCount++;
                txGuard.commit();
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads
    std::vector< std::future< bool > > createFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        createFutures.push_back(std::async(std::launch::async, createFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : createFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all creations were successful
    ASSERT_EQ(successCount, numThreads);

    // Reset success count for query test
    successCount = 0;

    // Query balances in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::DailyBalance::findById(nullptr, balanceIds[index]);
            if (result.has_value())
            {
                // Verify the balance has correct properties
                if (result->getIdentifier().value() == "thread_" + std::to_string(index) &&
                    result->getExchange() == "DailyBalanceMultithreadedOperations:concurrent_test")
                {
                    successCount++;
                }
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads for querying
    std::vector< std::future< bool > > queryFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        queryFutures.push_back(std::async(std::launch::async, queryFunc, i));
    }

    // Wait for all threads
    for (auto& future : queryFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all queries were successful
    ASSERT_EQ(successCount, numThreads);

    // Test concurrent filter queries
    auto filterFunc = [&]()
    {
        try
        {
            auto filter = CipherDB::DailyBalance::createFilter().withExchange(
                "DailyBalanceMultithreadedOperations:concurrent_test");
            auto result = CipherDB::DailyBalance::findByFilter(nullptr, filter);
            return result.has_value() && result->size() == numThreads;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch concurrent filter queries
    std::vector< std::future< bool > > filterFutures;
    for (int i = 0; i < 5; ++i)
    {
        filterFutures.push_back(std::async(std::launch::async, filterFunc));
    }

    // All filter queries should return correct results
    for (auto& future : filterFutures)
    {
        ASSERT_TRUE(future.get());
    }
}

// Test attribute construction
TEST_F(DBTest, DailyBalanceAttributeConstruction)
{
    // Create a balance using the attribute map constructor
    std::unordered_map< std::string, std::any > attributes;
    attributes["timestamp"]  = static_cast< int64_t >(1625184000000);
    attributes["identifier"] = std::string("attr_test");
    attributes["exchange"]   = std::string("attr_exchange");
    attributes["asset"]      = std::string("ETH");
    attributes["balance"]    = 3.14159;

    CipherDB::DailyBalance balance(attributes);

    // Verify attributes were correctly assigned
    ASSERT_EQ(balance.getTimestamp(), 1625184000000);
    ASSERT_EQ(balance.getIdentifier().value(), "attr_test");
    ASSERT_EQ(balance.getExchange(), "attr_exchange");
    ASSERT_EQ(balance.getAsset(), "ETH");
    ASSERT_DOUBLE_EQ(balance.getBalance(), 3.14159);

    // Test with UUID in attributes
    boost::uuids::uuid testId = boost::uuids::random_generator()();
    attributes["id"]          = testId;

    CipherDB::DailyBalance balanceWithId(attributes);
    ASSERT_EQ(balanceWithId.getId(), testId);

    // Test with string UUID in attributes
    std::string idStr = boost::uuids::to_string(testId);
    attributes["id"]  = idStr;

    CipherDB::DailyBalance balanceWithStrId(attributes);
    ASSERT_EQ(balanceWithStrId.getId(), testId);

    // Test with missing attributes (should use defaults)
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["exchange"] = std::string("partial_exchange");
    partialAttributes["asset"]    = std::string("BTC");

    CipherDB::DailyBalance partialBalance(partialAttributes);
    ASSERT_EQ(partialBalance.getExchange(), "partial_exchange");
    ASSERT_EQ(partialBalance.getAsset(), "BTC");
    ASSERT_EQ(partialBalance.getTimestamp(), 0);
    ASSERT_DOUBLE_EQ(partialBalance.getBalance(), 0.0);
    ASSERT_FALSE(partialBalance.getIdentifier().has_value());
}

// Test invalid data handling
TEST_F(DBTest, DailyBalanceInvalidData)
{
    // Test constructor with invalid attribute types
    std::unordered_map< std::string, std::any > invalidAttributes;
    invalidAttributes["timestamp"] = std::string("not_a_number"); // Wrong type

    // Should throw an exception
    ASSERT_THROW({ CipherDB::DailyBalance invalidBalance(invalidAttributes); }, std::runtime_error);

    // Test ID setter with invalid UUID string
    CipherDB::DailyBalance balance;
    ASSERT_THROW({ balance.setId("not-a-valid-uuid"); }, std::runtime_error);
}

// Test unique constraints
TEST_F(DBTest, DailyBalanceUniqueConstraints)
{
    // Some database schemas might enforce uniqueness on certain combinations
    // For example, timestamp + exchange + asset might be unique

    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a daily balance
    CipherDB::DailyBalance balance1;
    balance1.setTimestamp(1625184000000);
    balance1.setExchange("DailyBalanceUniqueConstraints:unique_test");
    balance1.setAsset("BTC");
    balance1.setBalance(1.0);

    ASSERT_TRUE(balance1.save(conn));

    // Create another balance with the same timestamp, exchange, and asset
    CipherDB::DailyBalance balance2;
    balance2.setTimestamp(1625184000000);
    balance2.setExchange("DailyBalanceUniqueConstraints:unique_test");
    balance2.setAsset("BTC");
    balance2.setBalance(2.0);

    // This should still work because we're using a new UUID
    ASSERT_TRUE(balance2.save(conn));

    // Verify both were saved
    auto result = CipherDB::DailyBalance::findByFilter(conn,
                                                       CipherDB::DailyBalance::createFilter()
                                                           .withExchange("DailyBalanceUniqueConstraints:unique_test")
                                                           .withAsset("BTC"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2);

    txGuard.commit();
}

// Test basic CRUD operations
TEST_F(DBTest, ExchangeApiKeysBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new API key
    auto apiKey = createTestApiKey();

    // Save the API key
    ASSERT_TRUE(apiKey.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = apiKey.getId();

    // Find the API key by ID
    auto foundApiKey = CipherDB::ExchangeApiKeys::findById(conn, id);

    // Verify API key was found
    ASSERT_TRUE(foundApiKey.has_value());

    // Verify API key properties
    ASSERT_EQ(foundApiKey->getExchangeName(), "binance");
    ASSERT_EQ(foundApiKey->getName(), "test_key");
    ASSERT_EQ(foundApiKey->getApiKey(), "api123456789");
    ASSERT_EQ(foundApiKey->getApiSecret(), "secret987654321");
    ASSERT_EQ(foundApiKey->getCreatedAt(), 1625184000000);

    // Check that JSON was properly stored and retrieved
    auto additionalFields = foundApiKey->getAdditionalFields();
    ASSERT_EQ(additionalFields["passphrase"], "test_passphrase");
    ASSERT_EQ(additionalFields["is_testnet"], false);

    // Modify the API key
    foundApiKey->setApiKey("new_api_key");
    foundApiKey->setApiSecret("new_api_secret");

    // Update additional fields
    nlohmann::json updatedFields = foundApiKey->getAdditionalFields();
    updatedFields["passphrase"]  = "updated_passphrase";
    updatedFields["is_testnet"]  = true;
    foundApiKey->setAdditionalFields(updatedFields);

    // Save the updated API key
    ASSERT_TRUE(foundApiKey->save(conn));

    // Retrieve it again
    auto updatedApiKey = CipherDB::ExchangeApiKeys::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedApiKey.has_value());
    ASSERT_EQ(updatedApiKey->getApiKey(), "new_api_key");
    ASSERT_EQ(updatedApiKey->getApiSecret(), "new_api_secret");

    // Check updated JSON fields
    auto updatedJsonFields = updatedApiKey->getAdditionalFields();
    ASSERT_EQ(updatedJsonFields["passphrase"], "updated_passphrase");
    ASSERT_EQ(updatedJsonFields["is_testnet"], true);

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test filtering API keys
TEST_F(DBTest, ExchangeApiKeysFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple API keys for different exchanges
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::ExchangeApiKeys apiKey;
        apiKey.setExchangeName("ExchangeApiKeysFindByFilter:binance_filter_test");
        apiKey.setName("binance_key_" + std::to_string(i));
        apiKey.setApiKey("api_" + std::to_string(i));
        apiKey.setApiSecret("secret_" + std::to_string(i));
        apiKey.setCreatedAt(1625184000000 + i * 86400000);
        ASSERT_TRUE(apiKey.save(conn));
    }

    // Create API keys for another exchange
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::ExchangeApiKeys apiKey;
        apiKey.setExchangeName("ExchangeApiKeysFindByFilter:coinbase_filter_test");
        apiKey.setName("coinbase_key_" + std::to_string(i));
        apiKey.setApiKey("api_" + std::to_string(i));
        apiKey.setApiSecret("secret_" + std::to_string(i));
        apiKey.setCreatedAt(1625184000000 + i * 86400000);
        ASSERT_TRUE(apiKey.save(conn));
    }

    // Commit all API keys at once
    ASSERT_TRUE(txGuard.commit());

    // Find all Binance API keys
    auto result = CipherDB::ExchangeApiKeys::findByFilter(
        conn,
        CipherDB::ExchangeApiKeys::createFilter().withExchangeName("ExchangeApiKeysFindByFilter:binance_filter_test"));

    // Verify we found the right number of API keys
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Find all Coinbase API keys
    result = CipherDB::ExchangeApiKeys::findByFilter(
        conn,
        CipherDB::ExchangeApiKeys::createFilter().withExchangeName("ExchangeApiKeysFindByFilter:coinbase_filter_test"));

    // Verify we found the right number of API keys
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Test with non-existent exchange
    result = CipherDB::ExchangeApiKeys::findByFilter(conn,
                                                     CipherDB::ExchangeApiKeys::createFilter().withExchangeName(
                                                         "ExchangeApiKeysFindByFilter:non_existent_exchange"));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

// Test transaction safety
TEST_F(DBTest, ExchangeApiKeysTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new API key
    auto apiKey = createTestApiKey();
    apiKey.setName("transaction_test");

    // Save the API key
    ASSERT_TRUE(apiKey.save(conn));

    // Get the ID
    boost::uuids::uuid id = apiKey.getId();

    // Rollback the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the API key - should not exist
    auto foundApiKey = CipherDB::ExchangeApiKeys::findById(conn, id);
    ASSERT_FALSE(foundApiKey.has_value());

    // Create a new transaction
    CipherDB::db::TransactionGuard txGuard2;
    auto conn2 = txGuard2.getConnection();

    // Save the API key again
    ASSERT_TRUE(apiKey.save(conn2));

    // Commit the transaction
    ASSERT_TRUE(txGuard2.commit());

    // Now the API key should exist
    foundApiKey = CipherDB::ExchangeApiKeys::findById(nullptr, id);
    ASSERT_TRUE(foundApiKey.has_value());
    ASSERT_EQ(foundApiKey->getName(), "transaction_test");
}

// Test additional fields JSON handling
TEST_F(DBTest, ExchangeApiKeysAdditionalFieldsJson)
{
    CipherDB::ExchangeApiKeys apiKey;
    apiKey.setExchangeName("json_test");
    apiKey.setName("json_key");
    apiKey.setApiKey("api_key");
    apiKey.setApiSecret("api_secret");

    // Test with empty JSON
    ASSERT_NO_THROW({
        auto fields = apiKey.getAdditionalFields();
        ASSERT_TRUE(fields.is_object());
        ASSERT_TRUE(fields.empty());
    });

    // Test with simple JSON
    nlohmann::json simpleJson = {{"key1", "value1"}, {"key2", 123}};
    apiKey.setAdditionalFields(simpleJson);

    auto retrievedJson = apiKey.getAdditionalFields();
    ASSERT_EQ(retrievedJson["key1"], "value1");
    ASSERT_EQ(retrievedJson["key2"], 123);

    // Test with nested JSON
    nlohmann::json nestedJson = {{"string", "value"},
                                 {"number", 42},
                                 {"boolean", true},
                                 {"null", nullptr},
                                 {"array", {1, 2, 3}},
                                 {"object", {{"nested", "value"}}}};

    apiKey.setAdditionalFields(nestedJson);

    auto retrievedNestedJson = apiKey.getAdditionalFields();
    ASSERT_EQ(retrievedNestedJson["string"], "value");
    ASSERT_EQ(retrievedNestedJson["number"], 42);
    ASSERT_EQ(retrievedNestedJson["boolean"], true);
    ASSERT_EQ(retrievedNestedJson["null"], nullptr);

    auto array = retrievedNestedJson["array"];
    ASSERT_TRUE(array.is_array());
    ASSERT_EQ(array.size(), 3);
    ASSERT_EQ(array[0], 1);
    ASSERT_EQ(array[1], 2);
    ASSERT_EQ(array[2], 3);

    auto object = retrievedNestedJson["object"];
    ASSERT_TRUE(object.is_object());
    ASSERT_EQ(object["nested"], "value");

    // Test directly setting JSON string
    std::string jsonStr = R"({"direct":"string","vals":[4,5,6]})";
    apiKey.setAdditionalFieldsJson(jsonStr);

    auto retrievedDirectJson = apiKey.getAdditionalFields();
    ASSERT_EQ(retrievedDirectJson["direct"], "string");
    ASSERT_EQ(retrievedDirectJson["vals"][0], 4);
    ASSERT_EQ(retrievedDirectJson["vals"][1], 5);
    ASSERT_EQ(retrievedDirectJson["vals"][2], 6);

    // Save and verify persistence
    ASSERT_TRUE(apiKey.save(nullptr));

    auto savedApiKey = CipherDB::ExchangeApiKeys::findById(nullptr, apiKey.getId());
    ASSERT_TRUE(savedApiKey.has_value());

    auto savedJson = savedApiKey->getAdditionalFields();
    ASSERT_EQ(savedJson["direct"], "string");
    ASSERT_EQ(savedJson["vals"][0], 4);
    ASSERT_EQ(savedJson["vals"][1], 5);
    ASSERT_EQ(savedJson["vals"][2], 6);
}

// Test multithreaded operations
TEST_F(DBTest, ExchangeApiKeysMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > apiKeyIds(numThreads);
    std::atomic< int > successCount{0};

    // Create API keys in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::ExchangeApiKeys apiKey;
            apiKey.setExchangeName("multithread_test");
            apiKey.setName("thread_key_" + std::to_string(index));
            apiKey.setApiKey("api_key_" + std::to_string(index));
            apiKey.setApiSecret("secret_" + std::to_string(index));

            nlohmann::json additionalFields;
            additionalFields["thread_id"] = index;
            apiKey.setAdditionalFields(additionalFields);

            if (apiKey.save(conn))
            {
                apiKeyIds[index] = apiKey.getId();
                successCount++;
                txGuard.commit();
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads
    std::vector< std::future< bool > > futures;
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, createFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : futures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all creations were successful
    ASSERT_EQ(successCount, numThreads);

    // Reset success count for query test
    successCount = 0;

    // Query API keys in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto apiKey = CipherDB::ExchangeApiKeys::findById(nullptr, apiKeyIds[index]);
            if (apiKey.has_value())
            {
                auto fields = apiKey->getAdditionalFields();
                if (fields["thread_id"] == index)
                {
                    successCount++;
                }
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch query threads
    std::vector< std::future< bool > > queryFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        queryFutures.push_back(std::async(std::launch::async, queryFunc, i));
    }

    // Wait for all query threads
    for (auto& future : queryFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // All queries should succeed
    ASSERT_EQ(successCount, numThreads);
}

// Test edge cases
TEST_F(DBTest, ExchangeApiKeysEdgeCases)
{
    // Test with minimum values
    CipherDB::ExchangeApiKeys minApiKey;
    minApiKey.setExchangeName("");
    minApiKey.setName("min_values");
    minApiKey.setApiKey("");
    minApiKey.setApiSecret("");
    minApiKey.setAdditionalFieldsJson("{}");
    minApiKey.setCreatedAt(0);

    // Save should still work
    ASSERT_TRUE(minApiKey.save(nullptr));

    // Test with extremely long values
    CipherDB::ExchangeApiKeys longApiKey;
    std::string longString(1000, 'a');
    longApiKey.setExchangeName(longString);
    longApiKey.setName("long_values");
    longApiKey.setApiKey(longString);
    longApiKey.setApiSecret(longString);

    // Create a very large JSON object
    nlohmann::json largeJson;
    for (int i = 0; i < 100; ++i)
    {
        largeJson["key_" + std::to_string(i)] = longString;
    }
    longApiKey.setAdditionalFields(largeJson);

    longApiKey.setCreatedAt(std::numeric_limits< int64_t >::max());

    // Save should still work with large values
    ASSERT_TRUE(longApiKey.save(nullptr));

    // Verify retrieval works
    auto foundLongApiKey = CipherDB::ExchangeApiKeys::findById(nullptr, longApiKey.getId());
    ASSERT_TRUE(foundLongApiKey.has_value());
    ASSERT_EQ(foundLongApiKey->getExchangeName(), longString);
    ASSERT_EQ(foundLongApiKey->getApiKey(), longString);
    ASSERT_EQ(foundLongApiKey->getCreatedAt(), std::numeric_limits< int64_t >::max());

    // Check a random element from the large JSON
    auto largeJsonRetrieved = foundLongApiKey->getAdditionalFields();
    ASSERT_EQ(largeJsonRetrieved["key_42"], longString);

    // Test with potentially problematic JSON characters
    CipherDB::ExchangeApiKeys specialCharsApiKey;
    specialCharsApiKey.setExchangeName("special_chars");
    specialCharsApiKey.setName("special_json");
    specialCharsApiKey.setApiKey("api_key");
    specialCharsApiKey.setApiSecret("secret");

    nlohmann::json specialJson = {{"quotes", "\"quoted text\""},
                                  {"backslashes", "\\path\\to\\file"},
                                  {"newlines", "line1\nline2\r\nline3"},
                                  {"unicode", "ñáéíóú➤☺♠"},
                                  {"html", "<script>alert('XSS')</script>"}};
    specialCharsApiKey.setAdditionalFields(specialJson);

    // Save should work with special characters
    ASSERT_TRUE(specialCharsApiKey.save(nullptr));

    // Verify retrieval preserves all characters
    auto foundSpecialApiKey = CipherDB::ExchangeApiKeys::findById(nullptr, specialCharsApiKey.getId());
    ASSERT_TRUE(foundSpecialApiKey.has_value());

    auto retrievedSpecialJson = foundSpecialApiKey->getAdditionalFields();
    ASSERT_EQ(retrievedSpecialJson["quotes"], "\"quoted text\"");
    ASSERT_EQ(retrievedSpecialJson["backslashes"], "\\path\\to\\file");
    ASSERT_EQ(retrievedSpecialJson["newlines"], "line1\nline2\r\nline3");
    ASSERT_EQ(retrievedSpecialJson["unicode"], "ñáéíóú➤☺♠");
    ASSERT_EQ(retrievedSpecialJson["html"], "<script>alert('XSS')</script>");
}

// Test error handling
TEST_F(DBTest, ExchangeApiKeysErrorHandling)
{
    // Test constructor with invalid attribute types
    std::unordered_map< std::string, std::any > invalidAttributes;
    invalidAttributes["created_at"] = std::string("not_a_number"); // Wrong type

    // Should throw an exception
    ASSERT_THROW({ CipherDB::ExchangeApiKeys invalidApiKey(invalidAttributes); }, std::runtime_error);

    // Test ID setter with invalid UUID string
    CipherDB::ExchangeApiKeys apiKey;
    ASSERT_THROW({ apiKey.setId("not-a-valid-uuid"); }, std::runtime_error);

    // Test with invalid JSON for additional fields
    ASSERT_THROW({ apiKey.setAdditionalFieldsJson("{invalid json}"); }, std::invalid_argument);

    // Test name uniqueness constraint
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // First key with a specific name
    CipherDB::ExchangeApiKeys firstKey;
    firstKey.setExchangeName("test_exchange");
    firstKey.setName("duplicate_name_test");
    firstKey.setApiKey("api_key_1");
    firstKey.setApiSecret("secret_1");
    ASSERT_TRUE(firstKey.save(conn));

    // Second key with the same name but different ID
    CipherDB::ExchangeApiKeys secondKey;
    secondKey.setExchangeName("test_exchange");
    secondKey.setName("duplicate_name_test"); // Same name
    secondKey.setApiKey("api_key_2");
    secondKey.setApiSecret("secret_2");

    // This is expected to fail because the name is unique
    // Note: This behavior depends on database constraints
    // In a real application, you'd handle this gracefully

    // Note: Depending on how your database is set up, this might throw or just return false
    // This test is assuming a constraint error is caught in the save method
    auto saved = secondKey.save(conn);
    ASSERT_FALSE(saved);

    // Skip the assertion only if your database doesn't enforce unique names
    // ASSERT_FALSE(saveResult);

    txGuard.rollback();
}

// Test attribute construction
TEST_F(DBTest, ExchangeApiKeysAttributeConstruction)
{
    // Create an API key using attribute map constructor
    std::unordered_map< std::string, std::any > attributes;
    attributes["exchange_name"]     = std::string("attr_exchange");
    attributes["name"]              = std::string("attr_test_key");
    attributes["api_key"]           = std::string("attr_api_key");
    attributes["api_secret"]        = std::string("attr_api_secret");
    attributes["additional_fields"] = std::string(R"({"source":"attributes"})");
    attributes["created_at"]        = static_cast< int64_t >(1625184000000);

    CipherDB::ExchangeApiKeys apiKey(attributes);

    // Verify attributes were correctly assigned
    ASSERT_EQ(apiKey.getExchangeName(), "attr_exchange");
    ASSERT_EQ(apiKey.getName(), "attr_test_key");
    ASSERT_EQ(apiKey.getApiKey(), "attr_api_key");
    ASSERT_EQ(apiKey.getApiSecret(), "attr_api_secret");
    ASSERT_EQ(apiKey.getCreatedAt(), 1625184000000);

    auto fields = apiKey.getAdditionalFields();
    ASSERT_EQ(fields["source"], "attributes");

    // Test with UUID in attributes
    boost::uuids::uuid testId = boost::uuids::random_generator()();
    attributes["id"]          = testId;

    CipherDB::ExchangeApiKeys idApiKey(attributes);
    ASSERT_EQ(idApiKey.getId(), testId);

    // Test with string UUID
    std::string idStr = boost::uuids::to_string(testId);
    attributes["id"]  = idStr;

    CipherDB::ExchangeApiKeys strIdApiKey(attributes);
    ASSERT_EQ(strIdApiKey.getId(), testId);

    // Test with partial attributes
    std::unordered_map< std::string, std::any > partialAttrs;
    partialAttrs["exchange_name"] = std::string("partial_exchange");
    partialAttrs["name"]          = std::string("partial_key");

    CipherDB::ExchangeApiKeys partialApiKey(partialAttrs);
    ASSERT_EQ(partialApiKey.getExchangeName(), "partial_exchange");
    ASSERT_EQ(partialApiKey.getName(), "partial_key");
    // Default values should be present for unspecified attributes
    ASSERT_EQ(partialApiKey.getApiKey(), "");
    ASSERT_EQ(partialApiKey.getApiSecret(), "");
}

// Test basic CRUD operations for Log
TEST_F(DBTest, LogBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new log entry
    boost::uuids::uuid sessionId = boost::uuids::random_generator()();
    CipherDB::Log log;
    log.setSessionId(sessionId);
    log.setTimestamp(1625184000000); // 2021-07-02 00:00:00 UTC
    log.setMessage("Test log message");
    log.setType(CipherDB::log::LogType::INFO);

    // Save the log
    ASSERT_TRUE(log.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = log.getId();

    // Find the log by ID
    auto foundLog = CipherDB::Log::findById(conn, id);

    // Verify log was found
    ASSERT_TRUE(foundLog.has_value());

    // Verify log properties
    ASSERT_EQ(foundLog->getSessionId(), sessionId);
    ASSERT_EQ(foundLog->getTimestamp(), 1625184000000);
    ASSERT_EQ(foundLog->getMessage(), "Test log message");
    ASSERT_EQ(foundLog->getType(), CipherDB::log::LogType::INFO);

    // Modify the log
    foundLog->setMessage("Updated log message");
    foundLog->setType(CipherDB::log::LogType::ERROR);

    // Save the updated log
    ASSERT_TRUE(foundLog->save(conn));

    // Retrieve it again
    auto updatedLog = CipherDB::Log::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedLog.has_value());
    ASSERT_EQ(updatedLog->getMessage(), "Updated log message");
    ASSERT_EQ(updatedLog->getType(), CipherDB::log::LogType::ERROR);

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test log filtering
TEST_F(DBTest, LogFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple logs with different properties
    boost::uuids::uuid sessionId1 = boost::uuids::random_generator()();
    boost::uuids::uuid sessionId2 = boost::uuids::random_generator()();

    // Create 5 INFO logs for session 1
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::Log log;
        log.setSessionId(sessionId1);
        log.setTimestamp(1625184000000 + i * 3600000);
        log.setMessage("LogFindByFilter:info_log_" + std::to_string(i));
        log.setType(CipherDB::log::LogType::INFO);
        ASSERT_TRUE(log.save(conn));
    }

    // Create 3 ERROR logs for session 2
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::Log log;
        log.setSessionId(sessionId2);
        log.setTimestamp(1625184000000 + i * 3600000);
        log.setMessage("LogFindByFilter:error_log_" + std::to_string(i));
        log.setType(CipherDB::log::LogType::ERROR);
        ASSERT_TRUE(log.save(conn));
    }

    // Commit all logs at once
    ASSERT_TRUE(txGuard.commit());

    // Find all logs for session 1
    auto result = CipherDB::Log::findByFilter(conn, CipherDB::Log::createFilter().withSessionId(sessionId1));

    // Verify we found the right logs
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Find all ERROR logs
    result = CipherDB::Log::findByFilter(conn, CipherDB::Log::createFilter().withType(CipherDB::log::LogType::ERROR));

    // Verify we found the right logs
    ASSERT_TRUE(result.has_value());
    ASSERT_GT(result->size(), 3 - 1);

    // Find logs within a timestamp range
    result = CipherDB::Log::findByFilter(
        conn, CipherDB::Log::createFilter().withSessionId(sessionId1).withTimestampRange(1625184000000, 1625187600000));

    // Verify we found the right logs
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2);

    // Test with non-existent parameters
    result = CipherDB::Log::findByFilter(conn, CipherDB::Log::createFilter().withType(CipherDB::log::LogType::WARNING));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

// Test transaction safety
TEST_F(DBTest, LogTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new log entry
    boost::uuids::uuid sessionId = boost::uuids::random_generator()();
    CipherDB::Log log;
    log.setSessionId(sessionId);
    log.setTimestamp(1625184000000);
    log.setMessage("Transaction safety test");
    log.setType(CipherDB::log::LogType::INFO);

    // Save the log
    ASSERT_TRUE(log.save(conn));

    // Get the ID before rolling back
    boost::uuids::uuid id = log.getId();

    // Rollback the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the log - should not exist after rollback
    auto foundLog = CipherDB::Log::findById(conn, id);
    ASSERT_FALSE(foundLog.has_value());
}

// Test edge cases
TEST_F(DBTest, LogEdgeCases)
{
    // Test with minimum values
    CipherDB::Log minLog;
    minLog.setSessionId(boost::uuids::random_generator()());
    minLog.setTimestamp(0);
    minLog.setMessage("");
    minLog.setType(CipherDB::log::LogType::INFO);

    // Save should still work
    ASSERT_TRUE(minLog.save(nullptr));

    // Test with extreme values
    CipherDB::Log extremeLog;
    extremeLog.setSessionId(boost::uuids::random_generator()());
    extremeLog.setTimestamp(std::numeric_limits< int64_t >::max());

    // Create a very long string
    std::string longString(1000, 'a');
    extremeLog.setMessage(longString);
    extremeLog.setType(CipherDB::log::LogType::ERROR);

    // Save should still work
    ASSERT_TRUE(extremeLog.save(nullptr));

    // Verify extreme log can be retrieved
    auto foundLog = CipherDB::Log::findById(nullptr, extremeLog.getId());
    ASSERT_TRUE(foundLog.has_value());
    ASSERT_EQ(foundLog->getTimestamp(), std::numeric_limits< int64_t >::max());
    ASSERT_EQ(foundLog->getMessage(), longString);
    ASSERT_EQ(foundLog->getType(), CipherDB::log::LogType::ERROR);

    // Test all log types
    std::vector< CipherDB::log::LogType > logTypes = {CipherDB::log::LogType::INFO,
                                                      CipherDB::log::LogType::ERROR,
                                                      CipherDB::log::LogType::WARNING,
                                                      CipherDB::log::LogType::DEBUG};

    for (auto type : logTypes)
    {
        CipherDB::Log typeLog;
        typeLog.setSessionId(boost::uuids::random_generator()());
        typeLog.setTimestamp(1625184000000);
        typeLog.setMessage("Test log for type: " + std::to_string(static_cast< int16_t >(type)));
        typeLog.setType(type);

        // Save and verify
        ASSERT_TRUE(typeLog.save(nullptr));
        auto foundTypeLog = CipherDB::Log::findById(nullptr, typeLog.getId());
        ASSERT_TRUE(foundTypeLog.has_value());
        ASSERT_EQ(foundTypeLog->getType(), type);
    }
}

// Test non-existent IDs
TEST_F(DBTest, LogFindByIdNonExistent)
{
    // Generate a random UUID that shouldn't exist in the database
    boost::uuids::uuid nonExistentId = boost::uuids::random_generator()();

    // Try to find a log with this ID
    auto result = CipherDB::Log::findById(nullptr, nonExistentId);

    // Should return nullopt
    ASSERT_FALSE(result.has_value());
}

// Test multithreaded log operations
TEST_F(DBTest, LogMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > logIds(numThreads);
    std::atomic< int > successCount{0};

    // Create logs in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::Log log;
            log.setSessionId(boost::uuids::random_generator()());
            log.setTimestamp(1625184000000 + index * 3600000);
            log.setMessage("Multithreaded log " + std::to_string(index));
            log.setType(index % 2 == 0 ? CipherDB::log::LogType::INFO : CipherDB::log::LogType::ERROR);

            if (log.save(conn))
            {
                logIds[index] = log.getId();
                successCount++;
                txGuard.commit();
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads
    std::vector< std::future< bool > > createFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        createFutures.push_back(std::async(std::launch::async, createFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : createFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all creations were successful
    ASSERT_EQ(successCount, numThreads);

    // Reset success count for query test
    successCount = 0;

    // Query logs in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::Log::findById(nullptr, logIds[index]);
            if (result.has_value())
            {
                // Verify the log has correct properties
                if (result->getTimestamp() == 1625184000000 + index * 3600000)
                {
                    successCount++;
                }
                return true;
            }
            return false;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch threads for querying logs
    std::vector< std::future< bool > > queryFutures;
    for (int i = 0; i < numThreads; ++i)
    {
        queryFutures.push_back(std::async(std::launch::async, queryFunc, i));
    }

    // Wait for all threads to complete
    for (auto& future : queryFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all queries were successful
    ASSERT_EQ(successCount, numThreads);

    // Test concurrent filter queries
    auto filterFunc = [&]()
    {
        try
        {
            auto filter = CipherDB::Log::createFilter().withType(CipherDB::log::LogType::INFO);
            auto result = CipherDB::Log::findByFilter(nullptr, filter);
            return result.has_value() && result->size() >= numThreads / 2;
        }
        catch (...)
        {
            return false;
        }
    };

    // Launch concurrent filter queries
    std::vector< std::future< bool > > filterFutures;
    for (int i = 0; i < 5; ++i)
    {
        filterFutures.push_back(std::async(std::launch::async, filterFunc));
    }

    // All filter queries should return correct results
    for (auto& future : filterFutures)
    {
        ASSERT_TRUE(future.get());
    }
}

// Test attribute construction
TEST_F(DBTest, LogAttributeConstruction)
{
    // Create a log using the attribute map constructor
    boost::uuids::uuid sessionId = boost::uuids::random_generator()();
    std::unordered_map< std::string, std::any > attributes;
    attributes["session_id"] = sessionId;
    attributes["timestamp"]  = static_cast< int64_t >(1625184000000);
    attributes["message"]    = std::string("Attribute construction test");
    attributes["type"]       = CipherDB::log::LogType::WARNING;

    CipherDB::Log log(attributes);

    // Verify attributes were correctly assigned
    ASSERT_EQ(log.getSessionId(), sessionId);
    ASSERT_EQ(log.getTimestamp(), 1625184000000);
    ASSERT_EQ(log.getMessage(), "Attribute construction test");
    ASSERT_EQ(log.getType(), CipherDB::log::LogType::WARNING);

    // Test with UUID in attributes
    boost::uuids::uuid testId = boost::uuids::random_generator()();
    attributes["id"]          = testId;

    CipherDB::Log logWithId(attributes);
    ASSERT_EQ(logWithId.getId(), testId);

    // Test with string UUID in attributes
    std::string idStr = boost::uuids::to_string(testId);
    attributes["id"]  = idStr;

    CipherDB::Log logWithStrId(attributes);
    ASSERT_EQ(logWithStrId.getId(), testId);

    // Test with partial attributes (should use defaults)
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["message"] = std::string("Partial log");

    CipherDB::Log partialLog(partialAttributes);
    ASSERT_EQ(partialLog.getMessage(), "Partial log");
    ASSERT_EQ(partialLog.getType(), CipherDB::log::LogType::INFO);
    ASSERT_EQ(partialLog.getTimestamp(), 0);
}

// Test invalid data handling
TEST_F(DBTest, LogInvalidDataHandling)
{
    // Test constructor with invalid attribute types
    std::unordered_map< std::string, std::any > invalidAttributes;
    invalidAttributes["timestamp"] = std::string("not_a_number"); // Wrong type

    // Should throw an exception
    ASSERT_THROW({ CipherDB::Log invalidLog(invalidAttributes); }, std::runtime_error);

    // Test ID setter with invalid UUID string
    CipherDB::Log log;
    ASSERT_THROW({ log.setId("not-a-valid-uuid"); }, std::runtime_error);

    // Test setting invalid log type
    CipherDB::Log typeLog;
    ASSERT_NO_THROW({
        typeLog.setType(CipherDB::log::LogType::INFO);
        typeLog.setType(CipherDB::log::LogType::ERROR);
        typeLog.setType(CipherDB::log::LogType::WARNING);
        typeLog.setType(CipherDB::log::LogType::DEBUG);
    });
}
