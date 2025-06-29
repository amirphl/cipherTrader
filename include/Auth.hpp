#ifndef CIPHER_AUTH_HPP
#define CIPHER_AUTH_HPP

namespace ct
{
namespace auth
{

/**
 * @brief Authentication service for handling tokens and authorization
 */
class Auth
{
   public:
    /**
     * @brief Get the singleton instance of the Auth service
     * @return Reference to the Auth instance
     */
    static Auth& getInstance();

    /**
     * @brief Convert password to authentication token
     * @param password The password to convert
     * @return JSON response with token or error message
     */
    nlohmann::json passwordToToken(const std::string& password) const;

    /**
     * @brief Validate if a token is valid
     * @param authToken The token to validate
     * @return True if token is valid, false otherwise
     */
    bool isValidToken(const std::string& authToken) const;

    /**
     * @brief Get unauthorized response
     * @return JSON response with error message
     */
    nlohmann::json genUnauthorizedResponse() const;

    /**
     * @brief Get access token for license API
     * @return Optional string containing the token if available
     */
    std::optional< std::string > getAccessToken() const;

   private:
    Auth()  = default; // Private constructor for Singleton
    ~Auth() = default;

    // Deleted to enforce Singleton
    Auth(const Auth&)            = delete;
    Auth& operator=(const Auth&) = delete;
};

} // namespace auth
} // namespace ct

#endif // CIPHER_AUTH_HPP
