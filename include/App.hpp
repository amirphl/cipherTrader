#pragma once

#include "DB.hpp"

namespace ct
{

/**
 * @brief Application state management class
 *
 * This class maintains the global state of the application including
 * time tracking, session information, and API keys for exchanges and notifications.
 */
class AppState
{
   public:
    /**
     * @brief Get the singleton instance
     *
     * @return AppState& Reference to the singleton instance
     */
    static AppState& getInstance()
    {
        static AppState instance;
        return instance;
    }

    /**
     * @brief Set the session ID
     *
     * @param sessionId The session identifier
     */
    void setSessionId(const std::string& sessionId);

    /**
     * @brief Set the exchange API key
     *
     * @param exchangeApiKeyId The exchange API key identifier
     * @throws std::invalid_argument If exchange API key has already been set
     */
    void setExchangeApiKey(const boost::uuids::uuid& exchangeApiKeyId);

    /**
     * @brief Set the notifications API key
     *
     * @param notificationsApiKeyId The notifications API key identifier
     * @throws std::invalid_argument If notifications API key has already been set
     */
    void setNotificationsApiKey(const boost::uuids::uuid& notificationsApiKeyId);

    // Getters
    int64_t getTime() const { return time_; }
    std::optional< int64_t > getStartingTime() const { return startingTime_; }
    std::optional< int64_t > getEndingTime() const { return endingTime_; }
    const std::vector< double >& getDailyBalance() const { return dailyBalance_; }
    int getTotalOpenTrades() const { return totalOpenTrades_; }
    double getTotalOpenPl() const { return totalOpenPl_; }
    int getTotalLiquidations() const { return totalLiquidations_; }
    const std::string& getSessionId() const { return sessionId_; }
    const std::unordered_map< std::string, std::string >& getSessionInfo() const { return sessionInfo_; }
    const std::optional< db::ExchangeApiKeys >& getExchangeApiKey() const { return exchangeApiKey_; }
    const std::optional< db::NotificationApiKeys >& getNotificationsApiKey() const { return notificationsApiKey_; }

    // Setters
    void setTime(int64_t time) { time_ = time; }
    void setStartingTime(int64_t startingTime) { startingTime_ = startingTime; }
    void setEndingTime(int64_t endingTime) { endingTime_ = endingTime; }
    void setDailyBalance(const std::vector< double >& dailyBalance) { dailyBalance_ = dailyBalance; }
    void setTotalOpenTrades(int totalOpenTrades) { totalOpenTrades_ = totalOpenTrades; }
    void setTotalOpenPl(double totalOpenPl) { totalOpenPl_ = totalOpenPl; }
    void setTotalLiquidations(int totalLiquidations) { totalLiquidations_ = totalLiquidations; }
    void setSessionInfo(const std::unordered_map< std::string, std::string >& sessionInfo)
    {
        sessionInfo_ = sessionInfo;
    }

   private:
    // Private constructor for singleton
    AppState();

    // Delete copy constructor and assignment operator
    AppState(const AppState&)            = delete;
    AppState& operator=(const AppState&) = delete;

    // Current timestamp in milliseconds
    int64_t time_;

    // Time boundaries
    std::optional< int64_t > startingTime_;
    std::optional< int64_t > endingTime_;

    // Balance history
    std::vector< double > dailyBalance_;

    // Open trades metrics
    int totalOpenTrades_;
    double totalOpenPl_;
    int totalLiquidations_;

    // Session information
    std::string sessionId_;
    std::unordered_map< std::string, std::string > sessionInfo_;

    // API keys (live mode only)
    std::optional< db::ExchangeApiKeys > exchangeApiKey_;
    std::optional< db::NotificationApiKeys > notificationsApiKey_;
};

} // namespace ct
