#include "Cache.hpp"
#include "Config.hpp"

namespace ct
{
namespace cache
{

Cache::Cache(const std::string& path) : path_(path)
{
    // Get cache driver from config
    driver_ = config::Config::getInstance().getValue< std::string >("env.caching.driver", "file");

    if (driver_ == "file")
    {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(path_);

        // Load cache database if it exists
        std::string dbPath = path_ + "cache_database.dat";
        if (std::filesystem::exists(dbPath))
        {
            try
            {
                std::ifstream is(dbPath, std::ios::binary);
                cereal::BinaryInputArchive archive(is);
                archive(db_);
            }
            catch (const std::exception&)
            {
                // File might be corrupted, start with empty database
                db_->clear();
            }
        }
    }
}

template < typename T >
void Cache::setValue(const std::string& key, const T& data, int expire_seconds)
{
    std::lock_guard< std::mutex > lock(*cache_mutex_);

    if (driver_.empty())
    {
        throw std::runtime_error("Caching driver is not set.");
    }

    // Calculate expiration time
    std::optional< std::chrono::time_point< std::chrono::system_clock > > expireAt = std::nullopt;
    if (expire_seconds != 0)
    {
        expireAt = std::chrono::system_clock::now() + std::chrono::seconds(expire_seconds);
    }

    // Create cache item record
    std::string dataPath = path_ + key + ".cache";
    db_->at(key)         = {expire_seconds, expireAt, dataPath};

    // Update database file
    updateDb();

    // Serialize and store data
    try
    {
        std::ofstream os(dataPath, std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        archive(data);
    }
    catch (const std::exception&)
    {
        // TODO:
        // Handle serialization error
    }
}

template < typename T >
std::optional< T > Cache::getValue(const std::string& key)
{
    std::lock_guard< std::mutex > lock(*cache_mutex_);

    if (driver_.empty())
    {
        throw std::runtime_error("Caching driver is not set.");
    }

    // Check if key exists
    auto it = db_->find(key);
    if (it == db_->end())
    {
        return std::nullopt;
    }

    Item& item = it->second;

    // Check if expired
    if (item.expire_at_.has_value() && std::chrono::system_clock::now() > item.expire_at_.value())
    {
        try
        {
            std::filesystem::remove(item.path_);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // File might not exist, ignore
        }
        db_->erase(it);
        updateDb();
        return std::nullopt;
    }

    // Check if file exists
    if (!std::filesystem::exists(item.path_))
    {
        db_->erase(it);
        updateDb();
        return std::nullopt;
    }

    // Renew expiration time
    if (item.expire_at_.has_value())
    {
        item.expire_at_ = std::chrono::system_clock::now() + std::chrono::seconds(item.expire_seconds_);
        updateDb();
    }

    // Deserialize data
    try
    {
        T result;
        std::ifstream is(item.path_, std::ios::binary);
        cereal::BinaryInputArchive archive(is);
        archive(result);
        return result;
    }
    catch (const std::exception&)
    {
        // Handle deserialization error
        try
        {
            std::filesystem::remove(item.path_);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // File might not exist, ignore
        }
        db_->erase(it);
        updateDb();
        return std::nullopt;
    }
}

template < typename R, typename... Args >
std::function< R(Args...) > Cache::cached(std::function< R(Args...) > func)
{
    // Create a unique pointer to a memoization map
    using CacheKey = std::tuple< Args... >;
    using CacheMap = std::unordered_map< CacheKey, R >;

    // Create a shared cache map that will be captured by the lambda
    auto cacheMap = std::make_shared< CacheMap >();

    // Return a function that checks the cache before calling the original function
    return [func, cacheMap](Args... args) -> R
    {
        CacheKey key(args...);

        // Check if result is in cache
        auto it = cacheMap->find(key);
        if (it != cacheMap->end())
        {
            return it->second;
        }

        // Call the function and cache the result
        R result         = func(args...);
        (*cacheMap)[key] = result;
        return result;
    };
}

void Cache::updateDb()
{
    // Save database to file
    std::string dbPath = path_ + "cache_database.dat";
    try
    {
        std::ofstream os(dbPath, std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        archive(db_);
    }
    catch (const std::exception&)
    {
        // TODO:
        // Handle serialization error
    }
}

void Cache::flush()
{
    std::lock_guard< std::mutex > lock(*cache_mutex_);

    if (driver_.empty())
    {
        return;
    }

    auto& db = *db_;

    // Create a copy of keys to avoid iterator invalidation
    std::vector< std::string > keysToRemove;
    for (const auto& [key, _] : db)
    {
        keysToRemove.push_back(key);
    }

    // Remove all cache files and entries
    for (const auto& key : keysToRemove)
    {
        auto it = db.find(key);
        if (it != db.end())
        {
            try
            {
                std::filesystem::remove(it->second.path_);
            }
            catch (const std::filesystem::filesystem_error&)
            {
                // File might not exist, ignore
            }
            db.erase(it);
        }
    }

    // Update database file
    updateDb();
}

} // namespace cache
} // namespace ct
