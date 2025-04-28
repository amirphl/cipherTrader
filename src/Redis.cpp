// RedisService.cpp
#include "Redis.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "Config.hpp"
#include "Helper.hpp"
#include "Logger.hpp"
#include <sw/redis++/redis++.h>

ct::redis::Redis& ct::redis::Redis::getInstance()
{
    static Redis instance;
    return instance;
}

void ct::redis::Redis::init()
{
    if (initialized_)
        return;

    if (!helper::isCiphertraderProject())
        return;

    try
    {
        auto& config = config::Config::getInstance();

        std::string redisHost     = config.getValue< std::string >("env.REDIS_HOST", "localhost");
        int redisPort             = config.getValue< int >("env.REDIS_PORT", 6379);
        std::string redisPassword = config.getValue< std::string >("env.REDIS_PASSWORD", "");
        int redisDb               = config.getValue< int >("env.REDIS_DB", 0);

        // Create connection URI
        std::string connectionUri = "tcp://" + redisHost + ":" + std::to_string(redisPort);

        // Connection options
        sw::redis::ConnectionOptions options;
        options.host = redisHost;
        options.port = redisPort;
        options.db   = redisDb;

        if (!redisPassword.empty())
        {
            options.password = redisPassword;
        }

        // Create Redis connection
        syncRedis_   = std::make_unique< sw::redis::Redis >(options);
        initialized_ = true;
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Redis connection error: " << e.what();
        logger::LOG.error(oss.str());

        throw;
    }
}

void ct::redis::Redis::syncPublish(const std::string& event, const nlohmann::json& msg, bool compression)
{
    if (helper::isUnitTesting())
    {
        throw std::runtime_error("syncPublish() should NOT be called during testing. There must be something wrong");
    }

    if (!initialized_ || !syncRedis_)
    {
        logger::LOG.error("Redis not initialized");
        return;
    }

    try
    {
        auto& config        = config::Config::getInstance();
        int16_t appPort     = config.getValue< int16_t >("APP_PORT", 8888);
        std::string channel = std::to_string(appPort) + ":channel:1";

        nlohmann::json payload;

        if (compression)
        {
            // Encode the compressed message using Base64
            payload = helper::compressedResponse(msg.dump());
        }
        else
        {
            payload["data"] = msg;
        }

        payload["id"]            = getpid();
        payload["event"]         = helper::getAppMode() + "." + event;
        payload["is_compressed"] = compression;

        syncRedis_->publish(channel, payload.dump());
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Redis publish error: " << e.what();
        logger::LOG.error(oss.str());
    }
}

bool ct::redis::Redis::isProcessActive(const std::string& clientId)
{
    if (helper::isUnitTesting())
    {
        return false;
    }

    if (!initialized_ || !syncRedis_)
    {
        return false;
    }

    try
    {
        auto& config    = config::Config::getInstance();
        int16_t appPort = config.getValue< int16_t >("APP_PORT", 8888);
        std::string key = std::to_string(appPort) + "|active-processes";

        return syncRedis_->sismember(key, clientId) == 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Redis isProcessActive error: " << e.what() << std::endl;
        return false;
    }
}

ct::redis::Redis::~Redis()
{
    // Clean up resources if needed
}
