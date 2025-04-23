#include "Config.hpp"
#include <gtest/gtest.h>

class ConfigTest : public ::testing::Test
{
   protected:
    void SetUp() override { ct::config::Config::getInstance().init(); }

    void TearDown() override
    {
        ct::config::Config::getInstance().reload(); // Reset state
    }
};

// --- initConfig Tests ---
TEST_F(ConfigTest, InitConfigSetsDefaults)
{
    EXPECT_TRUE(std::get< bool >(ct::config::Config::getInstance().get("env.logging.order_submission")));
    EXPECT_EQ(std::get< int >(ct::config::Config::getInstance().get("env.data.warmup_candles_num")), 240);
}

// --- getConfig Tests ---
TEST_F(ConfigTest, GetConfigNormalBool)
{
    auto value = ct::config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}

TEST_F(ConfigTest, GetConfigNormalInt)
{
    auto value = ct::config::Config::getInstance().get("env.data.warmup_candles_num", 0);
    EXPECT_EQ(std::get< int >(value), 240);
}

TEST_F(ConfigTest, GetConfigNormalDouble)
{
    auto value = ct::config::Config::getInstance().get("env.exchanges.SANDBOX.balance", 0.0);
    EXPECT_EQ(std::get< double >(value), 10000.0);
}

TEST_F(ConfigTest, GetConfigNormalString)
{
    auto value = ct::config::Config::getInstance().get("env.caching.driver", std::string("none"));
    EXPECT_EQ(std::get< std::string >(value), "none");
}

TEST_F(ConfigTest, GetConfigNormalVector)
{
    auto value = ct::config::Config::getInstance().get("app.considering_symbols", std::vector< std::string >{});
    EXPECT_TRUE(std::get< std::vector< std::string > >(value).empty());
}

TEST_F(ConfigTest, GetConfigNormalMap)
{
    auto value = ct::config::Config::getInstance().get("app.live_drivers", std::map< std::string, std::string >{});
    EXPECT_TRUE((std::get< std::map< std::string, std::string > >(value)).empty());
}

// Edge Cases
TEST_F(ConfigTest, GetConfigEmptyKey)
{
    EXPECT_TRUE(ct::config::Config::getInstance().getValue< bool >("", true));
}

TEST_F(ConfigTest, GetConfigInvalidKeyReturnsDefault)
{
    auto value = ct::config::Config::getInstance().get("invalid.key", 42);
    EXPECT_EQ(std::get< int >(value), 42);
}

TEST_F(ConfigTest, GetConfigNestedInvalidReturnsDefault)
{
    auto value = ct::config::Config::getInstance().get("env.exchanges.NONEXISTENT.fee", 999);
    EXPECT_EQ(std::get< int >(value), 999);
}

TEST_F(ConfigTest, GetConfigCacheHit)
{
    auto value1 = ct::config::Config::getInstance().get("env.logging.order_submission", false);
    auto value2 = ct::config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_EQ(std::get< bool >(value1), std::get< bool >(value2));
}

TEST_F(ConfigTest, GetConfigNoCacheInUnitTest)
{
    auto value = ct::config::Config::getInstance().get("env.logging.order_submission", false);
    EXPECT_TRUE(std::get< bool >(value));
}
