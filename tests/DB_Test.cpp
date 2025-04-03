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
        adminConfig.debug    = true;
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
        adminConfig.debug    = true;
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
