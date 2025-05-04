#ifndef CIPHER_REDIS_HPP
#define CIPHER_REDIS_HPP

#include "Precompiled.hpp"

namespace ct
{
namespace redis
{

class Redis
{
   public:
    static Redis& getInstance();

    // Initialize the Redis connection
    void init();

    // Publish message synchronously
    void syncPublish(const std::string& event, const nlohmann::json& msg, bool compression = false);

    // Check if a process is active
    bool isProcessActive(const std::string& clientId);

    // Delete copy/move constructors and assignment operators
    Redis(const Redis&)            = delete;
    Redis& operator=(const Redis&) = delete;
    Redis(Redis&&)                 = delete;
    Redis& operator=(Redis&&)      = delete;

   private:
    Redis() = default;
    ~Redis();

    std::unique_ptr< sw::redis::Redis > syncRedis_;
    bool initialized_ = false;
};

} // namespace redis
} // namespace ct

#endif // CIPHER_REDIS_HPP
