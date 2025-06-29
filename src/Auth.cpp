#include "Auth.hpp"
#include "Config.hpp"
#include "Helper.hpp"

namespace ct
{
namespace auth
{

Auth& Auth::getInstance()
{
    static Auth instance;
    return instance;
}

nlohmann::json Auth::passwordToToken(const std::string& password) const
{
    // Get password from environment values
    auto& config               = config::Config::getInstance();
    std::string storedPassword = config.getValue< std::string >("PASSWORD", "");

    if (password != storedPassword)
    {
        return genUnauthorizedResponse();
    }

    auto authToken = helper::computeSecureHash(password);

    // Create JSON response
    nlohmann::json response = {{"auth_token", authToken}};

    return response;
}

bool Auth::isValidToken(const std::string& authToken) const
{
    // Get password from environment values
    auto& config               = config::Config::getInstance();
    std::string storedPassword = config.getValue< std::string >("PASSWORD", "");

    auto h = helper::computeSecureHash(storedPassword);

    // Compare hashed password with provided token
    return authToken == h;
}

nlohmann::json Auth::genUnauthorizedResponse() const
{
    return {{"message", "Invalid password"}, {"status_code", 401}};
}

std::optional< std::string > Auth::getAccessToken() const
{
    auto& config = config::Config::getInstance();

    // Check if LICENSE_API_TOKEN exists in environment values
    if (!config.hasKey("LICENSE_API_TOKEN"))
    {
        return std::nullopt;
    }

    std::string token = config.getValue< std::string >("LICENSE_API_TOKEN", "");
    if (token.empty())
    {
        return std::nullopt;
    }

    return token;
}

} // namespace auth
} // namespace ct
