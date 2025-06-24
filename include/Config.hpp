#ifndef CIPHER_CONFIG_HPP
#define CIPHER_CONFIG_HPP

#include "Precompiled.hpp"

namespace ct
{
namespace config
{

// Define the variant type for all possible config values
using Value = std::variant< int,
                            size_t,
                            bool,
                            short,
                            double,
                            std::string,
                            std::map< std::string, std::string >,
                            std::vector< std::string >,
                            std::map< std::string, std::variant< int, bool, double, std::string > > >;

class AnyMap;

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
    Value get(const std::string& key, const Value& defaultValue = std::string("")) const;

    // TODO: Move to src/
    // Set a config value
    template < typename T >
    void setValue(const std::string& key, const T& value)
    {
        std::lock_guard< std::mutex > lock(configMutex_);

        // Update config
        config_[key] = Value(value);
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
    mutable std::map< std::string, Value > config_; // Main config storage

    // AnyMap cache_; // Cache for computed values

    // Save current configuration to YAML file
    bool saveToFile(const std::string& filePath = "") const;

    // Load configuration from YAML file
    bool loadFromFile(const std::string& filePath);

    // Helper methods
    Value fetchValue(const std::string& key, const Value& defaultValue) const;
    void setNestedValue(const std::string& key, const Value& value);
    std::vector< std::string > splitPath(const std::string& path) const;

    // YAML handling methods
    bool parseYamlNode(const YAML::Node& node);
    YAML::Node configToYamlNode() const;
    Value yamlNodeToConfValue(const YAML::Node& node) const;

    // Adds default values to the configuration
    void setDefaults();
};

} // namespace config
} // namespace ct

#endif
