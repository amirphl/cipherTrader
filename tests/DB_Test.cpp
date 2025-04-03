#include "DB.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <gtest/gtest.h>
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
                conn->commit_transaction();

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
            // Create a transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            auto result = CipherDB::Candle::findById(conn, candleIds[index]);
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
            // Create a transaction guard
            CipherDB::db::TransactionGuard txGuard;
            auto conn = txGuard.getConnection();

            auto filter = CipherDB::Candle::createFilter()
                              .withExchange("CandleMultithreadedOperations:thread_test")
                              .withSymbol("BTC/USD")
                              .withTimeframe("1h");
            auto result = CipherDB::Candle::findByFilter(conn, filter);
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
