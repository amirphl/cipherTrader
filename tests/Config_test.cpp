#include "Config.hpp"
#include <gtest/gtest.h>

class ConfigTest : public ::testing::Test
{
   protected:
    void SetUp() override { Config::Config::getInstance().init(); }

    void TearDown() override
    {
        Config::Config::getInstance().reload(); // Reset state
    }
};

// --- initConfig Tests ---
TEST_F(ConfigTest, InitConfigSetsDefaults)
{
    EXPECT_TRUE(std::get< bool >(Config::Config::getInstance().get("env.logging.order_submission")));
    EXPECT_EQ(std::get< int >(Config::Config::getInstance().get("env.data.warmup_candles_num")), 240);
    EXPECT_EQ(std::get< std::string >(Config::Config::getInstance().get("env.caching.driver")), "yaml");
}

TEST_F(ConfigTest, ReloadConfigPreservesCacheWhenTesting)
{
    Config::Config::getInstance().get("env.logging.order_submission"); // Cache it
    ASSERT_TRUE(Config::Config::getInstance().isCached("env.logging.order_submission"));

    Config::Config::getInstance().reload(true);

    ASSERT_FALSE(Config::Config::getInstance().isCached("env.logging.order_submission"));
}

// --- getConfig Tests ---
TEST_F(ConfigTest, GetConfigNormalBool)
{
    auto value = Config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}

TEST_F(ConfigTest, GetConfigNormalInt)
{
    auto value = Config::Config::getInstance().get("env.data.warmup_candles_num", 0);
    EXPECT_EQ(std::get< int >(value), 240);
}

TEST_F(ConfigTest, GetConfigNormalDouble)
{
    auto value = Config::Config::getInstance().get("env.exchanges.SANDBOX.balance", 0.0);
    EXPECT_EQ(std::get< double >(value), 10000.0);
}

TEST_F(ConfigTest, GetConfigNormalString)
{
    auto value = Config::Config::getInstance().get("env.caching.driver", std::string("none"));
    EXPECT_EQ(std::get< std::string >(value), "yaml");
}

TEST_F(ConfigTest, GetConfigNormalVector)
{
    auto value = Config::Config::getInstance().get("app.considering_symbols", std::vector< std::string >{});
    EXPECT_TRUE(std::get< std::vector< std::string > >(value).empty());
}

TEST_F(ConfigTest, GetConfigNormalMap)
{
    auto value = Config::Config::getInstance().get("app.live_drivers", std::map< std::string, std::string >{});
    EXPECT_TRUE((std::get< std::map< std::string, std::string > >(value)).empty());
}

// Edge Cases
TEST_F(ConfigTest, GetConfigEmptyKeyThrows)
{
    EXPECT_THROW(Config::Config::getInstance().get("", true), std::invalid_argument);
}

TEST_F(ConfigTest, GetConfigInvalidKeyReturnsDefault)
{
    auto value = Config::Config::getInstance().get("invalid.key", 42);
    EXPECT_EQ(std::get< int >(value), 42);
}

TEST_F(ConfigTest, GetConfigNestedInvalidReturnsDefault)
{
    auto value = Config::Config::getInstance().get("env.exchanges.NONEXISTENT.fee", 999);
    EXPECT_EQ(std::get< int >(value), 999);
}

TEST_F(ConfigTest, GetConfigEnvOverride)
{
    setenv("ENV_LOGGING_ORDER_SUBMISSION", "false", 1);
    auto value = Config::Config::getInstance().get("env.logging.order_submission", true);
    EXPECT_FALSE(std::get< bool >(value));
    unsetenv("ENV_LOGGING_ORDER_SUBMISSION");
}

TEST_F(ConfigTest, GetConfigEnvOverrideInt)
{
    setenv("ENV_DATA_WARMUP_CANDLES_NUM", "500", 1);
    auto value = Config::Config::getInstance().get("env.data.warmup_candles_num", 0);
    EXPECT_EQ(std::get< int >(value), 500);
    unsetenv("ENV_DATA_WARMUP_CANDLES_NUM");
}

TEST_F(ConfigTest, GetConfigCacheHit)
{
    auto value1 = Config::Config::getInstance().get("env.logging.order_submission", false);
    auto value2 = Config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_EQ(std::get< bool >(value1), std::get< bool >(value2));
    EXPECT_TRUE(Config::Config::getInstance().isCached("env.logging.order_submission")); // Cached after first call
}

TEST_F(ConfigTest, GetConfigNoCacheInUnitTest)
{
    auto value = Config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}
