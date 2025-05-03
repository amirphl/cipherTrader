// In one of your test files (or a separate setup.cpp)
#include "Logger.hpp"
#include <gtest/gtest.h>

class GlobalEnvironment : public ::testing::Environment
{
   public:
    void SetUp() override
    {
        // Global initialization here
        std::cout << "Global setup for all tests" << std::endl;

        ct::logger::LOG.init("CipherTrader",
                             ct::logger::LogLevel::Debug, // Set global level to Debug
                             true,                        // Enable console output
                             true,                        // Enable stderr output
                             false,                       // Disable file output
                             "logs/cipher_trader_tests.log");
    }

    void TearDown() override
    {
        // Global cleanup here
        std::cout << "Global cleanup after all tests" << std::endl;
    }
};

// Register the environment - should be in the same file
// This needs to be outside of any namespace
::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new GlobalEnvironment());
