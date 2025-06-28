#ifndef CIPHER_CACHE_HPP
#define CIPHER_CACHE_HPP

namespace ct
{
namespace cache
{


/**
 * @brief Cache class for storing and retrieving serialized data with expiration
 *
 * This class provides file-based caching functionality with expiration times.
 * It supports storing any serializable data type and manages cache expiration.
 */
class Cache
{
   public:
    /**
     * @brief Construct a new Cache object
     *
     * @param path Directory path where cache files will be stored
     */
    explicit Cache(const std::string& path);

    /**
     * @brief Store a value in the cache with expiration
     *
     * @tparam T Type of data to store
     * @param key Unique identifier for the cached item
     * @param data Data to cache
     * @param ttl Time in seconds until the cache expires (default: 3600)
     */
    template < typename T >
    void setValue(const std::string& key, const T& data, int ttl = 3600);

    /**
     * @brief Retrieve a value from the cache
     *
     * @tparam T Expected type of the cached data
     * @param key Unique identifier for the cached item
     * @return std::optional<T> The cached value or std::nullopt if not found/expired
     */
    template < typename T >
    std::optional< T > getValue(const std::string& key);

    /**
     * @brief Clear all cached items
     */
    void flush();

    /**
     * @brief Decorator for caching method results using std::function
     *
     * @tparam R Return type of the function
     * @tparam Args Argument types of the function
     * @param func Function to cache
     * @return std::function<R(Args...)> Cached function
     */
    template < typename R, typename... Args >
    std::function< R(Args...) > cached(std::function< R(Args...) > func);

    struct Item
    {
        int expire_seconds_;
        std::optional< std::chrono::time_point< std::chrono::system_clock > > expire_at_;
        std::string path_;
    };

   private:
    std::string path_;
    std::string driver_;
    std::unordered_map< std::string, Item > db_;
    std::mutex cache_mutex_;

    /**
     * @brief Update the cache database file
     */
    void updateDb();
};

} // namespace cache
} // namespace ct

namespace cereal
{
template < class Archive >
void save(Archive& archive, const std::chrono::time_point< std::chrono::system_clock >& tp)
{
    // Convert time_point to duration since epoch (in milliseconds)
    auto duration = tp.time_since_epoch();
    auto millis   = std::chrono::duration_cast< std::chrono::milliseconds >(duration).count();
    archive(millis);
}

template < class Archive >
void load(Archive& archive, std::chrono::time_point< std::chrono::system_clock >& tp)
{
    // Load duration in milliseconds and convert back to time_point
    int64_t millis;
    archive(millis);
    tp = std::chrono::time_point< std::chrono::system_clock >(std::chrono::milliseconds(millis));
}

template < class Archive >
void serialize(Archive& archive, ct::cache::Cache::Item& item)
{
    archive(CEREAL_NVP(item.expire_seconds_), // Serialize expire_seconds_ with name
            CEREAL_NVP(item.expire_at_),      // Serialize expire_at_ (optional<time_point>)
            CEREAL_NVP(item.path_)            // Serialize path_
    );
}
} // namespace cereal


#endif // CIPHER_CACHE_HPP
