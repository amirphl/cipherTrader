#include "Config.hpp"
#include <gtest/gtest.h>

class ConfigTest : public ::testing::Test
{
   protected:
    void SetUp() override { CipherConfig::Config::getInstance().init(); }

    void TearDown() override
    {
        CipherConfig::Config::getInstance().reload(); // Reset state
    }
};

// --- initConfig Tests ---
TEST_F(ConfigTest, InitConfigSetsDefaults)
{
    EXPECT_TRUE(std::get< bool >(CipherConfig::Config::getInstance().get("env.logging.order_submission")));
    EXPECT_EQ(std::get< int >(CipherConfig::Config::getInstance().get("env.data.warmup_candles_num")), 240);
    EXPECT_EQ(std::get< std::string >(CipherConfig::Config::getInstance().get("env.caching.driver")), "yaml");
}

TEST_F(ConfigTest, ReloadConfigPreservesCacheWhenTesting)
{
    CipherConfig::Config::getInstance().get("env.logging.order_submission"); // Cache it
    ASSERT_TRUE(CipherConfig::Config::getInstance().isCached("env.logging.order_submission"));

    CipherConfig::Config::getInstance().reload(true);

    ASSERT_FALSE(CipherConfig::Config::getInstance().isCached("env.logging.order_submission"));
}

// --- getConfig Tests ---
TEST_F(ConfigTest, GetConfigNormalBool)
{
    auto value = CipherConfig::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}

TEST_F(ConfigTest, GetConfigNormalInt)
{
    auto value = CipherConfig::Config::getInstance().get("env.data.warmup_candles_num", 0);
    EXPECT_EQ(std::get< int >(value), 240);
}

TEST_F(ConfigTest, GetConfigNormalDouble)
{
    auto value = CipherConfig::Config::getInstance().get("env.exchanges.SANDBOX.balance", 0.0);
    EXPECT_EQ(std::get< double >(value), 10000.0);
}

TEST_F(ConfigTest, GetConfigNormalString)
{
    auto value = CipherConfig::Config::getInstance().get("env.caching.driver", std::string("none"));
    EXPECT_EQ(std::get< std::string >(value), "yaml");
}

TEST_F(ConfigTest, GetConfigNormalVector)
{
    auto value = CipherConfig::Config::getInstance().get("app.considering_symbols", std::vector< std::string >{});
    EXPECT_TRUE(std::get< std::vector< std::string > >(value).empty());
}

TEST_F(ConfigTest, GetConfigNormalMap)
{
    auto value = CipherConfig::Config::getInstance().get("app.live_drivers", std::map< std::string, std::string >{});
    EXPECT_TRUE((std::get< std::map< std::string, std::string > >(value)).empty());
}

// Edge Cases
TEST_F(ConfigTest, GetConfigEmptyKeyThrows)
{
    EXPECT_THROW(CipherConfig::Config::getInstance().get("", true), std::invalid_argument);
}

TEST_F(ConfigTest, GetConfigInvalidKeyReturnsDefault)
{
    auto value = CipherConfig::Config::getInstance().get("invalid.key", 42);
    EXPECT_EQ(std::get< int >(value), 42);
}

TEST_F(ConfigTest, GetConfigNestedInvalidReturnsDefault)
{
    auto value = CipherConfig::Config::getInstance().get("env.exchanges.NONEXISTENT.fee", 999);
    EXPECT_EQ(std::get< int >(value), 999);
}

TEST_F(ConfigTest, GetConfigEnvOverride)
{
    setenv("ENV_LOGGING_ORDER_SUBMISSION", "false", 1);
    auto value = CipherConfig::Config::getInstance().get("env.logging.order_submission", true);
    EXPECT_FALSE(std::get< bool >(value));
    unsetenv("ENV_LOGGING_ORDER_SUBMISSION");
}

TEST_F(ConfigTest, GetConfigEnvOverrideInt)
{
    setenv("ENV_DATA_WARMUP_CANDLES_NUM", "500", 1);
    auto value = CipherConfig::Config::getInstance().get("env.data.warmup_candles_num", 0);
    EXPECT_EQ(std::get< int >(value), 500);
    unsetenv("ENV_DATA_WARMUP_CANDLES_NUM");
}

TEST_F(ConfigTest, GetConfigCacheHit)
{
    auto value1 = CipherConfig::Config::getInstance().get("env.logging.order_submission", false);
    auto value2 = CipherConfig::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_EQ(std::get< bool >(value1), std::get< bool >(value2));
    EXPECT_TRUE(
        CipherConfig::Config::getInstance().isCached("env.logging.order_submission")); // Cached after first call
}

TEST_F(ConfigTest, GetConfigNoCacheInUnitTest)
{
    auto value = CipherConfig::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}
