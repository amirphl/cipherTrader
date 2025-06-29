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
    std::optional< int64_t > getStartingTime() const { return starting_time_; }
    std::optional< int64_t > getEndingTime() const { return ending_time_; }
    const std::vector< double >& getDailyBalance() const { return daily_balance_; }
    int getTotalOpenTrades() const { return total_open_trades_; }
    double getTotalOpenPl() const { return total_open_pl_; }
    int getTotalLiquidations() const { return total_liquidations_; }
    const std::string& getSessionId() const { return session_id_; }
    const std::unordered_map< std::string, std::string >& getSessionInfo() const { return session_info_; }
    const std::optional< db::ExchangeApiKeys >& getExchangeApiKey() const { return exchange_api_key_; }
    const std::optional< db::NotificationApiKeys >& getNotificationsApiKey() const { return notifications_api_key_; }

    // Setters
    void setTime(int64_t time) { time_ = time; }
    void setStartingTime(int64_t startingTime) { starting_time_ = startingTime; }
    void setEndingTime(int64_t endingTime) { ending_time_ = endingTime; }
    void setDailyBalance(const std::vector< double >& dailyBalance) { daily_balance_ = dailyBalance; }
    void setTotalOpenTrades(int totalOpenTrades) { total_open_trades_ = totalOpenTrades; }
    void setTotalOpenPl(double totalOpenPl) { total_open_pl_ = totalOpenPl; }
    void setTotalLiquidations(int totalLiquidations) { total_liquidations_ = totalLiquidations; }
    void setSessionInfo(const std::unordered_map< std::string, std::string >& sessionInfo)
    {
        session_info_ = sessionInfo;
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
    std::optional< int64_t > starting_time_;
    std::optional< int64_t > ending_time_;

    // Balance history
    std::vector< double > daily_balance_;

    // Open trades metrics
    int total_open_trades_;
    double total_open_pl_;
    int total_liquidations_;

    // Session information
    std::string session_id_;
    std::unordered_map< std::string, std::string > session_info_;

    // API keys (live mode only)
    std::optional< db::ExchangeApiKeys > exchange_api_key_;
    std::optional< db::NotificationApiKeys > notifications_api_key_;
};

} // namespace ct
