#ifndef CIPHER_CONFIG_HPP
#define CIPHER_CONFIG_HPP

#include <any>
#include <map>
#include <mutex>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace CipherConfig
{

// Define the variant type for all possible config values
using ConfValue = std::variant< int,
                                bool,
                                double,
                                std::string,
                                std::map< std::string, std::string >,
                                std::vector< std::string >,
                                std::map< std::string, std::variant< int, bool, double, std::string > > >;

class AnyMap
{
   public:
    // Store a value of any type
    template < typename T >
    void store(const std::string& key, T&& value)
    {
        map_[key] = std::forward< T >(value); // Perfect forwarding to avoid copies
    }

    // Retrieve and cast a value to type T
    template < typename T >
    T get(const std::string& key, const T& defaultValue = T()) const
    {
        auto it = map_.find(key);
        if (it == map_.end())
        {
            return defaultValue;
        }
        try
        {
            return std::any_cast< T >(it->second);
        }
        catch (const std::bad_any_cast&)
        {
            return defaultValue; // Return default on type mismatch
        }
    }

    // Check if a key exists
    bool has(const std::string& key) const { return map_.find(key) != map_.end(); }

    // Clear the map
    void clear() { map_.clear(); }

   private:
    std::map< std::string, std::any > map_;
};

// Singleton Config class
class Config
{
   public:
    // Singleton access
    static Config& getInstance();

    // Initialize the config with default path
    void init(const std::string& configPath = "config.yaml");

    // Reload the config (e.g., from environment or file)
    void reload();

    // Get a config value by dot-separated key string
    template < typename T >
    T getValue(const std::string& key, const T& defaultValue = T()) const;

    // Get config value as a variant
    ConfValue get(const std::string& key, const ConfValue& defaultValue = std::string("")) const;

    // Set a config value
    template < typename T >
    void setValue(const std::string& key, const T& value)
    {
        std::lock_guard< std::mutex > lock(configMutex_);

        // Update config
        config_[key] = ConfValue(value);
    }

    // Check if a key exists
    bool hasKey(const std::string& key) const;

    // Deleted to enforce Singleton
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

   private:
    Config() = default; // Private constructor for Singleton

    mutable std::mutex configMutex_;
    std::string configPath_ = "conf.yaml";
    mutable std::map< std::string, ConfValue > config_; // Main config storage

    // AnyMap cache_; // Cache for computed values

    // Save current configuration to YAML file
    bool saveToFile(const std::string& filePath = "") const;

    // Load configuration from YAML file
    bool loadFromFile(const std::string& filePath);

    // Helper methods
    ConfValue fetchValue(const std::string& key, const ConfValue& defaultValue) const;
    void setNestedValue(const std::string& key, const ConfValue& value);
    std::vector< std::string > splitPath(const std::string& path) const;

    // YAML handling methods
    bool parseYamlNode(const YAML::Node& node);
    YAML::Node configToYamlNode() const;
    ConfValue yamlNodeToConfValue(const YAML::Node& node) const;

    // Adds default values to the configuration
    void setDefaults();
};

} // namespace CipherConfig

#endif
