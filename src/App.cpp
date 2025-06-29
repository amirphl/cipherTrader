#include "App.hpp"

namespace ct
{

AppState::AppState()
    : time_(std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
                .count()) // TODO: Use Helper.
    , starting_time_(std::nullopt)
    , ending_time_(std::nullopt)
    , total_open_trades_(0)
    , total_open_pl_(0.0)
    , total_liquidations_(0)
    , session_id_("")
{
    // Initialize empty vectors and maps
    daily_balance_.clear();
    session_info_.clear();
}

void AppState::setSessionId(const std::string& sessionId)
{
    session_id_ = sessionId;
}

void AppState::setExchangeApiKey(const boost::uuids::uuid& exchangeApiKeyId)
{
    if (exchange_api_key_.has_value())
    {
        throw std::invalid_argument("exchange_api_key has already been set");
    }

    auto conn   = db::Database::getInstance().getConnection();
    auto apiKey = db::ExchangeApiKeys::findById(conn, exchangeApiKeyId);

    if (apiKey.has_value())
    {
        exchange_api_key_ = std::move(apiKey);
    }
}

void AppState::setNotificationsApiKey(const boost::uuids::uuid& notificationsApiKeyId)
{
    if (notifications_api_key_.has_value())
    {
        throw std::invalid_argument("notifications_api_key has already been set");
    }

    auto conn   = db::Database::getInstance().getConnection();
    auto apiKey = db::NotificationApiKeys::findById(conn, notificationsApiKeyId);

    if (apiKey.has_value())
    {
        notifications_api_key_ = std::move(apiKey);
    }
}

} // namespace ct
