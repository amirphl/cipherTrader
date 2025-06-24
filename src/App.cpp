#include "App.hpp"

namespace ct
{

AppState::AppState()
    : time_(std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch())
                .count()) // TODO: Use Helper.
    , startingTime_(std::nullopt)
    , endingTime_(std::nullopt)
    , totalOpenTrades_(0)
    , totalOpenPl_(0.0)
    , totalLiquidations_(0)
    , sessionId_("")
{
    // Initialize empty vectors and maps
    dailyBalance_.clear();
    sessionInfo_.clear();
}

void AppState::setSessionId(const std::string& sessionId)
{
    sessionId_ = sessionId;
}

void AppState::setExchangeApiKey(const boost::uuids::uuid& exchangeApiKeyId)
{
    if (exchangeApiKey_.has_value())
    {
        throw std::invalid_argument("exchange_api_key has already been set");
    }

    auto conn   = db::Database::getInstance().getConnection();
    auto apiKey = db::ExchangeApiKeys::findById(conn, exchangeApiKeyId);

    if (apiKey.has_value())
    {
        exchangeApiKey_ = std::move(apiKey);
    }
}

void AppState::setNotificationsApiKey(const boost::uuids::uuid& notificationsApiKeyId)
{
    if (notificationsApiKey_.has_value())
    {
        throw std::invalid_argument("notifications_api_key has already been set");
    }

    auto conn   = db::Database::getInstance().getConnection();
    auto apiKey = db::NotificationApiKeys::findById(conn, notificationsApiKeyId);

    if (apiKey.has_value())
    {
        notificationsApiKey_ = std::move(apiKey);
    }
}

} // namespace ct
