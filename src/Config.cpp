#include "Config.hpp"
#include <filesystem>
#include <fstream>
#include "Logger.hpp"

ct::config::Config& ct::config::Config::getInstance()
{
    static Config instance;
    return instance;
}

void ct::config::Config::init(const std::string& configPath)
{
    std::lock_guard< std::mutex > lock(configMutex_);

    // Set defaults first
    setDefaults();

    // Try to load from file
    if (std::filesystem::exists(configPath))
    {
        configPath_ = configPath;
        loadFromFile(configPath_);
    }
    else
    {
        std::ostringstream oss;
        oss << "Config file not found at " << configPath << ". Using defaults.";
        ct::logger::LOG.error(oss.str());

        saveToFile(configPath_); // Create default config file
    }
}

void ct::config::Config::reload()
{
    std::lock_guard< std::mutex > lock(configMutex_);

    if (std::filesystem::exists(configPath_))
    {
        loadFromFile(configPath_);
    }
}

ct::config::Value ct::config::Config::get(const std::string& key, const Value& defaultValue) const
{
    std::string k = key;
    std::replace(k.begin(), k.end(), '.', '_');
    std::replace(k.begin(), k.end(), '-', '_');
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);

    std::lock_guard< std::mutex > lock(configMutex_);

    if (config_.count(k))
    {
        return config_.at(k);
    }
    else
    {
        return config_[k] = defaultValue;
    }
}

template < typename T >
T ct::config::Config::getValue(const std::string& key, const T& defaultValue) const
{
    Value result = get(key, Value(defaultValue));

    try
    {
        return std::get< T >(result);
    }
    catch (const std::bad_variant_access&)
    {
        throw;
    }
}

bool ct::config::Config::hasKey(const std::string& key) const
{
    std::lock_guard< std::mutex > lock(configMutex_);
    return config_.find(key) != config_.end();
}

bool ct::config::Config::saveToFile(const std::string& filePath) const
{
    std::string path = filePath.empty() ? configPath_ : filePath;

    try
    {
        YAML::Node root = configToYamlNode();
        std::ofstream file(path);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << "Failed to open file for writing: " << path;
            ct::logger::LOG.error(oss.str());

            return false;
        }

        // class Guard
        // {
        //     ~Guard() { file.close(); }
        // };

        file << YAML::Dump(root);
        file.close(); // FIXME:Defer

        return true;
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error saving config to file: " << e.what();
        ct::logger::LOG.error(oss.str());

        return false;
    }
}

bool ct::config::Config::loadFromFile(const std::string& filePath)
{
    try
    {
        YAML::Node root = YAML::LoadFile(filePath);

        // Clear existing config
        config_.clear();

        // Parse YAML into config map
        return parseYamlNode(root);
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Error loading config from file: " << e.what();
        ct::logger::LOG.error(oss.str());

        return false;
    }
}

bool ct::config::Config::parseYamlNode(const YAML::Node& node)
{
    if (!node.IsMap())
    {
        return false;
    }

    for (const auto& pair : node)
    {
        std::string key = pair.first.as< std::string >();
        config_[key]    = yamlNodeToConfValue(pair.second);
    }

    return true;
}

YAML::Node ct::config::Config::configToYamlNode() const
{
    YAML::Node root;

    for (const auto& [key, value] : config_)
    {
        // TODO:
        // if (std::holds_alternative< std::map< std::string, ConfValue > >(value))
        // {
        //     YAML::Node childNode;
        //     // Convert nested map to YAML node
        //     // (You can implement a recursive function here if needed)
        //     // For simplicity, we will just skip nested maps in this example
        // }
        if (std::holds_alternative< int >(value))
        {
            root[key] = std::get< int >(value);
        }
        else if (std::holds_alternative< bool >(value))
        {
            root[key] = std::get< bool >(value);
        }
        else if (std::holds_alternative< double >(value))
        {
            root[key] = std::get< double >(value);
        }
        else if (std::holds_alternative< std::string >(value))
        {
            root[key] = std::get< std::string >(value);
        }
        else if (std::holds_alternative< std::vector< std::string > >(value))
        {
            YAML::Node arrayNode;
            for (const auto& item : std::get< std::vector< std::string > >(value))
            {
                arrayNode.push_back(item);
            }
            root[key] = arrayNode;
        }
    }

    return root;
}

ct::config::Value ct::config::Config::yamlNodeToConfValue(const YAML::Node& node) const
{
    if (node.IsScalar())
    {
        // Attempt to determine the type and convert
        try
        {
            if (node.Tag() == "!")
            {
                return node.as< std::string >();
            }

            // Try to parse as different types
            if (node.as< std::string >() == "true" || node.as< std::string >() == "false")
            {
                return node.as< bool >();
            }

            // Check if it's an integer
            try
            {
                int intValue = node.as< int >();
                return intValue;
            }
            catch (...)
            {
                // Not an integer, try double
                try
                {
                    double doubleValue = node.as< double >();
                    return doubleValue;
                }
                catch (...)
                {
                    // Default to string
                    return node.as< std::string >();
                }
            }
        }
        catch (...)
        {
            return std::string();
        }
    }
    else if (node.IsSequence())
    {
        std::vector< std::string > result;
        for (const auto& item : node)
        {
            if (item.IsScalar())
            {
                result.push_back(item.as< std::string >());
            }
        }
        return result;
    }
    else if (node.IsMap())
    {
        std::map< std::string, std::string > result;
        for (const auto& pair : node)
        {
            if (pair.second.IsScalar())
            {
                result[pair.first.as< std::string >()] = pair.second.as< std::string >();
            }
        }
        return result;
    }

    return std::string();
}

void ct::config::Config::setDefaults()
{
    // Set default values directly in the config map
    config_["env_logging_order_submission"]         = true;
    config_["env_logging_order_cancellation"]       = true;
    config_["env_logging_order_execution"]          = true;
    config_["env_logging_position_opened"]          = true;
    config_["env_logging_position_increased"]       = true;
    config_["env_logging_position_reduced"]         = true;
    config_["env_logging_position_closed"]          = true;
    config_["env_logging_shorter_period_candles"]   = false;
    config_["env_logging_trading_candles"]          = true;
    config_["env_logging_balance_update"]           = true;
    config_["env_logging_exchange_ws_reconnection"] = true;

    // Default exchange settings
    config_["env_exchanges_sandbox_fee"]                   = 0;
    config_["env_exchanges_sandbox_type"]                  = std::string("futures");
    config_["env_exchanges_sandbox_futures_leverage_mode"] = std::string("cross");
    config_["env_exchanges_sandbox_futures_leverage"]      = 1;
    config_["env_exchanges_sandbox_balance"]               = 10000.0;

    // Optimization settings
    config_["env_optimization_ratio"] = std::string("sharpe");

    // Data settings
    config_["env_data_warmup_candles_num"]       = 240;
    config_["env_data_generate_candles_from_1m"] = false;
    config_["env_data_persistency"]              = true;

    // Notifications settings
    config_["env_notifications_events_submitted_orders"] = true;
    config_["env_notifications_events_cancelled_orders"] = true;
    config_["env_notifications_events_executed_orders"]  = true;

    // App settings
    config_["app_considering_symbols"]    = std::vector< std::string >{};
    config_["app_trading_symbols"]        = std::vector< std::string >{};
    config_["app_considering_timeframes"] = std::vector< std::string >{};
    config_["app_trading_timeframes"]     = std::vector< std::string >{};
    config_["app_considering_exchanges"]  = std::vector< std::string >{};
    config_["app_trading_exchanges"]      = std::vector< std::string >{};
    config_["app_considering_candles"]    = std::vector< std::string >{};
    config_["app_live_drivers"]           = std::map< std::string, std::string >{};
    config_["app_trading_mode"]           = std::string("backtest");
    config_["app_debug_mode"]             = false;
    config_["app_is_unit_testing"]        = false;

    config_["env_redis_host"]     = "localhost";
    config_["env_redis_port"]     = 6379;
    config_["env_redis_db"]       = 0;
    config_["env_redis_password"] = "";

    config_["env_postgres_host"]     = "localhost";
    config_["env_postgres_port"]     = 5432;
    config_["env_postgres_db"]       = "cipherTraderDemo";
    config_["env_postgres_username"] = "postgres";
    config_["env_postgres_password"] = "postgres";

    config_["app_port"] = int16_t(8888);
}

// Explicit template instantiations for common types
template int ct::config::Config::getValue< int >(const std::string&, const int&) const;
template bool ct::config::Config::getValue< bool >(const std::string&, const bool&) const;
template double ct::config::Config::getValue< double >(const std::string&, const double&) const;
template std::string ct::config::Config::getValue< std::string >(const std::string&, const std::string&) const;
template std::vector< std::string > ct::config::Config::getValue< std::vector< std::string > >(
    const std::string&, const std::vector< std::string >&) const;
