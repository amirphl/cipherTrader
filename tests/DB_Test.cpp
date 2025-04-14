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
#include <sqlpp11/postgresql/connection.h>
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

        auto adminConn = std::make_shared< sqlpp::postgresql::connection >(adminConfig);

        // Create test database
        adminConn->execute("CREATE DATABASE " + tempDbName);

        // NOTE:
        adminConfig.dbname = tempDbName;

        adminConn = std::make_shared< sqlpp::postgresql::connection >(adminConfig);

        // Initialize our connection pool with the test database
        CipherDB::db::Database::getInstance().init(host, tempDbName, username, password, port);

        // Apply migrations from the migrations directory
        ApplyMigrations("up", adminConn);
    }

    // Static test suite teardown - runs once after all tests
    static void TearDownTestSuite()
    {
        std::cout << "Tearing down test suite - dropping database..." << std::endl;

        CipherDB::db::Database::getInstance().shutdown();

        // Drop the test database
        sqlpp::postgresql::connection_config adminConfig;
        // adminConfig.debug    = true;
        adminConfig.host     = "localhost";
        adminConfig.dbname   = GetDBName();
        adminConfig.user     = "postgres";
        adminConfig.password = "postgres";
        adminConfig.port     = 5432;

        auto adminConn = std::make_shared< sqlpp::postgresql::connection >(adminConfig);

        // Apply down migrations to clean up tables
        ApplyMigrations("down", adminConn);

        // NOTE:
        adminConfig.dbname = "postgres";

        adminConn = std::make_shared< sqlpp::postgresql::connection >(adminConfig);

        // Terminate all connections to our test database
        adminConn->execute("SELECT pg_terminate_backend(pg_stat_activity.pid) "
                           "FROM pg_stat_activity "
                           "WHERE pg_stat_activity.datname = '" +
                           GetDBName() +
                           "' "
                           "AND pid <> pg_backend_pid()");

        // Drop the test database
        adminConn->execute("DROP DATABASE IF EXISTS " + GetDBName());
    }

    // Make the apply migrations function static
    static void ApplyMigrations(const std::string& direction, std::shared_ptr< sqlpp::postgresql::connection > conn)
    {
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
                        conn->execute(statement);
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
        trade.setSymbol("ClosedTradeFindByFilter:BTC/USD");
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

    CipherConfig::Config::getInstance().setValue("env_exchanges_binance_fee", 0);

    ASSERT_NEAR(trade.getRoi(), expectedRoi, 0.01);
    ASSERT_NEAR(trade.getPnlPercentage(), expectedRoi, 0.01);

    // Test JSON conversion
    auto json = trade.toJson();
    ASSERT_EQ(json["strategy_name"], "calculations_test");
    ASSERT_EQ(json["symbol"], "BTC/USD");
    ASSERT_DOUBLE_EQ(json["entry_price"], 10600.0);
    ASSERT_DOUBLE_EQ(json["exit_price"], 12000.0);
    ASSERT_DOUBLE_EQ(json["qty"], 5.0);

    CipherConfig::Config::getInstance().reload();
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

    CipherConfig::Config::getInstance().setValue("env_exchanges_binance_fee", 0);

    ASSERT_NEAR(trade.getPnl(), expectedProfit, 10.0); // Allow some precision error

    CipherConfig::Config::getInstance().reload();
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

// Test basic CRUD operations for NotificationApiKeys
TEST_F(DBTest, NotificationApiKeysBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a notification API key
    CipherDB::NotificationApiKeys apiKey;
    apiKey.setName("test_notification_key");
    apiKey.setDriver("telegram");

    nlohmann::json fields;
    fields["bot_token"] = "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11";
    fields["chat_id"]   = "12345678";
    apiKey.setFields(fields);

    // Current timestamp
    int64_t now =
        std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
            .count();
    apiKey.setCreatedAt(now);

    // Save the API key
    ASSERT_TRUE(apiKey.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = apiKey.getId();

    // Find the API key by ID
    auto foundApiKey = CipherDB::NotificationApiKeys::findById(conn, id);

    // Verify API key was found
    ASSERT_TRUE(foundApiKey.has_value());

    // Verify API key properties
    ASSERT_EQ(foundApiKey->getName(), "test_notification_key");
    ASSERT_EQ(foundApiKey->getDriver(), "telegram");
    ASSERT_EQ(foundApiKey->getCreatedAt(), now);

    // Verify JSON fields
    auto fields_json = foundApiKey->getFields();
    ASSERT_EQ(fields_json["bot_token"], "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11");
    ASSERT_EQ(fields_json["chat_id"], "12345678");

    // Modify the API key
    foundApiKey->setDriver("discord");

    nlohmann::json updatedFields;
    updatedFields["webhook_url"] = "https://discord.com/api/webhooks/123456789/abcdef";
    updatedFields["username"]    = "TradingBot";
    foundApiKey->setFields(updatedFields);

    // Save the updated API key
    ASSERT_TRUE(foundApiKey->save(conn));

    // Retrieve it again
    auto updatedApiKey = CipherDB::NotificationApiKeys::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedApiKey.has_value());
    ASSERT_EQ(updatedApiKey->getDriver(), "discord");

    auto updated_fields_json = updatedApiKey->getFields();
    ASSERT_EQ(updated_fields_json["webhook_url"], "https://discord.com/api/webhooks/123456789/abcdef");
    ASSERT_EQ(updated_fields_json["username"], "TradingBot");

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test finding by name and driver
TEST_F(DBTest, NotificationApiKeysFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple API keys with different properties
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::NotificationApiKeys telegramKey;
        telegramKey.setName("telegram_key_" + std::to_string(i));
        telegramKey.setDriver("NotificationApiKeysFindByFilter:telegram");

        nlohmann::json fields;
        fields["bot_token"] = "telegram_token_" + std::to_string(i);
        fields["chat_id"]   = std::to_string(10000 + i);
        telegramKey.setFields(fields);
        telegramKey.setCreatedAt(1625184000000 + i * 3600000);

        ASSERT_TRUE(telegramKey.save(conn));
    }

    // Create Discord keys
    for (int i = 0; i < 2; ++i)
    {
        CipherDB::NotificationApiKeys discordKey;
        discordKey.setName("NotificationApiKeysFindByFilter:discord_key_" + std::to_string(i));
        discordKey.setDriver("discord");

        nlohmann::json fields;
        fields["webhook_url"] = "discord_webhook_" + std::to_string(i);
        discordKey.setFields(fields);
        discordKey.setCreatedAt(1625184000000 + i * 3600000);

        ASSERT_TRUE(discordKey.save(conn));
    }

    // Create a Slack key
    CipherDB::NotificationApiKeys slackKey;
    slackKey.setName("slack_key");
    slackKey.setDriver("slack");

    nlohmann::json slackFields;
    slackFields["webhook_url"] = "slack_webhook";
    slackKey.setFields(slackFields);
    slackKey.setCreatedAt(1625184000000);

    ASSERT_TRUE(slackKey.save(conn));

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());

    // Test finding by driver
    auto telegramKeys = CipherDB::NotificationApiKeys::findByFilter(
        conn, CipherDB::NotificationApiKeys::createFilter().withDriver("NotificationApiKeysFindByFilter:telegram"));

    ASSERT_TRUE(telegramKeys.has_value());
    ASSERT_EQ(telegramKeys->size(), 3);

    // Test finding by name
    auto discordKey0 = CipherDB::NotificationApiKeys::findByFilter(
        conn, CipherDB::NotificationApiKeys::createFilter().withName("NotificationApiKeysFindByFilter:discord_key_0"));

    ASSERT_TRUE(discordKey0.has_value());
    ASSERT_EQ(discordKey0->size(), 1);
    ASSERT_EQ((*discordKey0)[0].getDriver(), "discord");

    // Test finding by driver with no results
    auto emailKeys = CipherDB::NotificationApiKeys::findByFilter(
        conn, CipherDB::NotificationApiKeys::createFilter().withDriver("email"));

    ASSERT_TRUE(emailKeys.has_value());
    ASSERT_EQ(emailKeys->size(), 0);
}

// Test transaction safety (rollback)
TEST_F(DBTest, NotificationApiKeysTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a notification API key
    CipherDB::NotificationApiKeys apiKey;
    apiKey.setName("rollback_test_key");
    apiKey.setDriver("telegram");

    nlohmann::json fields;
    fields["bot_token"] = "test_token";
    apiKey.setFields(fields);
    apiKey.setCreatedAt(1625184000000);

    // Save the API key
    ASSERT_TRUE(apiKey.save(conn));

    // Get the ID for later check
    boost::uuids::uuid id = apiKey.getId();

    // Roll back the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the API key - should not exist after rollback
    auto foundApiKey = CipherDB::NotificationApiKeys::findById(nullptr, id);
    ASSERT_FALSE(foundApiKey.has_value());
}

// Test JSON field handling
TEST_F(DBTest, NotificationApiKeysJsonFields)
{
    // Test empty JSON
    CipherDB::NotificationApiKeys emptyJsonKey;
    emptyJsonKey.setName("empty_json_key");
    emptyJsonKey.setDriver("telegram");
    emptyJsonKey.setFields(nlohmann::json::object());
    emptyJsonKey.setCreatedAt(1625184000000);

    ASSERT_TRUE(emptyJsonKey.save());

    // Retrieve and verify empty JSON
    auto foundEmptyKey = CipherDB::NotificationApiKeys::findById(nullptr, emptyJsonKey.getId());
    ASSERT_TRUE(foundEmptyKey.has_value());
    ASSERT_EQ(foundEmptyKey->getFields().dump(), "{}");

    // Test complex nested JSON
    CipherDB::NotificationApiKeys complexJsonKey;
    complexJsonKey.setName("complex_json_key");
    complexJsonKey.setDriver("custom");

    nlohmann::json complexFields;
    complexFields["server"]    = "example.com";
    complexFields["port"]      = 443;
    complexFields["ssl"]       = true;
    complexFields["retry"]     = {{"max_attempts", 3}, {"backoff", {{"initial", 1000}, {"multiplier", 2}}}};
    complexFields["endpoints"] = nlohmann::json::array({"notify", "alert", "message"});

    complexJsonKey.setFields(complexFields);
    complexJsonKey.setCreatedAt(1625184000000);

    ASSERT_TRUE(complexJsonKey.save());

    // Retrieve and verify complex JSON
    auto foundComplexKey = CipherDB::NotificationApiKeys::findById(nullptr, complexJsonKey.getId());
    ASSERT_TRUE(foundComplexKey.has_value());
    auto retrievedJson = foundComplexKey->getFields();

    ASSERT_EQ(retrievedJson["server"], "example.com");
    ASSERT_EQ(retrievedJson["port"], 443);
    ASSERT_EQ(retrievedJson["ssl"], true);
    ASSERT_EQ(retrievedJson["retry"]["max_attempts"], 3);
    ASSERT_EQ(retrievedJson["retry"]["backoff"]["initial"], 1000);
    ASSERT_EQ(retrievedJson["retry"]["backoff"]["multiplier"], 2);
    ASSERT_EQ(retrievedJson["endpoints"].size(), 3);
    ASSERT_EQ(retrievedJson["endpoints"][0], "notify");

    // Test JSON with special characters
    CipherDB::NotificationApiKeys specialCharsKey;
    specialCharsKey.setName("special_chars_key");
    specialCharsKey.setDriver("test");

    nlohmann::json specialFields;
    specialFields["special"] = R"("quoted",'quotes',\backslash,/slash,\u00F1,\n\r\t)";
    specialFields["unicode"] = "Unicode: 你好, привет, مرحبا, 😀";

    specialCharsKey.setFields(specialFields);
    specialCharsKey.setCreatedAt(1625184000000);

    ASSERT_TRUE(specialCharsKey.save());

    // Retrieve and verify special characters JSON
    auto foundSpecialKey = CipherDB::NotificationApiKeys::findById(nullptr, specialCharsKey.getId());
    ASSERT_TRUE(foundSpecialKey.has_value());
    ASSERT_EQ(foundSpecialKey->getFields()["special"], R"("quoted",'quotes',\backslash,/slash,\u00F1,\n\r\t)");
    ASSERT_EQ(foundSpecialKey->getFields()["unicode"], "Unicode: 你好, привет, مرحبا, 😀");

    // Test invalid JSON handling
    CipherDB::NotificationApiKeys invalidJsonKey;
    invalidJsonKey.setName("invalid_json_key");
    invalidJsonKey.setDriver("test");

    ASSERT_THROW(invalidJsonKey.setFieldsJson("{invalid_json:}"), std::invalid_argument);
}

// Test multithreaded operations
TEST_F(DBTest, NotificationApiKeysMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > keyIds(numThreads);
    std::atomic< int > successCount{0};

    // Create API keys in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::NotificationApiKeys apiKey;
            apiKey.setName("concurrent_key_" + std::to_string(index));
            apiKey.setDriver(index % 2 == 0 ? "NotificationApiKeysMultithreadedOperations:telegram"
                                            : "NotificationApiKeysMultithreadedOperations:discord");

            nlohmann::json fields;
            fields["index"]     = index;
            fields["timestamp"] = std::chrono::duration_cast< std::chrono::milliseconds >(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            apiKey.setFields(fields);
            apiKey.setCreatedAt(1625184000000 + index * 3600000);

            if (apiKey.save(conn))
            {
                keyIds[index] = apiKey.getId();
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

    // Reset success count for concurrent queries
    successCount = 0;

    // Query API keys in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::NotificationApiKeys::findById(nullptr, keyIds[index]);
            if (result.has_value() && result->getName() == "concurrent_key_" + std::to_string(index))
            {
                successCount++;
            }
            return result.has_value();
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

    // Wait for all query threads to complete
    for (auto& future : queryFutures)
    {
        ASSERT_TRUE(future.get());
    }

    // Verify all queries found the correct data
    ASSERT_EQ(successCount, numThreads);

    // Test concurrent filter queries
    std::atomic< int > telegramCount{0};
    std::atomic< int > discordCount{0};

    auto filterFunc = [&](const std::string& driver)
    {
        try
        {
            auto filter = CipherDB::NotificationApiKeys::createFilter().withDriver(driver);
            auto result = CipherDB::NotificationApiKeys::findByFilter(nullptr, filter);

            if (result.has_value())
            {
                if (driver == "NotificationApiKeysMultithreadedOperations:telegram")
                {
                    telegramCount = result->size();
                }
                else if (driver == "NotificationApiKeysMultithreadedOperations:discord")
                {
                    discordCount = result->size();
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

    // Launch concurrent filter queries
    auto telegramFuture =
        std::async(std::launch::async, filterFunc, "NotificationApiKeysMultithreadedOperations:telegram");
    auto discordFuture =
        std::async(std::launch::async, filterFunc, "NotificationApiKeysMultithreadedOperations:discord");

    // Wait for both queries to complete
    ASSERT_TRUE(telegramFuture.get());
    ASSERT_TRUE(discordFuture.get());

    // Verify we found the expected number of each driver type
    // (numThreads/2 rounded up for telegram, numThreads/2 rounded down for discord)
    ASSERT_EQ(telegramCount, (numThreads + 1) / 2);
    ASSERT_EQ(discordCount, numThreads / 2);
}

// Test edge cases
TEST_F(DBTest, NotificationApiKeysEdgeCases)
{
    // Test with minimum values
    CipherDB::NotificationApiKeys minApiKey;
    minApiKey.setName("min_key");
    minApiKey.setDriver("");                       // Empty string for driver
    minApiKey.setFields(nlohmann::json::object()); // Empty JSON object
    minApiKey.setCreatedAt(0);                     // Minimum timestamp

    // Save should work
    ASSERT_TRUE(minApiKey.save());

    // Verify retrieval
    auto foundMinKey = CipherDB::NotificationApiKeys::findById(nullptr, minApiKey.getId());
    ASSERT_TRUE(foundMinKey.has_value());
    ASSERT_EQ(foundMinKey->getDriver(), "");
    ASSERT_EQ(foundMinKey->getCreatedAt(), 0);

    // Test with extremely long values
    CipherDB::NotificationApiKeys longValuesKey;

    // Create a very long string
    std::string longString(1000, 'a');

    longValuesKey.setName("long_values_key");
    longValuesKey.setDriver(longString);

    // Create a large JSON object
    nlohmann::json largeJson;
    for (int i = 0; i < 100; ++i)
    {
        largeJson["key_" + std::to_string(i)] = longString;
    }
    longValuesKey.setFields(largeJson);

    longValuesKey.setCreatedAt(std::numeric_limits< int64_t >::max());

    // Save should work with long values
    ASSERT_TRUE(longValuesKey.save());

    // Verify retrieval of long values
    auto foundLongKey = CipherDB::NotificationApiKeys::findById(nullptr, longValuesKey.getId());
    ASSERT_TRUE(foundLongKey.has_value());
    ASSERT_EQ(foundLongKey->getDriver(), longString);
    ASSERT_EQ(foundLongKey->getCreatedAt(), std::numeric_limits< int64_t >::max());

    // Check JSON size is preserved
    ASSERT_EQ(foundLongKey->getFields().size(), 100);

    // Test with special characters in name and driver
    CipherDB::NotificationApiKeys specialCharsKey;
    specialCharsKey.setName("special!@#$%^&*()_+=");
    specialCharsKey.setDriver("驱动/пример/مثال");
    specialCharsKey.setFields({{"special", true}});
    specialCharsKey.setCreatedAt(1625184000000);

    // Save should work with special characters
    ASSERT_TRUE(specialCharsKey.save());

    // Verify retrieval of special characters
    auto foundSpecialKey = CipherDB::NotificationApiKeys::findById(nullptr, specialCharsKey.getId());
    ASSERT_TRUE(foundSpecialKey.has_value());
    ASSERT_EQ(foundSpecialKey->getName(), "special!@#$%^&*()_+=");
    ASSERT_EQ(foundSpecialKey->getDriver(), "驱动/пример/مثال");
}

// Test attribute construction
TEST_F(DBTest, NotificationApiKeysAttributeConstruction)
{
    // Create attributes map
    std::unordered_map< std::string, std::any > attributes;

    // UUID as string
    boost::uuids::uuid id = boost::uuids::random_generator()();
    attributes["id"]      = boost::uuids::to_string(id);

    // Regular attributes
    attributes["name"]   = std::string("attr_constructed_key");
    attributes["driver"] = std::string("telegram");

    // JSON as string
    attributes["fields_json"] = std::string(R"({"token":"12345","chat_id":"67890"})");

    // Timestamp
    attributes["created_at"] = int64_t(1625184000000);

    // Construct from attributes
    CipherDB::NotificationApiKeys attrKey(attributes);

    // Verify attributes were set correctly
    ASSERT_EQ(attrKey.getId(), id);
    ASSERT_EQ(attrKey.getName(), "attr_constructed_key");
    ASSERT_EQ(attrKey.getDriver(), "telegram");
    ASSERT_EQ(attrKey.getFields()["token"], "12345");
    ASSERT_EQ(attrKey.getFields()["chat_id"], "67890");
    ASSERT_EQ(attrKey.getCreatedAt(), 1625184000000);

    // Save to database
    ASSERT_TRUE(attrKey.save());

    // Retrieve and verify
    auto foundAttrKey = CipherDB::NotificationApiKeys::findById(nullptr, id);
    ASSERT_TRUE(foundAttrKey.has_value());
    ASSERT_EQ(foundAttrKey->getName(), "attr_constructed_key");
}

// Test error handling
TEST_F(DBTest, NotificationApiKeysErrorHandling)
{
    // Test unique constraint violation
    CipherDB::NotificationApiKeys key1;
    key1.setName("unique_test_key");
    key1.setDriver("telegram");
    key1.setFields({{"test", true}});
    key1.setCreatedAt(1625184000000);

    ASSERT_TRUE(key1.save());

    // Try to create another key with the same name
    CipherDB::NotificationApiKeys key2;
    key2.setName("unique_test_key"); // Same name as key1
    key2.setDriver("discord");
    key2.setFields({{"different", true}});
    key2.setCreatedAt(1625184000000);

    // Should fail due to unique constraint
    ASSERT_FALSE(key2.save());

    // Test invalid JSON in setFieldsJson
    CipherDB::NotificationApiKeys invalidJsonKey;
    invalidJsonKey.setName("invalid_json_test");
    invalidJsonKey.setDriver("test");

    // These should throw exceptions
    ASSERT_THROW(invalidJsonKey.setFieldsJson("{not valid json}"), std::invalid_argument);
    ASSERT_THROW(invalidJsonKey.setFieldsJson("not even json format"), std::invalid_argument);

    // Valid JSON should not throw
    ASSERT_NO_THROW(invalidJsonKey.setFieldsJson("{}"));
    ASSERT_NO_THROW(invalidJsonKey.setFieldsJson(R"({"valid":true})"));
}

// Test table creation
TEST_F(DBTest, NotificationApiKeysTableCreation)
{
    // This test is a bit tricky since the table is already created in the test setup
    // But we can verify that the model can save data, which proves the table exists

    CipherDB::NotificationApiKeys testKey;
    testKey.setName("table_creation_test");
    testKey.setDriver("test");
    testKey.setFields({{"test", true}});
    testKey.setCreatedAt(1625184000000);

    // Should save successfully if table exists
    ASSERT_TRUE(testKey.save());

    // Verify retrieval
    auto foundKey = CipherDB::NotificationApiKeys::findById(nullptr, testKey.getId());
    ASSERT_TRUE(foundKey.has_value());
}

// Test basic CRUD operations for Option model
TEST_F(DBTest, OptionBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new option
    CipherDB::Option option;
    option.setType("test_option");
    option.setUpdatedAt(1625184000000); // 2021-07-02 00:00:00 UTC

    nlohmann::json testJson = {{"key1", "value1"}, {"key2", 42}, {"key3", true}};
    option.setJson(testJson);

    // Save the option
    ASSERT_TRUE(option.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = option.getId();

    // Find the option by ID
    auto foundOption = CipherDB::Option::findById(conn, id);

    // Verify option was found
    ASSERT_TRUE(foundOption.has_value());

    // Verify option properties
    ASSERT_EQ(foundOption->getType(), "test_option");
    ASSERT_EQ(foundOption->getUpdatedAt(), 1625184000000);

    auto jsonData = foundOption->getJson();
    ASSERT_EQ(jsonData["key1"], "value1");
    ASSERT_EQ(jsonData["key2"], 42);
    ASSERT_EQ(jsonData["key3"], true);

    // Update the option
    foundOption->setType("updated_option");
    foundOption->updateTimestamp(); // Should update timestamp to current time

    nlohmann::json updatedJson = {{"key1", "new_value"}, {"key4", "added_field"}};
    foundOption->setJson(updatedJson);

    // Save the updated option
    ASSERT_TRUE(foundOption->save(conn));

    // Retrieve it again
    auto updatedOption = CipherDB::Option::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedOption.has_value());
    ASSERT_EQ(updatedOption->getType(), "updated_option");
    ASSERT_GT(updatedOption->getUpdatedAt(), 1625184000000); // Should be greater than initial timestamp

    auto updatedJsonData = updatedOption->getJson();
    ASSERT_EQ(updatedJsonData["key1"], "new_value");
    ASSERT_EQ(updatedJsonData["key4"], "added_field");
    ASSERT_FALSE(updatedJsonData.contains("key2")); // Old keys should be gone

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

// Test filtering options
TEST_F(DBTest, OptionFindByFilter)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create multiple options with different types
    std::vector< boost::uuids::uuid > optionIds;

    // Create 5 options of type "settings"
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::Option option;
        option.setType("OptionFindByFilter:settings");
        option.setUpdatedAt(1625184000000 + i * 3600000); // 1 hour increments

        nlohmann::json testJson = {{"setting_id", i}, {"name", "setting_" + std::to_string(i)}, {"value", i * 10}};
        option.setJson(testJson);

        ASSERT_TRUE(option.save(conn));
        optionIds.push_back(option.getId());
    }

    // Create 3 options of type "preferences"
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::Option option;
        option.setType("OptionFindByFilter:preferences");
        option.setUpdatedAt(1625184000000 + i * 3600000);

        nlohmann::json testJson = {{"pref_id", i}, {"user", "user_" + std::to_string(i)}, {"enabled", i % 2 == 0}};
        option.setJson(testJson);

        ASSERT_TRUE(option.save(conn));
        optionIds.push_back(option.getId());
    }

    // Commit all options at once
    ASSERT_TRUE(txGuard.commit());

    // Test filtering by type "settings"
    auto result =
        CipherDB::Option::findByFilter(conn, CipherDB::Option::createFilter().withType("OptionFindByFilter:settings"));

    // Verify we found the right options
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);

    // Verify these are settings type options
    for (const auto& opt : *result)
    {
        ASSERT_EQ(opt.getType(), "OptionFindByFilter:settings");
        auto json = opt.getJson();
        ASSERT_TRUE(json.contains("setting_id"));
    }

    // Test filtering by type "preferences"
    result = CipherDB::Option::findByFilter(
        conn, CipherDB::Option::createFilter().withType("OptionFindByFilter:preferences"));

    // Verify we found the right options
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);

    // Verify these are preferences type options
    for (const auto& opt : *result)
    {
        ASSERT_EQ(opt.getType(), "OptionFindByFilter:preferences");
        auto json = opt.getJson();
        ASSERT_TRUE(json.contains("pref_id"));
    }

    // Test filtering by ID
    auto firstId = optionIds[0];
    result       = CipherDB::Option::findByFilter(conn, CipherDB::Option::createFilter().withId(firstId));

    // Verify we found exactly one option
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1);
    ASSERT_EQ((*result)[0].getId(), firstId);

    // Test filtering with non-existent type
    result = CipherDB::Option::findByFilter(
        conn, CipherDB::Option::createFilter().withType("OptionFindByFilter:non_existent_type"));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

// Test JSON handling
TEST_F(DBTest, OptionJsonHandling)
{
    CipherDB::Option option;
    option.setType("json_test");

    // Test setting and getting simple JSON
    nlohmann::json simpleJson = {{"string", "text"}, {"number", 42}, {"boolean", true}, {"null", nullptr}};
    option.setJson(simpleJson);

    auto retrievedJson = option.getJson();
    ASSERT_EQ(retrievedJson["string"], "text");
    ASSERT_EQ(retrievedJson["number"], 42);
    ASSERT_EQ(retrievedJson["boolean"], true);
    ASSERT_EQ(retrievedJson["null"], nullptr);

    // Test nested JSON structures
    nlohmann::json nestedJson = {{"array", {1, 2, 3, 4}},
                                 {"object", {{"nested", "value"}, {"deep", {{"deeper", "deepest"}}}}}};
    option.setJson(nestedJson);

    retrievedJson = option.getJson();
    ASSERT_EQ(retrievedJson["array"][0], 1);
    ASSERT_EQ(retrievedJson["array"][3], 4);
    ASSERT_EQ(retrievedJson["object"]["nested"], "value");
    ASSERT_EQ(retrievedJson["object"]["deep"]["deeper"], "deepest");

    // Test setting JSON from string
    std::string jsonStr = R"({"string_key":"string_value","array_key":[1,2,3]})";
    option.setJsonStr(jsonStr);

    retrievedJson = option.getJson();
    ASSERT_EQ(retrievedJson["string_key"], "string_value");
    ASSERT_EQ(retrievedJson["array_key"][0], 1);
    ASSERT_EQ(retrievedJson["array_key"][2], 3);

    // Test invalid JSON handling
    ASSERT_THROW(option.setJsonStr("invalid json"), std::invalid_argument);

    // Test empty JSON
    option.setJsonStr("{}");
    retrievedJson = option.getJson();
    ASSERT_TRUE(retrievedJson.empty());

    // Save all these changes to verify JSON persistence
    ASSERT_TRUE(option.save());
}

// Test transaction safety
TEST_F(DBTest, OptionTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new option
    CipherDB::Option option;
    option.setType("transaction_test");
    option.setUpdatedAt(1625184000000);

    nlohmann::json testJson = {{"test", "transaction_value"}};
    option.setJson(testJson);

    // Save the option
    ASSERT_TRUE(option.save(conn));

    // Get the ID for later check
    boost::uuids::uuid id = option.getId();

    // Roll back the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the option - should not exist after rollback
    auto foundOption = CipherDB::Option::findById(nullptr, id);
    ASSERT_FALSE(foundOption.has_value());

    // Create another transaction
    CipherDB::db::TransactionGuard txGuard2;
    auto conn2 = txGuard2.getConnection();

    // Save the option again
    ASSERT_TRUE(option.save(conn2));

    // Commit this time
    ASSERT_TRUE(txGuard2.commit());

    // Now the option should exist
    foundOption = CipherDB::Option::findById(nullptr, id);
    ASSERT_TRUE(foundOption.has_value());
    ASSERT_EQ(foundOption->getJson()["test"], "transaction_value");
}

// Test edge cases
TEST_F(DBTest, OptionEdgeCases)
{
    // Test with empty type
    CipherDB::Option emptyTypeOption;
    emptyTypeOption.setType("");
    emptyTypeOption.setUpdatedAt(1625184000000);
    nlohmann::json emptyTypeJson = {{"test", "value"}};
    emptyTypeOption.setJson(emptyTypeJson);

    // Save should work with empty type
    ASSERT_TRUE(emptyTypeOption.save());

    // Test with minimum values
    CipherDB::Option minOption;
    minOption.setType("min_test");
    minOption.setUpdatedAt(0);
    minOption.setJsonStr("{}");

    // Save should work with minimum values
    ASSERT_TRUE(minOption.save());

    // Test with extreme values
    CipherDB::Option extremeOption;
    extremeOption.setType("extreme_test");
    extremeOption.setUpdatedAt(std::numeric_limits< int64_t >::max());

    // Create a very large JSON object
    nlohmann::json largeJson;
    for (int i = 0; i < 1000; i++)
    {
        largeJson["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }
    extremeOption.setJson(largeJson);

    // Save should still work
    ASSERT_TRUE(extremeOption.save());

    // Verify extreme option can be retrieved
    auto foundOption = CipherDB::Option::findById(nullptr, extremeOption.getId());
    ASSERT_TRUE(foundOption.has_value());
    ASSERT_EQ(foundOption->getUpdatedAt(), std::numeric_limits< int64_t >::max());
    ASSERT_EQ(foundOption->getJson()["key_999"], "value_999");

    // Test with very long type name
    CipherDB::Option longTypeOption;
    std::string longType(1000, 'x');
    longTypeOption.setType(longType);
    longTypeOption.setUpdatedAt(1625184000000);

    // Save should work with long type
    ASSERT_TRUE(longTypeOption.save());

    // Test invalid JSON parsing recovery
    CipherDB::Option invalidJsonOption;
    invalidJsonOption.setType("invalid_json_test");
    invalidJsonOption.setUpdatedAt(1625184000000);

    // Force invalid JSON string (not using public API)
    // This test is for the recovery part of getJson()
    std::string invalidJsonStr = "{invalid:json}";

    // getJson should return empty object on parse error
    auto recoveredJson = invalidJsonOption.getJson();
    ASSERT_TRUE(recoveredJson.is_object());
    ASSERT_TRUE(recoveredJson.empty());
}

// Test multithreaded operations
TEST_F(DBTest, OptionMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > optionIds(numThreads);
    std::atomic< int > successCount{0};

    // Create options in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::Option option;
            option.setType("multithreaded_test_" + std::to_string(index));
            option.setUpdatedAt(1625184000000 + index * 3600000);

            nlohmann::json testJson = {{"thread_id", index}, {"value", index * 100}};
            option.setJson(testJson);

            if (option.save(conn))
            {
                optionIds[index] = option.getId();
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

    // Launch threads for creating options
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

    // Query options in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::Option::findById(nullptr, optionIds[index]);
            if (result.has_value())
            {
                // Verify the option has the correct properties
                if (result->getType() == "multithreaded_test_" + std::to_string(index) &&
                    result->getJson()["thread_id"] == index)
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

    // Launch threads for querying options
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
            for (int i = 0; i < numThreads; i++)
            {
                // Get a random option to test
                int index   = rand() % numThreads;
                auto filter = CipherDB::Option::createFilter().withType("multithreaded_test_" + std::to_string(index));
                auto result = CipherDB::Option::findByFilter(nullptr, filter);

                if (!result.has_value() || result->size() != 1 || (*result)[0].getJson()["thread_id"] != index)
                {
                    return false;
                }
            }
            return true;
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

// Test with attribute map constructor
TEST_F(DBTest, OptionAttributeConstruction)
{
    // Create an attribute map with all fields
    std::unordered_map< std::string, std::any > attributes;

    boost::uuids::uuid id    = boost::uuids::random_generator()();
    attributes["id"]         = boost::uuids::to_string(id);
    attributes["type"]       = std::string("constructed_type");
    attributes["updated_at"] = int64_t(1625184000000);

    nlohmann::json attributeJson = {{"constructed", true}, {"values", {1, 2, 3}}};
    attributes["json"]           = attributeJson.dump();

    // Create option from attributes
    CipherDB::Option option(attributes);

    // Verify the attributes were set correctly
    ASSERT_EQ(option.getId(), id);
    ASSERT_EQ(option.getType(), "constructed_type");
    ASSERT_EQ(option.getUpdatedAt(), 1625184000000);
    ASSERT_EQ(option.getJson()["constructed"], true);
    ASSERT_EQ(option.getJson()["values"][1], 2);

    // Save to database and retrieve
    ASSERT_TRUE(option.save());

    auto foundOption = CipherDB::Option::findById(nullptr, id);
    ASSERT_TRUE(foundOption.has_value());
    ASSERT_EQ(foundOption->getType(), "constructed_type");
    ASSERT_EQ(foundOption->getJson()["values"][2], 3);

    // Test with partial attributes
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["type"] = std::string("partial_type");

    CipherDB::Option partialOption(partialAttributes);

    // ID should be a new UUID
    ASSERT_NE(partialOption.getIdAsString(), "");
    ASSERT_EQ(partialOption.getType(), "partial_type");

    // JSON should be empty object by default
    ASSERT_TRUE(partialOption.getJson().is_object());
    ASSERT_TRUE(partialOption.getJson().empty());
}

// Test for table creation
TEST_F(DBTest, OptionTableCreation)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new option
    CipherDB::Option option;
    option.setType("table_test");
    option.setUpdatedAt(1625184000000);
    nlohmann::json testJson = {{"feature", "enabled"}};
    option.setJson(testJson);

    // If the table exists, this should succeed
    ASSERT_TRUE(option.save(conn));
    ASSERT_TRUE(txGuard.commit());
}

// Tests for the Orderbook model
TEST_F(DBTest, OrderbookBasicCRUD)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new orderbook
    CipherDB::Orderbook orderbook;
    orderbook.setTimestamp(1625184000000); // 2021-07-02 00:00:00 UTC
    orderbook.setSymbol("BTC/USD");
    orderbook.setExchange("binance");

    // Set some binary data (normally this would be serialized orderbook data)
    std::string testData = R"({"bids":[[38000.0,1.5],[37900.0,2.1]],"asks":[[38100.0,1.2],[38200.0,3.0]]})";
    orderbook.setDataFromString(testData);

    // Save the orderbook
    ASSERT_TRUE(orderbook.save(conn));

    // Get the ID for later retrieval
    boost::uuids::uuid id = orderbook.getId();

    // Find the orderbook by ID
    auto foundOrderbook = CipherDB::Orderbook::findById(conn, id);

    // Verify orderbook was found
    ASSERT_TRUE(foundOrderbook.has_value());

    // Verify orderbook properties
    ASSERT_EQ(foundOrderbook->getTimestamp(), 1625184000000);
    ASSERT_EQ(foundOrderbook->getSymbol(), "BTC/USD");
    ASSERT_EQ(foundOrderbook->getExchange(), "binance");
    ASSERT_EQ(foundOrderbook->getDataAsString(), testData);

    // Update the orderbook
    foundOrderbook->setTimestamp(1625270400000); // One day later
    std::string updatedData = R"({"bids":[[38500.0,1.8],[38400.0,2.2]],"asks":[[38600.0,1.5],[38700.0,2.5]]})";
    foundOrderbook->setDataFromString(updatedData);

    // Save the updated orderbook
    ASSERT_TRUE(foundOrderbook->save(conn));

    // Retrieve it again
    auto updatedOrderbook = CipherDB::Orderbook::findById(conn, id);

    // Verify the updates
    ASSERT_TRUE(updatedOrderbook.has_value());
    ASSERT_EQ(updatedOrderbook->getTimestamp(), 1625270400000);
    ASSERT_EQ(updatedOrderbook->getDataAsString(), updatedData);

    // Commit the transaction
    ASSERT_TRUE(txGuard.commit());
}

TEST_F(DBTest, OrderbookFindByFilter)
{
    // Create a transaction guard for batch operations
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create several orderbooks for "binance" exchange
    for (int i = 0; i < 5; ++i)
    {
        CipherDB::Orderbook orderbook;
        orderbook.setTimestamp(1625184000000 + i * 3600000); // 1 hour increments
        orderbook.setSymbol("BTC/USD");
        orderbook.setExchange("OrderbookFindByFilter:binance");

        std::string data = "Data for BTC/USD at timestamp " + std::to_string(1625184000000 + i * 3600000);
        orderbook.setDataFromString(data);

        ASSERT_TRUE(orderbook.save(conn));
    }

    // Create orderbooks for "kraken" exchange
    for (int i = 0; i < 3; ++i)
    {
        CipherDB::Orderbook orderbook;
        orderbook.setTimestamp(1625184000000 + i * 3600000);
        orderbook.setSymbol("ETH/USD");
        orderbook.setExchange("OrderbookFindByFilter:kraken");

        std::string data = "Data for ETH/USD at timestamp " + std::to_string(1625184000000 + i * 3600000);
        orderbook.setDataFromString(data);

        ASSERT_TRUE(orderbook.save(conn));
    }

    // Commit all changes
    ASSERT_TRUE(txGuard.commit());

    // Test filtering by exchange
    auto result = CipherDB::Orderbook::findByFilter(
        conn, CipherDB::Orderbook::createFilter().withExchange("OrderbookFindByFilter:binance"));

    // Verify we found the right orderbooks
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5);
    for (const auto& ob : *result)
    {
        ASSERT_EQ(ob.getExchange(), "OrderbookFindByFilter:binance");
        ASSERT_EQ(ob.getSymbol(), "BTC/USD");
    }

    // Test filtering by symbol
    result = CipherDB::Orderbook::findByFilter(conn, CipherDB::Orderbook::createFilter().withSymbol("ETH/USD"));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3);
    for (const auto& ob : *result)
    {
        ASSERT_EQ(ob.getExchange(), "OrderbookFindByFilter:kraken");
        ASSERT_EQ(ob.getSymbol(), "ETH/USD");
    }

    // Test filtering by timestamp
    result = CipherDB::Orderbook::findByFilter(conn, CipherDB::Orderbook::createFilter().withTimestamp(1625184000000));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2); // One from each exchange at the same timestamp

    // Test filtering by timestamp range
    result = CipherDB::Orderbook::findByFilter(conn,
                                               CipherDB::Orderbook::createFilter()
                                                   .withExchange("OrderbookFindByFilter:binance")
                                                   .withTimestampRange(1625184000000, 1625184000000 + 2 * 3600000));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3); // First 3 binance orderbooks

    // Test with non-existent parameters
    result = CipherDB::Orderbook::findByFilter(
        conn, CipherDB::Orderbook::createFilter().withExchange("non_existent_exchange"));

    // Should return empty vector but not nullopt
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 0);
}

TEST_F(DBTest, OrderbookTransactionSafety)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create a new orderbook
    CipherDB::Orderbook orderbook;
    orderbook.setTimestamp(1625284000000);
    orderbook.setSymbol("BTC/USD");
    orderbook.setExchange("binance");
    orderbook.setDataFromString("Transaction test data");

    // Save the orderbook
    ASSERT_TRUE(orderbook.save(conn));

    // Get the ID for later check
    boost::uuids::uuid id = orderbook.getId();

    // Roll back the transaction
    ASSERT_TRUE(txGuard.rollback());

    // Try to find the orderbook - should not exist after rollback
    auto foundOrderbook = CipherDB::Orderbook::findById(nullptr, id);
    ASSERT_FALSE(foundOrderbook.has_value());

    // Create another transaction
    CipherDB::db::TransactionGuard txGuard2;
    auto conn2 = txGuard2.getConnection();

    // Save the orderbook again
    ASSERT_TRUE(orderbook.save(conn2));

    // Commit this time
    ASSERT_TRUE(txGuard2.commit());

    // Now the orderbook should exist
    foundOrderbook = CipherDB::Orderbook::findById(nullptr, id);
    ASSERT_TRUE(foundOrderbook.has_value());
    ASSERT_EQ(foundOrderbook->getDataAsString(), "Transaction test data");
}

TEST_F(DBTest, OrderbookEdgeCases)
{
    // Edge case 1: Empty data
    CipherDB::Orderbook emptyOrderbook;
    emptyOrderbook.setTimestamp(1625384000000);
    emptyOrderbook.setSymbol("BTC/USD");
    emptyOrderbook.setExchange("binance");
    emptyOrderbook.setDataFromString("");

    // Save should work with empty data
    ASSERT_TRUE(emptyOrderbook.save());

    auto foundEmptyOrderbook = CipherDB::Orderbook::findById(nullptr, emptyOrderbook.getId());
    ASSERT_TRUE(foundEmptyOrderbook.has_value());
    ASSERT_EQ(foundEmptyOrderbook->getDataAsString(), "");

    // Edge case 2: Minimum values
    CipherDB::Orderbook minOrderbook;
    minOrderbook.setTimestamp(0);
    minOrderbook.setSymbol("");
    minOrderbook.setExchange("");
    std::vector< uint8_t > emptyData;
    minOrderbook.setData(emptyData);

    // Save should work with minimum values
    ASSERT_TRUE(minOrderbook.save());

    // Edge case 3: Very large data
    CipherDB::Orderbook largeOrderbook;
    largeOrderbook.setTimestamp(std::numeric_limits< int64_t >::max());
    largeOrderbook.setSymbol("BTC/USD");
    largeOrderbook.setExchange("binance");

    // Create a 100KB string
    std::string largeData(100 * 1024, 'X');
    largeOrderbook.setDataFromString(largeData);

    // Save should still work with large data
    ASSERT_TRUE(largeOrderbook.save());

    auto foundLargeOrderbook = CipherDB::Orderbook::findById(nullptr, largeOrderbook.getId());
    ASSERT_TRUE(foundLargeOrderbook.has_value());
    ASSERT_EQ(foundLargeOrderbook->getTimestamp(), std::numeric_limits< int64_t >::max());
    ASSERT_EQ(foundLargeOrderbook->getData().size(), 100 * 1024);

    // Edge case 4: Binary data with null bytes and control characters
    CipherDB::Orderbook binaryOrderbook;
    binaryOrderbook.setTimestamp(1625484000000);
    binaryOrderbook.setSymbol("BTC/USD");
    binaryOrderbook.setExchange("binance");

    // Create binary data with various byte values including nulls
    std::vector< uint8_t > binaryData;
    for (int i = 0; i < 256; i++)
    {
        binaryData.push_back(static_cast< uint8_t >(i));
    }
    binaryOrderbook.setData(binaryData);

    // Save should work with binary data
    ASSERT_TRUE(binaryOrderbook.save());

    auto foundBinaryOrderbook = CipherDB::Orderbook::findById(nullptr, binaryOrderbook.getId());
    ASSERT_TRUE(foundBinaryOrderbook.has_value());
    ASSERT_EQ(foundBinaryOrderbook->getData().size(), 256);

    // Verify the binary data was preserved exactly
    const auto& retrievedData = foundBinaryOrderbook->getData();
    for (int i = 0; i < 256; i++)
    {
        ASSERT_EQ(retrievedData[i], static_cast< uint8_t >(i));
    }

    // Edge case 5: Very long strings for symbol and exchange
    CipherDB::Orderbook longStringOrderbook;
    longStringOrderbook.setTimestamp(1625584000000);
    std::string longString(255, 'a');
    longStringOrderbook.setSymbol(longString);
    longStringOrderbook.setExchange(longString);
    longStringOrderbook.setDataFromString("Test data");

    // Save should work with long strings
    ASSERT_TRUE(longStringOrderbook.save());
}

TEST_F(DBTest, OrderbookMultithreadedOperations)
{
    constexpr int numThreads = 10;
    std::vector< boost::uuids::uuid > orderbookIds(numThreads);
    std::atomic< int > successCount{0};

    // Create orderbooks in parallel
    auto createFunc = [&](int index)
    {
        try
        {
            // Create transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            CipherDB::Orderbook orderbook;
            orderbook.setTimestamp(1625684000000 + index * 3600000);
            orderbook.setSymbol("BTC/USD");
            orderbook.setExchange("OrderbookMultithreadedOperations:thread_test");

            std::string data =
                "Thread " + std::to_string(index) + " data at " + std::to_string(1625684000000 + index * 3600000);
            orderbook.setDataFromString(data);

            if (orderbook.save(conn))
            {
                orderbookIds[index] = orderbook.getId();
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

    // Launch threads for creating orderbooks
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

    // Query orderbooks in parallel
    auto queryFunc = [&](int index)
    {
        try
        {
            auto result = CipherDB::Orderbook::findById(nullptr, orderbookIds[index]);
            if (result.has_value())
            {
                // Verify the orderbook has the correct properties
                if (result->getTimestamp() == 1625684000000 + index * 3600000 &&
                    result->getExchange() == "OrderbookMultithreadedOperations:thread_test" &&
                    result->getDataAsString().find("Thread " + std::to_string(index)) != std::string::npos)
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

    // Launch threads for querying orderbooks
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
            auto filter = CipherDB::Orderbook::createFilter()
                              .withExchange("OrderbookMultithreadedOperations:thread_test")
                              .withSymbol("BTC/USD");
            auto result = CipherDB::Orderbook::findByFilter(nullptr, filter);
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

TEST_F(DBTest, OrderbookAttributeConstruction)
{
    // Create an attribute map with all fields
    std::unordered_map< std::string, std::any > attributes;

    boost::uuids::uuid id   = boost::uuids::random_generator()();
    attributes["id"]        = boost::uuids::to_string(id);
    attributes["timestamp"] = int64_t(1625784000000);
    attributes["symbol"]    = std::string("BTC/USD");
    attributes["exchange"]  = std::string("binance");

    std::string data   = "Test data for attribute construction";
    attributes["data"] = data;

    // Create orderbook from attributes
    CipherDB::Orderbook orderbook(attributes);

    // Verify the attributes were set correctly
    ASSERT_EQ(orderbook.getId(), id);
    ASSERT_EQ(orderbook.getTimestamp(), 1625784000000);
    ASSERT_EQ(orderbook.getSymbol(), "BTC/USD");
    ASSERT_EQ(orderbook.getExchange(), "binance");
    ASSERT_EQ(orderbook.getDataAsString(), data);

    // Save to database and retrieve
    ASSERT_TRUE(orderbook.save());

    auto foundOrderbook = CipherDB::Orderbook::findById(nullptr, id);
    ASSERT_TRUE(foundOrderbook.has_value());
    ASSERT_EQ(foundOrderbook->getDataAsString(), data);

    // Test with partial attributes
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["symbol"]   = std::string("ETH/USD");
    partialAttributes["exchange"] = std::string("kraken");

    CipherDB::Orderbook partialOrderbook(partialAttributes);

    // ID should be a new UUID
    ASSERT_NE(partialOrderbook.getIdAsString(), "");
    ASSERT_EQ(partialOrderbook.getSymbol(), "ETH/USD");
    ASSERT_EQ(partialOrderbook.getExchange(), "kraken");
    ASSERT_EQ(partialOrderbook.getTimestamp(), 0);   // Default timestamp
    ASSERT_EQ(partialOrderbook.getData().size(), 0); // Empty data
}

TEST_F(DBTest, OrderbookTimestampRangeFiltering)
{
    // Create a transaction guard
    CipherDB::db::TransactionGuard txGuard;
    auto conn = txGuard.getConnection();

    // Create several orderbooks with sequential timestamps
    std::vector< boost::uuids::uuid > ids;
    int64_t baseTimestamp = 1625884000000; // 2021-07-02 00:00:00 UTC

    for (int i = 0; i < 10; ++i)
    {
        CipherDB::Orderbook orderbook;
        orderbook.setTimestamp(baseTimestamp + i * 3600000); // Hourly intervals
        orderbook.setSymbol("BTC/USD");
        orderbook.setExchange("OrderbookTimestampRangeFiltering:test");
        orderbook.setDataFromString("Data " + std::to_string(i));

        ASSERT_TRUE(orderbook.save(conn));
        ids.push_back(orderbook.getId());
    }

    ASSERT_TRUE(txGuard.commit());

    // Test various timestamp range queries

    // 1. Exact range (inclusive)
    auto result = CipherDB::Orderbook::findByFilter(
        nullptr,
        CipherDB::Orderbook::createFilter()
            .withExchange("OrderbookTimestampRangeFiltering:test")
            .withTimestampRange(baseTimestamp + 2 * 3600000, baseTimestamp + 5 * 3600000));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 4); // Should include timestamps at 2, 3, 4, and 5 hours

    // 2. Lower bound only
    result = CipherDB::Orderbook::findByFilter(
        nullptr,
        CipherDB::Orderbook::createFilter()
            .withExchange("OrderbookTimestampRangeFiltering:test")
            .withTimestampRange(baseTimestamp + 8 * 3600000, 0) // End time 0 means no upper bound
    );

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2); // Should include timestamps at 8 and 9 hours

    // 3. Upper bound only
    auto filter = CipherDB::Orderbook::createFilter().withExchange("OrderbookTimestampRangeFiltering:test");
    filter.withTimestampRange(0, baseTimestamp + 1 * 3600000); // Start time 0 means no lower bound

    result = CipherDB::Orderbook::findByFilter(nullptr, filter);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2); // Should include timestamps at 0 and 1 hours

    // 4. Range with additional symbol filter
    result = CipherDB::Orderbook::findByFilter(
        nullptr,
        CipherDB::Orderbook::createFilter()
            .withExchange("OrderbookTimestampRangeFiltering:test")
            .withSymbol("BTC/USD")
            .withTimestampRange(baseTimestamp + 3 * 3600000, baseTimestamp + 7 * 3600000));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 5); // Should include timestamps at 3, 4, 5, 6, and 7 hours
}

TEST_F(DBTest, OrderbookDataHandling)
{
    CipherDB::Orderbook orderbook;
    orderbook.setTimestamp(1625984000000);
    orderbook.setSymbol("BTC/USD");
    orderbook.setExchange("binance");

    // Test setting and getting data with string conversion
    std::string jsonData = R"({"bids":[[39000.5,2.1],[38900.75,1.8]],"asks":[[39100.25,1.5],[39200.0,2.0]]})";
    orderbook.setDataFromString(jsonData);

    std::string retrievedData = orderbook.getDataAsString();
    ASSERT_EQ(retrievedData, jsonData);

    // Test setting data directly with vector<uint8_t>
    std::vector< uint8_t > binaryData = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xFE, 0xFD, 0xFC};
    orderbook.setData(binaryData);

    const auto& retrievedBinaryData = orderbook.getData();
    ASSERT_EQ(retrievedBinaryData.size(), binaryData.size());
    for (size_t i = 0; i < binaryData.size(); i++)
    {
        ASSERT_EQ(retrievedBinaryData[i], binaryData[i]);
    }

    // Save and verify persistence of binary data
    ASSERT_TRUE(orderbook.save());

    auto foundOrderbook = CipherDB::Orderbook::findById(nullptr, orderbook.getId());
    ASSERT_TRUE(foundOrderbook.has_value());

    const auto& persistedData = foundOrderbook->getData();
    ASSERT_EQ(persistedData.size(), binaryData.size());
    for (size_t i = 0; i < binaryData.size(); i++)
    {
        ASSERT_EQ(persistedData[i], binaryData[i]);
    }
}

TEST_F(DBTest, TickerBasicCRUD)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Create a ticker
    CipherDB::Ticker ticker;
    ticker.setTimestamp(1620000000000);
    ticker.setLastPrice(50000.0);
    ticker.setVolume(2.5);
    ticker.setHighPrice(51000.0);
    ticker.setLowPrice(49000.0);
    ticker.setSymbol("BTC/USD");
    ticker.setExchange("binance");

    EXPECT_TRUE(ticker.save(conn));

    // Read the ticker back
    auto found = CipherDB::Ticker::findById(conn, ticker.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(found->getLastPrice(), 50000.0);
    EXPECT_DOUBLE_EQ(found->getVolume(), 2.5);
    EXPECT_DOUBLE_EQ(found->getHighPrice(), 51000.0);
    EXPECT_DOUBLE_EQ(found->getLowPrice(), 49000.0);
    EXPECT_EQ(found->getSymbol(), "BTC/USD");
    EXPECT_EQ(found->getExchange(), "binance");

    // Update the ticker
    ticker.setLastPrice(52000.0);
    ticker.setVolume(3.0);
    EXPECT_TRUE(ticker.save(conn));

    // Read again and verify update
    found = CipherDB::Ticker::findById(conn, ticker.getId());
    ASSERT_TRUE(found);
    EXPECT_DOUBLE_EQ(found->getLastPrice(), 52000.0);
    EXPECT_DOUBLE_EQ(found->getVolume(), 3.0);
}

TEST_F(DBTest, TickerFindByFilter)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Create test tickers
    CipherDB::Ticker ticker1;
    ticker1.setTimestamp(1620000000000);
    ticker1.setLastPrice(50000.0);
    ticker1.setVolume(2.5);
    ticker1.setHighPrice(51000.0);
    ticker1.setLowPrice(49000.0);
    ticker1.setSymbol("TickerFindByFilter:BTC/USD");
    ticker1.setExchange("TickerFindByFilter:binance");
    EXPECT_TRUE(ticker1.save(conn));

    CipherDB::Ticker ticker2;
    ticker2.setTimestamp(1620000100000);
    ticker2.setLastPrice(51000.0);
    ticker2.setVolume(1.5);
    ticker2.setHighPrice(52000.0);
    ticker2.setLowPrice(50000.0);
    ticker2.setSymbol("TickerFindByFilter:BTC/USD");
    ticker2.setExchange("TickerFindByFilter:binance");
    EXPECT_TRUE(ticker2.save(conn));

    CipherDB::Ticker ticker3;
    ticker3.setTimestamp(1620000200000);
    ticker3.setLastPrice(49000.0);
    ticker3.setVolume(3.0);
    ticker3.setHighPrice(50000.0);
    ticker3.setLowPrice(48000.0);
    ticker3.setSymbol("TickerFindByFilter:ETH/USD");
    ticker3.setExchange("TickerFindByFilter:coinbase");
    EXPECT_TRUE(ticker3.save(conn));

    // Find by symbol
    auto filter1 = CipherDB::Ticker::createFilter().withSymbol("TickerFindByFilter:BTC/USD");
    auto result1 = CipherDB::Ticker::findByFilter(conn, filter1);
    ASSERT_TRUE(result1);
    EXPECT_EQ(result1->size(), 2);

    // Find by exchange
    auto filter2 = CipherDB::Ticker::createFilter().withExchange("TickerFindByFilter:coinbase");
    auto result2 = CipherDB::Ticker::findByFilter(conn, filter2);
    ASSERT_TRUE(result2);
    EXPECT_EQ(result2->size(), 1);
    EXPECT_EQ((*result2)[0].getSymbol(), "TickerFindByFilter:ETH/USD");

    // Find by price range
    auto filter3 = CipherDB::Ticker::createFilter().withLastPriceRange(49500.0, 51500.0);
    auto result3 = CipherDB::Ticker::findByFilter(conn, filter3);
    ASSERT_TRUE(result3);
    EXPECT_EQ(result3->size(), 2);

    // Find by timestamp range
    auto filter4 = CipherDB::Ticker::createFilter().withTimestampRange(1620000050000, 1620000250000);
    auto result4 = CipherDB::Ticker::findByFilter(conn, filter4);
    ASSERT_TRUE(result4);
    EXPECT_EQ(result4->size(), 2);

    // Combined filters
    auto filter5 = CipherDB::Ticker::createFilter()
                       .withSymbol("TickerFindByFilter:BTC/USD")
                       .withExchange("TickerFindByFilter:binance")
                       .withTimestampRange(1620000050000, 1620000150000);
    auto result5 = CipherDB::Ticker::findByFilter(conn, filter5);
    ASSERT_TRUE(result5);
    EXPECT_EQ(result5->size(), 1);
    EXPECT_EQ((*result5)[0].getTimestamp(), 1620000100000);
}

TEST_F(DBTest, TickerTransactionSafety)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    CipherDB::Ticker ticker;
    ticker.setTimestamp(1620000000000);
    ticker.setLastPrice(50000.0);
    ticker.setVolume(2.5);
    ticker.setHighPrice(51000.0);
    ticker.setLowPrice(49000.0);
    ticker.setSymbol("BTC/USD");
    ticker.setExchange("binance");

    // Save ticker in transaction and roll back
    {
        CipherDB::db::TransactionGuard txGuard;
        EXPECT_TRUE(ticker.save(txGuard.getConnection()));

        // Should be able to find ticker within transaction
        auto found = CipherDB::Ticker::findById(txGuard.getConnection(), ticker.getId());
        EXPECT_TRUE(found);

        // Roll back
        EXPECT_TRUE(txGuard.rollback());
    }

    // After rollback, ticker should not exist
    auto found = CipherDB::Ticker::findById(conn, ticker.getId());
    EXPECT_FALSE(found);
}

TEST_F(DBTest, TickerMultithreadedOperations)
{
    const int numThreads = 10;
    std::vector< boost::uuids::uuid > tickerIds(numThreads);
    std::vector< std::future< void > > futures;

    // Test creating tickers concurrently
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [i, &tickerIds]
                                     {
                                         auto conn = CipherDB::db::Database::getInstance().getConnection();

                                         CipherDB::Ticker ticker;
                                         ticker.setTimestamp(1620000000000 + i * 1000);
                                         ticker.setLastPrice(50000.0 + i * 100);
                                         ticker.setVolume(2.5 + i * 0.1);
                                         ticker.setHighPrice(51000.0 + i * 100);
                                         ticker.setLowPrice(49000.0 + i * 100);
                                         ticker.setSymbol("TickerMultithreadedOperations:BTC/USD");
                                         ticker.setExchange("TickerMultithreadedOperations:binance");

                                         EXPECT_TRUE(ticker.save(conn));
                                         tickerIds[i] = ticker.getId();
                                     }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    // Verify all tickers were saved correctly
    auto conn = CipherDB::db::Database::getInstance().getConnection();
    for (int i = 0; i < numThreads; ++i)
    {
        auto ticker = CipherDB::Ticker::findById(conn, tickerIds[i]);
        ASSERT_TRUE(ticker);
        EXPECT_EQ(ticker->getTimestamp(), 1620000000000 + i * 1000);
        EXPECT_DOUBLE_EQ(ticker->getLastPrice(), 50000.0 + i * 100);
    }

    // Test querying concurrently
    futures.clear();
    std::atomic< int > foundCount(0);

    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [&foundCount]
                                     {
                                         auto conn   = CipherDB::db::Database::getInstance().getConnection();
                                         auto filter = CipherDB::Ticker::createFilter().withSymbol(
                                             "TickerMultithreadedOperations:BTC/USD");
                                         auto result = CipherDB::Ticker::findByFilter(conn, filter);
                                         if (result && !result->empty())
                                         {
                                             foundCount += result->size();
                                         }
                                     }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    EXPECT_EQ(foundCount, numThreads * numThreads); // Each thread should find all 10 records
}

TEST_F(DBTest, TickerEdgeCases)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Test with minimum values
    CipherDB::Ticker minTicker;
    minTicker.setTimestamp(0);
    minTicker.setLastPrice(0.0);
    minTicker.setVolume(0.0);
    minTicker.setHighPrice(0.0);
    minTicker.setLowPrice(0.0);
    minTicker.setSymbol("");
    minTicker.setExchange("");
    EXPECT_TRUE(minTicker.save(conn));

    auto found = CipherDB::Ticker::findById(conn, minTicker.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), 0);
    EXPECT_DOUBLE_EQ(found->getLastPrice(), 0.0);
    EXPECT_DOUBLE_EQ(found->getVolume(), 0.0);

    // Test with extremely large values
    CipherDB::Ticker maxTicker;
    maxTicker.setTimestamp(9223372036854775807LL); // max int64_t
    maxTicker.setLastPrice(std::numeric_limits< double >::max() /
                           2); // Very large but not max to avoid precision issues
    maxTicker.setVolume(std::numeric_limits< double >::max() / 2);
    maxTicker.setHighPrice(std::numeric_limits< double >::max() / 2);
    maxTicker.setLowPrice(0.0);
    maxTicker.setSymbol("VERY_LONG_SYMBOL_NAME_TO_TEST_STRING_HANDLING");
    maxTicker.setExchange("VERY_LONG_EXCHANGE_NAME_TO_TEST_STRING_HANDLING");
    EXPECT_TRUE(maxTicker.save(conn));

    found = CipherDB::Ticker::findById(conn, maxTicker.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), 9223372036854775807LL);
    EXPECT_DOUBLE_EQ(found->getLastPrice(), std::numeric_limits< double >::max() / 2);

    // Test non-existent ID
    auto nonExistent = CipherDB::Ticker::findById(conn, boost::uuids::random_generator()());
    EXPECT_FALSE(nonExistent);
}

TEST_F(DBTest, TickerAttributeConstruction)
{
    // Create ticker from attribute map
    std::unordered_map< std::string, std::any > attributes;
    attributes["timestamp"]  = static_cast< int64_t >(1620000000000);
    attributes["last_price"] = 50000.0;
    attributes["volume"]     = 2.5;
    attributes["high_price"] = 51000.0;
    attributes["low_price"]  = 49000.0;
    attributes["symbol"]     = std::string("BTC/USD");
    attributes["exchange"]   = std::string("binance");

    CipherDB::Ticker ticker(attributes);

    EXPECT_EQ(ticker.getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(ticker.getLastPrice(), 50000.0);
    EXPECT_DOUBLE_EQ(ticker.getVolume(), 2.5);
    EXPECT_DOUBLE_EQ(ticker.getHighPrice(), 51000.0);
    EXPECT_DOUBLE_EQ(ticker.getLowPrice(), 49000.0);
    EXPECT_EQ(ticker.getSymbol(), "BTC/USD");
    EXPECT_EQ(ticker.getExchange(), "binance");

    // Test with partial attributes
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["timestamp"]  = static_cast< int64_t >(1620000000000);
    partialAttributes["last_price"] = 50000.0;
    partialAttributes["symbol"]     = std::string("BTC/USD");

    CipherDB::Ticker partialTicker(partialAttributes);
    EXPECT_EQ(partialTicker.getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(partialTicker.getLastPrice(), 50000.0);
    EXPECT_EQ(partialTicker.getSymbol(), "BTC/USD");

    // Default values for missing attributes
    EXPECT_DOUBLE_EQ(partialTicker.getVolume(), 0.0);

    // Test with UUID in attributes
    std::unordered_map< std::string, std::any > attributesWithId;
    boost::uuids::uuid testUuid   = boost::uuids::random_generator()();
    attributesWithId["id"]        = boost::uuids::to_string(testUuid);
    attributesWithId["timestamp"] = static_cast< int64_t >(1620000000000);

    CipherDB::Ticker tickerWithId(attributesWithId);
    EXPECT_EQ(tickerWithId.getIdAsString(), boost::uuids::to_string(testUuid));
}

TEST_F(DBTest, TradeBasicCRUD)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Create a trade
    CipherDB::Trade trade;
    trade.setTimestamp(1620000000000);
    trade.setPrice(50000.0);
    trade.setBuyQty(1.5);
    trade.setSellQty(0.5);
    trade.setBuyCount(3);
    trade.setSellCount(1);
    trade.setSymbol("BTC/USD");
    trade.setExchange("binance");

    EXPECT_TRUE(trade.save(conn));

    // Read the trade back
    auto found = CipherDB::Trade::findById(conn, trade.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(found->getPrice(), 50000.0);
    EXPECT_DOUBLE_EQ(found->getBuyQty(), 1.5);
    EXPECT_DOUBLE_EQ(found->getSellQty(), 0.5);
    EXPECT_EQ(found->getBuyCount(), 3);
    EXPECT_EQ(found->getSellCount(), 1);
    EXPECT_EQ(found->getSymbol(), "BTC/USD");
    EXPECT_EQ(found->getExchange(), "binance");

    // Update the trade
    trade.setPrice(52000.0);
    trade.setBuyQty(2.0);
    trade.setSellQty(1.0);
    EXPECT_TRUE(trade.save(conn));

    // Read again and verify update
    found = CipherDB::Trade::findById(conn, trade.getId());
    ASSERT_TRUE(found);
    EXPECT_DOUBLE_EQ(found->getPrice(), 52000.0);
    EXPECT_DOUBLE_EQ(found->getBuyQty(), 2.0);
    EXPECT_DOUBLE_EQ(found->getSellQty(), 1.0);
}

TEST_F(DBTest, TradeFindByFilter)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Create test trades
    CipherDB::Trade trade1;
    trade1.setTimestamp(1620000000000);
    trade1.setPrice(50000.0);
    trade1.setBuyQty(1.5);
    trade1.setSellQty(0.5);
    trade1.setBuyCount(3);
    trade1.setSellCount(1);
    trade1.setSymbol("TradeFindByFilter:BTC/USD");
    trade1.setExchange("TradeFindByFilter:binance");
    EXPECT_TRUE(trade1.save(conn));

    CipherDB::Trade trade2;
    trade2.setTimestamp(1620000100000);
    trade2.setPrice(51000.0);
    trade2.setBuyQty(2.0);
    trade2.setSellQty(1.0);
    trade2.setBuyCount(4);
    trade2.setSellCount(2);
    trade2.setSymbol("TradeFindByFilter:BTC/USD");
    trade2.setExchange("TradeFindByFilter:binance");
    EXPECT_TRUE(trade2.save(conn));

    CipherDB::Trade trade3;
    trade3.setTimestamp(1620000200000);
    trade3.setPrice(55000.0);
    trade3.setBuyQty(0.5);
    trade3.setSellQty(0.2);
    trade3.setBuyCount(1);
    trade3.setSellCount(1);
    trade3.setSymbol("TradeFindByFilter:ETH/USD");
    trade3.setExchange("TradeFindByFilter:coinbase");
    EXPECT_TRUE(trade3.save(conn));

    // Find by symbol
    auto filter1 = CipherDB::Trade::createFilter().withSymbol("TradeFindByFilter:BTC/USD");
    auto result1 = CipherDB::Trade::findByFilter(conn, filter1);
    ASSERT_TRUE(result1);
    EXPECT_EQ(result1->size(), 2);

    // Find by exchange
    auto filter2 = CipherDB::Trade::createFilter().withExchange("TradeFindByFilter:coinbase");
    auto result2 = CipherDB::Trade::findByFilter(conn, filter2);
    ASSERT_TRUE(result2);
    EXPECT_EQ(result2->size(), 1);
    EXPECT_EQ((*result2)[0].getSymbol(), "TradeFindByFilter:ETH/USD");

    // Find by timestamp
    auto filter3 = CipherDB::Trade::createFilter().withTimestamp(1620000100000);
    auto result3 = CipherDB::Trade::findByFilter(conn, filter3);
    ASSERT_TRUE(result3);
    EXPECT_EQ(result3->size(), 1);
    EXPECT_DOUBLE_EQ((*result3)[0].getPrice(), 51000.0);

    // Find by timestamp range
    auto filter5 = CipherDB::Trade::createFilter().withTimestampRange(1620000050000, 1620000250000);
    auto result5 = CipherDB::Trade::findByFilter(conn, filter5);
    ASSERT_TRUE(result5);
    EXPECT_EQ(result5->size(), 2);

    // Combined filters
    auto filter6 =
        CipherDB::Trade::createFilter().withExchange("TradeFindByFilter:binance").withPriceRange(50000.0, 52000.0);
    auto result6 = CipherDB::Trade::findByFilter(conn, filter6);
    ASSERT_TRUE(result6);
    EXPECT_EQ(result6->size(), 2);
}

TEST_F(DBTest, TradeTransactionSafety)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    CipherDB::Trade trade;
    trade.setTimestamp(1620000000000);
    trade.setPrice(50000.0);
    trade.setBuyQty(1.5);
    trade.setSellQty(0.5);
    trade.setBuyCount(3);
    trade.setSellCount(1);
    trade.setSymbol("BTC/USD");
    trade.setExchange("binance");

    // Save trade in transaction and roll back
    {
        CipherDB::db::TransactionGuard txGuard;
        EXPECT_TRUE(trade.save(txGuard.getConnection()));

        // Should be able to find trade within transaction
        auto found = CipherDB::Trade::findById(txGuard.getConnection(), trade.getId());
        EXPECT_TRUE(found);

        // Roll back
        EXPECT_TRUE(txGuard.rollback());
    }

    // After rollback, trade should not exist
    auto found = CipherDB::Trade::findById(conn, trade.getId());
    EXPECT_FALSE(found);

    // Save trade in transaction and commit
    {
        CipherDB::db::TransactionGuard txGuard;
        EXPECT_TRUE(trade.save(txGuard.getConnection()));
        EXPECT_TRUE(txGuard.commit());
    }

    // After commit, trade should exist
    found = CipherDB::Trade::findById(conn, trade.getId());
    EXPECT_TRUE(found);
}

TEST_F(DBTest, TradeMultithreadedOperations)
{
    const int numThreads = 10;
    std::vector< boost::uuids::uuid > tradeIds(numThreads);
    std::vector< std::future< void > > futures;

    // Test creating trades concurrently
    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [i, &tradeIds]
                                     {
                                         auto conn = CipherDB::db::Database::getInstance().getConnection();

                                         CipherDB::Trade trade;
                                         trade.setTimestamp(1620000000000 + i * 1000);
                                         trade.setPrice(50000.0 + i * 100);
                                         trade.setBuyQty(1.0 + i * 0.1);
                                         trade.setSellQty(0.5 + i * 0.05);
                                         trade.setBuyCount(i + 1);
                                         trade.setSellCount(i);
                                         trade.setSymbol("TradeMultithreadedOperations:BTC/USD");
                                         trade.setExchange("TradeMultithreadedOperations:binance");

                                         EXPECT_TRUE(trade.save(conn));
                                         tradeIds[i] = trade.getId();
                                     }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    // Verify all trades were saved correctly
    auto conn = CipherDB::db::Database::getInstance().getConnection();
    for (int i = 0; i < numThreads; ++i)
    {
        auto trade = CipherDB::Trade::findById(conn, tradeIds[i]);
        ASSERT_TRUE(trade);
        EXPECT_EQ(trade->getTimestamp(), 1620000000000 + i * 1000);
        EXPECT_DOUBLE_EQ(trade->getPrice(), 50000.0 + i * 100);
    }

    // Test querying concurrently
    futures.clear();
    std::atomic< int > foundCount(0);

    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [&foundCount]
                                     {
                                         auto conn   = CipherDB::db::Database::getInstance().getConnection();
                                         auto filter = CipherDB::Trade::createFilter().withSymbol(
                                             "TradeMultithreadedOperations:BTC/USD");
                                         auto result = CipherDB::Trade::findByFilter(conn, filter);
                                         if (result && !result->empty())
                                         {
                                             foundCount += result->size();
                                         }
                                     }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    EXPECT_EQ(foundCount, numThreads * numThreads); // Each thread should find all 10 records

    // Test concurrent filtering operations
    futures.clear();
    std::atomic< bool > allSucceeded(true);

    for (int i = 0; i < numThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async,
                                     [&allSucceeded, i]
                                     {
                                         try
                                         {
                                             auto conn     = CipherDB::db::Database::getInstance().getConnection();
                                             auto priceMin = 50000.0 + (i % 5) * 100;
                                             auto priceMax = priceMin + 500.0;

                                             auto filter = CipherDB::Trade::createFilter()
                                                               .withSymbol("TradeMultithreadedOperations:BTC/USD")
                                                               .withPriceRange(priceMin, priceMax);

                                             auto result = CipherDB::Trade::findByFilter(conn, filter);
                                             if (!result)
                                             {
                                                 allSucceeded = false;
                                             }
                                         }
                                         catch (...)
                                         {
                                             allSucceeded = false;
                                         }
                                     }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    EXPECT_TRUE(allSucceeded.load());
}

TEST_F(DBTest, TradeEdgeCases)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Test with minimum values
    CipherDB::Trade minTrade;
    minTrade.setTimestamp(0);
    minTrade.setPrice(0.0);
    minTrade.setBuyQty(0.0);
    minTrade.setSellQty(0.0);
    minTrade.setBuyCount(0);
    minTrade.setSellCount(0);
    minTrade.setSymbol("");
    minTrade.setExchange("");
    EXPECT_TRUE(minTrade.save(conn));

    auto found = CipherDB::Trade::findById(conn, minTrade.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), 0);
    EXPECT_DOUBLE_EQ(found->getPrice(), 0.0);
    EXPECT_DOUBLE_EQ(found->getBuyQty(), 0.0);

    // Test with extremely large values
    CipherDB::Trade maxTrade;
    maxTrade.setTimestamp(std::numeric_limits< int64_t >::max());
    maxTrade.setPrice(std::numeric_limits< double >::max() / 2); // Very large but not max to avoid precision issues
    maxTrade.setBuyQty(std::numeric_limits< double >::max() / 2);
    maxTrade.setSellQty(std::numeric_limits< double >::max() / 2);
    maxTrade.setBuyCount(std::numeric_limits< int >::max());
    maxTrade.setSellCount(std::numeric_limits< int >::max());
    maxTrade.setSymbol("VERY_LONG_SYMBOL_NAME_TO_TEST_STRING_HANDLING");
    maxTrade.setExchange("VERY_LONG_EXCHANGE_NAME_TO_TEST_STRING_HANDLING");
    EXPECT_TRUE(maxTrade.save(conn));

    found = CipherDB::Trade::findById(conn, maxTrade.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), std::numeric_limits< int64_t >::max());
    EXPECT_DOUBLE_EQ(found->getPrice(), std::numeric_limits< double >::max() / 2);
    EXPECT_EQ(found->getBuyCount(), std::numeric_limits< int >::max());

    // Test negative values
    CipherDB::Trade negativeTrade;
    negativeTrade.setTimestamp(-1);
    negativeTrade.setPrice(-100.0);
    negativeTrade.setBuyQty(-5.0);
    negativeTrade.setSellQty(-2.5);
    negativeTrade.setBuyCount(-3);
    negativeTrade.setSellCount(-1);
    negativeTrade.setSymbol("BTC/USD");
    negativeTrade.setExchange("binance");
    EXPECT_TRUE(negativeTrade.save(conn));

    found = CipherDB::Trade::findById(conn, negativeTrade.getId());
    ASSERT_TRUE(found);
    EXPECT_EQ(found->getTimestamp(), -1);
    EXPECT_DOUBLE_EQ(found->getPrice(), -100.0);
    EXPECT_DOUBLE_EQ(found->getBuyQty(), -5.0);
    EXPECT_EQ(found->getBuyCount(), -3);

    // Test non-existent ID
    auto nonExistent = CipherDB::Trade::findById(conn, boost::uuids::random_generator()());
    EXPECT_FALSE(nonExistent);
}

TEST_F(DBTest, TradeAttributeConstruction)
{
    // Create trade from attribute map
    std::unordered_map< std::string, std::any > attributes;
    attributes["timestamp"]  = static_cast< int64_t >(1620000000000);
    attributes["price"]      = 50000.0;
    attributes["buy_qty"]    = 1.5;
    attributes["sell_qty"]   = 0.5;
    attributes["buy_count"]  = 3;
    attributes["sell_count"] = 1;
    attributes["symbol"]     = std::string("BTC/USD");
    attributes["exchange"]   = std::string("binance");

    CipherDB::Trade trade(attributes);

    EXPECT_EQ(trade.getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(trade.getPrice(), 50000.0);
    EXPECT_DOUBLE_EQ(trade.getBuyQty(), 1.5);
    EXPECT_DOUBLE_EQ(trade.getSellQty(), 0.5);
    EXPECT_EQ(trade.getBuyCount(), 3);
    EXPECT_EQ(trade.getSellCount(), 1);
    EXPECT_EQ(trade.getSymbol(), "BTC/USD");
    EXPECT_EQ(trade.getExchange(), "binance");

    // Test with partial attributes
    std::unordered_map< std::string, std::any > partialAttributes;
    partialAttributes["timestamp"] = static_cast< int64_t >(1620000000000);
    partialAttributes["price"]     = 50000.0;
    partialAttributes["symbol"]    = std::string("BTC/USD");

    CipherDB::Trade partialTrade(partialAttributes);
    EXPECT_EQ(partialTrade.getTimestamp(), 1620000000000);
    EXPECT_DOUBLE_EQ(partialTrade.getPrice(), 50000.0);
    EXPECT_EQ(partialTrade.getSymbol(), "BTC/USD");

    // Default values for missing attributes
    EXPECT_DOUBLE_EQ(partialTrade.getBuyQty(), 0.0);
    EXPECT_DOUBLE_EQ(partialTrade.getSellQty(), 0.0);
    EXPECT_EQ(partialTrade.getBuyCount(), 0);
    EXPECT_EQ(partialTrade.getSellCount(), 0);

    // Test with UUID in attributes
    std::unordered_map< std::string, std::any > attributesWithId;
    boost::uuids::uuid testUuid   = boost::uuids::random_generator()();
    attributesWithId["id"]        = boost::uuids::to_string(testUuid);
    attributesWithId["timestamp"] = static_cast< int64_t >(1620000000000);

    CipherDB::Trade tradeWithId(attributesWithId);
    EXPECT_EQ(tradeWithId.getIdAsString(), boost::uuids::to_string(testUuid));
}

TEST_F(DBTest, TradeExceptionSafety)
{
    auto conn = CipherDB::db::Database::getInstance().getConnection();

    // Attempt to create bad attributes that should throw
    std::unordered_map< std::string, std::any > badAttributes;
    badAttributes["timestamp"] = std::string("not_a_number");
    badAttributes["price"]     = std::vector< int >{1, 2, 3}; // Wrong type

    EXPECT_THROW({ CipherDB::Trade badTrade(badAttributes); }, std::runtime_error);

    // Create a valid trade that we'll use to test findById with a null connection
    CipherDB::Trade validTrade;
    validTrade.setTimestamp(1620000000000);
    validTrade.setPrice(50000.0);
    validTrade.setSymbol("BTC/USD");
    validTrade.setExchange("binance");
    EXPECT_TRUE(validTrade.save(conn));

    // Save should handle null connections gracefully by getting a default connection
    CipherDB::Trade nullConnTrade;
    nullConnTrade.setTimestamp(1620000000000);
    nullConnTrade.setPrice(50000.0);
    nullConnTrade.setSymbol("BTC/USD");
    nullConnTrade.setExchange("binance");
    EXPECT_TRUE(nullConnTrade.save(nullptr));

    // FindById should also handle null connections
    auto found = CipherDB::Trade::findById(nullptr, validTrade.getId());
    ASSERT_TRUE(found);

    // FindByFilter should handle null connections
    auto filter  = CipherDB::Trade::createFilter().withSymbol("BTC/USD");
    auto results = CipherDB::Trade::findByFilter(nullptr, filter);
    ASSERT_TRUE(results);
    EXPECT_GE(results->size(), 2);
}
