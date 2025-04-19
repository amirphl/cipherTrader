#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <thread>
#include "root_certificates.hpp"
#include "websocket_client.hpp"
#include <boost/algorithm/hex.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/url.hpp>
#include <boost/url/url.hpp>
#include <boost/url/urls.hpp>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = net::ip::tcp;

const auto SELL = "sell";
const auto BUY  = "buy";

// Configuration structure for the bot
struct BotConfig
{
    bool useTestNet;
    std::string accessToken;
    std::string symbolA;
    std::string symbolB;
    std::string symbolC;
    double tradeAmountA;
    bool useWebSocket;
};

class NobitexClient
{
   private:
    const std::string BASE_URL_REAL = "https://api.nobitex.ir:443";
    const std::string BASE_URL_TEST = "https://testnetapi.nobitex.ir:443";
    std::string baseUrl;
    std::string accessToken;
    // net::io_context io_ctx_;
    // ssl::context ssl_ctx_{ssl::context::tlsv12_client};
    const int INTERNAL_MS = 10;

    boost::json::value makeRequest(const std::string &method,
                                   const std::string &path,
                                   const boost::json::value &body = {})
    {
        try
        {
            // Parse the URL
            boost::urls::url url(baseUrl + path);

            // Set up the HTTP request
            http::request< http::string_body > req{
                method == "GET" ? http::verb::get : http::verb::post, std::string(url.path()), 11};

            req.set(http::field::host, url.host());
            req.set(http::field::user_agent, "TraderBot/HAHA1.0");

            if (!accessToken.empty())
            {
                req.set("Authorization", "Token " + accessToken);
            }

            if (!body.is_null())
            {
                req.set(http::field::content_type, "application/json");
                req.body() = boost::json::serialize(body);
                req.prepare_payload();
            }

            // The io_context is required for all I/O
            net::io_context ioc;

            // The SSL context is required, and holds certificates
            ssl::context ctx(ssl::context::tlsv12_client);

            // This holds the root certificate used for verification
            load_root_certificates(ctx);

            // Verify the remote server's certificate
            ctx.set_verify_mode(ssl::verify_peer);

            // These objects perform our I/O
            tcp::resolver resolver(ioc);
            ssl::stream< beast::tcp_stream > stream(ioc, ctx);

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(stream.native_handle(), url.host().c_str()))
            {
                throw beast::system_error(static_cast< int >(::ERR_get_error()), net::error::get_ssl_category());
            }

            // Set the expected hostname in the peer certificate for verification
            stream.set_verify_callback(ssl::host_name_verification(url.host()));

            // Look up the domain name
            auto const results = resolver.resolve(url.host(), url.port());

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(stream).connect(results);

            // Perform the SSL handshake
            stream.handshake(ssl::stream_base::client);

            // Send the request
            http::write(stream, req);

            // Receive the response
            beast::flat_buffer buffer;
            http::response< http::string_body > res;
            http::read(stream, buffer, res);

            boost::system::error_code shutdown_ec;
            auto _ = stream.shutdown(shutdown_ec);
            if (shutdown_ec != net::ssl::error::stream_truncated)
                throw beast::system_error{shutdown_ec};

            if (INTERNAL_MS > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(INTERNAL_MS));
            }

            // Parse and return the response
            if (res.result() == http::status::ok)
            {
                return boost::json::parse(res.body());
            }

            throw std::runtime_error("HTTP error: " + std::to_string(res.result_int()));
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string("Request failed: ") + e.what());
        }
    }

   public:
    NobitexClient(bool useTestNet, const std::string &token)
        : baseUrl(useTestNet ? BASE_URL_TEST : BASE_URL_REAL), accessToken(token)
    {
        if (token.empty())
        {
            throw std::invalid_argument("Access token cannot be empty");
        }
    }

    /**
     * Get the balance of a specific wallet
     *
     * @param currency The currency code (e.g., "btc", "eth", "ltc", "irt")
     * @return JSON response containing the balance
     */
    boost::json::value getWalletBalance(std::string currency)
    {
        if (currency.empty())
        {
            throw std::invalid_argument("Currency cannot be empty");
        }

        // FIXME:
        if (currency == "IRT")
        {
            currency = "RLS";
        }

        std::transform(
            currency.begin(), currency.end(), currency.begin(), [](unsigned char c) { return std::tolower(c); });
        boost::json::object body;
        body["currency"] = currency;

        return makeRequest("POST", "/users/wallets/balance", body);
    }

    /**
     * Get the balance of a specific wallet and return the actual balance value
     *
     * @param currency The currency code (e.g., "btc", "eth", "ltc", "irt")
     * @return The balance as a double
     */
    double getWalletBalanceValue(const std::string &currency)
    {
        auto response = getWalletBalance(currency);

        if (!response.is_object() || !response.as_object().contains("status") ||
            response.as_object()["status"].as_string() != "ok" || !response.as_object().contains("balance"))
        {
            throw std::runtime_error("Invalid response format for wallet balance");
        }

        try
        {
            return std::stod(response.as_object()["balance"].as_string().c_str());
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to parse balance value: " + std::string(e.what()));
        }
    }

    /**
     * Get all wallet balances (convenience method)
     *
     * @param currencies Vector of currency codes to get balances for
     * @return Map of currency code to balance value
     */
    std::map< std::string, double > getAllWalletBalances(const std::vector< std::string > &currencies)
    {
        std::map< std::string, double > balances;

        for (const auto &currency : currencies)
        {
            try
            {
                balances[currency] = getWalletBalanceValue(currency);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error getting balance for " << currency << ": " << e.what() << std::endl;
                // Continue with other currencies even if one fails
            }
        }

        return balances;
    }

    boost::json::value getOrderBook(const std::string &symbol)
    {
        if (symbol.empty())
        {
            throw std::invalid_argument("Symbol cannot be empty");
        }
        return makeRequest("GET", "/v3/orderbook/" + symbol);
    }

    boost::json::value placeMarketOrder(
        std::string base, std::string quote, const std::string &orderType, const double amount, const double price)
    {
        if (base.empty() || quote.empty())
        {
            throw std::invalid_argument("Symbol cannot be empty");
        }
        if (amount <= 0)
        {
            throw std::invalid_argument("Amount must be positive");
        }

        // FIXME:
        if (quote == "IRT")
        {
            quote = "RLS";
        }

        std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(quote.begin(), quote.end(), quote.begin(), [](unsigned char c) { return std::tolower(c); });

        boost::json::object body;
        body["type"]          = orderType;
        body["srcCurrency"]   = base;
        body["dstCurrency"]   = quote;
        body["amount"]        = amount;
        body["execution"]     = "limit";
        body["clientOrderId"] = generate_random_string();
        if (price > 0)
        {
            body["price"] = price;
        }

        return makeRequest("POST", "/market/orders/add", body);
    }

    boost::json::value getAccountBalance() { return makeRequest("POST", "/users/wallets/list"); }

    std::string generate_random_string()
    {
        // Use cryptographic-quality random device
        boost::random::random_device rng;

        // Generate 16 bytes (128 bits) of random data
        std::array< unsigned char, 16 > buffer{};
        for (unsigned char &byte : buffer)
        {
            boost::random::uniform_int_distribution< unsigned char > dist(0, 255);
            byte = dist(rng);
        }

        // Convert to 32-character hexadecimal string
        std::string result;
        boost::algorithm::hex(buffer.begin(), buffer.end(), std::back_inserter(result));

        return result; // Returns exactly 32 characters (16 bytes * 2 hex chars per
                       // byte)
    }
};

class ArbitrageBot
{
   private:
    NobitexClient client;
    BotConfig config;
    std::unique_ptr< NobitexWebSocketClient > wsClient;
    std::mutex marketDataMutex;
    std::map< std::string, boost::json::value > marketPrices;
    std::atomic< bool > running_;
    const int MIN_RETRY_INTERVAL_MS = 1000; // Minimum 1 second between retries
    const int MAX_RETRY_ATTEMPTS    = 3;    // Maximum retry attempts for operations

    std::pair< double, double > getBestTurnOver(boost::json::value &orderbook, double amount)
    {
        try
        {
            // Extract bids and asks from orderbook
            auto &bids = orderbook.at("bids").as_array();
            auto &asks = orderbook.at("asks").as_array();

            // Calculate how much B we get when selling amount of A
            double sellAmount    = amount;
            double receiveAmount = 0.0;

            for (const auto &bid : bids)
            {
                double price  = std::stod(bid.at(0).as_string().c_str());
                double volume = std::stod(bid.at(1).as_string().c_str());

                if (sellAmount <= volume)
                {
                    // Can sell all at this price
                    receiveAmount += sellAmount * price;
                    sellAmount = 0;
                    break;
                }
                else
                {
                    // Partial fill at this price
                    receiveAmount += volume * price;
                    sellAmount -= volume;
                }
            }

            // Calculate how much A we get when selling amount of B
            double buyAmount = amount;
            double payAmount = 0;

            for (const auto &ask : asks)
            {
                double price  = std::stod(ask.at(0).as_string().c_str());
                double volume = std::stod(ask.at(1).as_string().c_str());
                double t      = price * volume;

                if (buyAmount <= t)
                {
                    // Can buy all at this price
                    payAmount += price > 0 ? buyAmount / price : std::numeric_limits< double >::quiet_NaN();
                    buyAmount = 0;
                    break;
                }
                else
                {
                    // Partial fill at this price
                    payAmount += volume;
                    buyAmount -= t;
                }
            }

            // If we couldn't fill the whole order, return invalid prices
            // if (sellAmount > 0 || buyAmount > 0) {
            //   return {0.0, 0.0};
            // }

            // Return the results:
            return {receiveAmount, payAmount};
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing orderbook: " << e.what() << std::endl;
            throw;
            // return {0.0, 0.0};
        }
    }

    std::pair< double, double > getBestTurnOver(const std::string &symbol, const double amount)
    {
        // First check if we have cached data from WebSocket
        if (config.useWebSocket)
        {
            std::lock_guard< std::mutex > lock(marketDataMutex);
            auto it = marketPrices.find(symbol);
            if (it != marketPrices.end())
            {
                return getBestTurnOver(it->second, amount);
            }
        }

        // Fall back to REST API
        auto orderbook = client.getOrderBook(symbol);
        if (orderbook.is_null())
        {
            throw std::runtime_error(symbol + " Orderbook is empty.");
            // return {0.0, 0.0};
        }

        return getBestTurnOver(orderbook, amount);
    }

    // Update market data from WebSocket
    void updateMarketData(const std::string &symbol, const boost::json::value &orderbook)
    {
        std::lock_guard< std::mutex > lock(marketDataMutex);
        marketPrices[symbol] = orderbook;
    }

    void validateConfig(const BotConfig &config)
    {
        if (config.accessToken.empty())
        {
            throw std::invalid_argument("Access token cannot be empty");
        }
        if (config.symbolA.empty() || config.symbolB.empty() || config.symbolC.empty())
        {
            throw std::invalid_argument("All trading symbols must be specified");
        }
        if (config.tradeAmountA <= 0)
        {
            throw std::invalid_argument("Trade amount must be positive");
        }
    }

    std::pair< double, double > getBestTurnOverWithRetry(const std::string &symbol, const double amount)
    {
        for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; ++attempt)
        {
            try
            {
                auto price = getBestTurnOver(symbol, amount);
                // if (price.first > 0 && price.second > 0) {
                return price;
                // }
                // if (attempt < MAX_RETRY_ATTEMPTS) {
                //   std::this_thread::sleep_for(
                //       std::chrono::milliseconds(MIN_RETRY_INTERVAL_MS));
                // }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error getting price for " << symbol << " (attempt " << attempt << "/"
                          << MAX_RETRY_ATTEMPTS << "): " << e.what() << std::endl;
                if (attempt < MAX_RETRY_ATTEMPTS)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(MIN_RETRY_INTERVAL_MS));
                }
            }
        }
        throw std::runtime_error("Failed to get valid price for " + symbol + " after " +
                                 std::to_string(MAX_RETRY_ATTEMPTS) + " attempts");
    }

   public:
    ArbitrageBot(const BotConfig &botConfig)
        : client(botConfig.useTestNet, botConfig.accessToken), config(botConfig), running_(false)
    {
        validateConfig(config);

        if (config.useWebSocket)
        {
            wsClient = std::make_unique< NobitexWebSocketClient >(config.useTestNet);
            wsClient->setOrderbookCallback([this](const std::string &symbol, const boost::json::value &orderbook)
                                           { updateMarketData(symbol, orderbook); });
        }
    }

    ~ArbitrageBot()
    {
        stop();
        if (wsClient)
        {
            wsClient->disconnect();
        }
    }

    void start()
    {
        if (config.useWebSocket)
        {
            wsClient->connect();
            // Subscribe to orderbook updates for all pairs
            wsClient->subscribeToOrderbook(config.symbolA + config.symbolB);
            wsClient->subscribeToOrderbook(config.symbolB + config.symbolC);
            wsClient->subscribeToOrderbook(config.symbolA + config.symbolC);
        }
        running_ = true;
    }

    void stop() { running_ = false; }

    bool isRunning() const { return running_; }

    std::pair< double, double > calculateArbitrageProfit()
    {
        try
        {
            // Calculate forward path profit (A -> B -> C -> A)
            auto turnOverAB = getBestTurnOverWithRetry(config.symbolA + config.symbolB, config.tradeAmountA);
            double B        = turnOverAB.first; // Sell A
            B *= 0.9965;                        // Apply 0.35% fee

            auto turnOverBC = getBestTurnOverWithRetry(config.symbolB + config.symbolC, B);
            double C        = turnOverBC.first; // Sell B
            C *= 0.9965;                        // Apply 0.35% fee

            auto turnOverAC = getBestTurnOverWithRetry(config.symbolA + config.symbolC, C);
            double A        = turnOverAC.second; // Buy A
            A *= 0.9965;                         // Apply 0.35% fee

            double forwardProfit        = A - config.tradeAmountA;
            double forwardProfitPercent = (forwardProfit / config.tradeAmountA) * 100.0;

            // Calculate reverse path profit (A -> C -> B -> A)
            turnOverAC = getBestTurnOverWithRetry(config.symbolA + config.symbolC, config.tradeAmountA);
            C          = turnOverAC.first; // Sell A
            C *= 0.9965;                   // Apply 0.35% fee

            turnOverBC = getBestTurnOverWithRetry(config.symbolB + config.symbolC, C);
            B          = turnOverBC.second; // Buy B
            B *= 0.9965;                    // Apply 0.35% fee

            turnOverAB = getBestTurnOverWithRetry(config.symbolA + config.symbolB, B);
            A          = turnOverAB.second; // Buy A
            A *= 0.9965;                    // Apply 0.35% fee

            double reverseProfit        = A - config.tradeAmountA;
            double reverseProfitPercent = (reverseProfit / config.tradeAmountA) * 100.0;

            // Return the better profit percentage
            return {forwardProfitPercent, reverseProfitPercent};
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error calculating arbitrage profit: " << e.what() << std::endl;
            // return {0.0, 0.0};
            throw;
        }
    }

    bool orderStatusOk(boost::json::value &jv)
    {
        // Get the object
        const boost::json::object &obj = jv.as_object();

        // Check if "status" key exists and its value is "ok"
        auto it = obj.find("status");
        return it != obj.end() && it->value().is_string() && it->value().as_string() == "ok";
    }

    bool executeArbitrage()
    {
        try
        {
            // Calculate profits for both paths and determine which is better
            auto [forwardProfitPercent, reverseProfitPercent] = calculateArbitrageProfit();
            bool useForwardPath                               = forwardProfitPercent > reverseProfitPercent;
            double bestProfitPercent                          = std::max(forwardProfitPercent, reverseProfitPercent);

            // TODO:
            // if (bestProfitPercent <= 0) {
            // std::cout << "No profitable arbitrage opportunity found" << std::endl;
            // return false;
            // }

            // Get initial wallet balances
            std::cout << "Getting initial wallet balances..." << std::endl;
            double initialBalanceA = client.getWalletBalanceValue(config.symbolA);
            double initialBalanceB = client.getWalletBalanceValue(config.symbolB);
            double initialBalanceC = client.getWalletBalanceValue(config.symbolC);

            std::cout << "Initial balances: " << config.symbolA << ": " << initialBalanceA << ", " << config.symbolB
                      << ": " << initialBalanceB << ", " << config.symbolC << ": " << initialBalanceC << std::endl;

            // Execute the trades
            if (useForwardPath)
            {
                std::cout << "Executing forward path arbitrage..." << std::endl;

                // First trade: A -> B (Sell A for B)
                auto r1 = client.placeMarketOrder(config.symbolA,
                                                  config.symbolB,
                                                  SELL,
                                                  config.tradeAmountA,
                                                  -1); // Price per unit // turnOverAB.first / config.tradeAmountA

                std::cout << "A -> B trade: " << boost::json::serialize(r1) << std::endl;

                if (r1.is_null())
                    throw std::runtime_error("Failed to execute A -> B trade");
                if (!orderStatusOk(r1))
                    throw std::runtime_error("Failed to execute A -> B trade: status is not ok");

                double B       = client.getWalletBalanceValue(config.symbolB);
                double amountB = B - initialBalanceB;
                std::cout << "We have " << amountB << " B Now!\n";

                // Second trade: B -> C (Sell B for C)
                auto r2 = client.placeMarketOrder(config.symbolB,
                                                  config.symbolC,
                                                  SELL,
                                                  amountB,
                                                  -1); // Price per unit // turnOverBC.first / amountB

                std::cout << "B -> C trade: " << boost::json::serialize(r2) << std::endl;

                if (r2.is_null())
                    throw std::runtime_error("Failed to execute B -> C trade");
                if (!orderStatusOk(r2))
                    throw std::runtime_error("Failed to execute B -> C trade: status is not ok");

                double C       = client.getWalletBalanceValue(config.symbolC);
                double amountC = C - initialBalanceC;
                std::cout << "We have " << amountC << " C Now!\n";

                // Third trade: A -> C (Buy A with C)
                auto turnOverAC = getBestTurnOverWithRetry(config.symbolA + config.symbolC, amountC);
                auto r3         = client.placeMarketOrder(config.symbolA,
                                                  config.symbolC,
                                                  BUY,
                                                  turnOverAC.second,
                                                  -1); // Price per unit // amountC / turnOverAC.second

                std::cout << "A -> C trade: " << boost::json::serialize(r3) << std::endl;

                if (r3.is_null())
                    throw std::runtime_error("Failed to execute A -> C trade");
                if (!orderStatusOk(r3))
                    throw std::runtime_error("Failed to execute A -> C trade: status is not ok");

                double A       = client.getWalletBalanceValue(config.symbolA);
                double amountA = A - initialBalanceA;
                std::cout << "We have " << amountA << " A Now!\n";

                double finalAmountA        = amountA;
                double actualProfit        = finalAmountA - config.tradeAmountA;
                double actualProfitPercent = (actualProfit / config.tradeAmountA) * 100.0;

                std::cout << "Arbitrage executed successfully. Actual profit: " << actualProfit << " " << config.symbolA
                          << " (" << actualProfitPercent << "%)" << std::endl;
            }
            else
            {
                std::cout << "Executing reverse path arbitrage..." << std::endl;

                // First trade: A -> C (Sell A for C)
                auto r1 = client.placeMarketOrder(config.symbolA,
                                                  config.symbolC,
                                                  SELL,
                                                  config.tradeAmountA,
                                                  -1); // Price per unit // turnOverAC.first / config.tradeAmountA

                std::cout << "Completed A -> C trade: " << boost::json::serialize(r1) << std::endl;

                if (r1.is_null())
                    throw std::runtime_error("Failed to execute A -> C trade");
                if (!orderStatusOk(r1))
                    throw std::runtime_error("Failed to execute A -> C trade: status is not ok");

                double C       = client.getWalletBalanceValue(config.symbolC);
                double amountC = C - initialBalanceC;
                std::cout << "We have " << amountC << " C Now!\n";

                // Second trade: B -> C (Buy B with C)
                auto turnOverBC = getBestTurnOverWithRetry(config.symbolB + config.symbolC, amountC);
                auto r2         = client.placeMarketOrder(config.symbolB,
                                                  config.symbolC,
                                                  BUY,
                                                  turnOverBC.second,
                                                  -1); // Price per unit // amountC / turnOverBC.second

                std::cout << "B -> C trade: " << boost::json::serialize(r2) << std::endl;

                if (r2.is_null())
                    throw std::runtime_error("Failed to execute B -> C trade");
                if (!orderStatusOk(r2))
                    throw std::runtime_error("Failed to execute B -> C trade: status is not ok");

                double B       = client.getWalletBalanceValue(config.symbolB);
                double amountB = B - initialBalanceB;
                std::cout << "We have " << amountB << " B Now!\n";

                // Third trade: A -> B (Buy A with B)
                auto turnOverAB = getBestTurnOverWithRetry(config.symbolA + config.symbolB, amountB);
                auto r3         = client.placeMarketOrder(config.symbolA,
                                                  config.symbolB,
                                                  BUY,
                                                  turnOverAB.second,
                                                  -1); // Price per unit // amountB / turnOverAB.second

                std::cout << "A -> B trade: " << boost::json::serialize(r3) << std::endl;

                if (r3.is_null())
                    throw std::runtime_error("Failed to execute A -> B trade");
                if (!orderStatusOk(r3))
                    throw std::runtime_error("Failed to execute A -> B trade: status is not ok");

                double A       = client.getWalletBalanceValue(config.symbolA);
                double amountA = A - initialBalanceA;
                std::cout << "We have " << amountA << " A Now!\n";

                double finalAmountA        = amountA;
                double actualProfit        = finalAmountA - config.tradeAmountA;
                double actualProfitPercent = (actualProfit / config.tradeAmountA) * 100.0;

                std::cout << "Arbitrage executed successfully. Actual profit: " << actualProfit << " " << config.symbolA
                          << " (" << actualProfitPercent << "%)" << std::endl;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error executing arbitrage: " << e.what() << std::endl;
            return false;
        }
    }

    void monitorArbitrageOpportunities(int intervalMs = 5000, double minProfitPercent = 0.2)
    {
        if (intervalMs < MIN_RETRY_INTERVAL_MS)
        {
            std::cerr << "Warning: Monitoring interval too low, setting to " << MIN_RETRY_INTERVAL_MS << "ms"
                      << std::endl;
            intervalMs = MIN_RETRY_INTERVAL_MS;
        }

        std::cout << "Starting arbitrage monitoring..." << std::endl;
        std::cout << "Minimum profit threshold: " << minProfitPercent << "%" << std::endl;

        while (running_)
        {
            try
            {
                auto [forwardProfitPercent, reverseProfitPercent] = calculateArbitrageProfit();
                double profit = std::max(forwardProfitPercent, reverseProfitPercent);

                // TODO:
                // if (profit >= minProfitPercent) {
                std::cout << "Profitable opportunity found! Profit: " << profit << "%" << std::endl;
                if (executeArbitrage())
                {
                    // Add a small delay after successful execution
                    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs * 2));
                }
                // }

                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error in monitoring loop: " << e.what() << std::endl;
                // Add a small delay before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
        }
    }
};

void signal_handler(int signal)
{
    std::cout << "Received signal: " << signal << std::endl;
    exit(signal);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // TODO: Read fee from API.
    // Default configuration
    BotConfig config;
    config.useTestNet   = true;
    config.accessToken  = "d2ece1a37b6d4fca4a3a1e57362dc07cdf087494";
    config.symbolA      = "DOGE";
    config.symbolB      = "USDT";
    config.symbolC      = "IRT";
    config.tradeAmountA = 10;    // Amount in symbolA
    config.useWebSocket = false; // Use WebSocket for faster updates

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--real" || arg == "-r")
        {
            config.useTestNet = false;
        }
        else if (arg == "--token" || arg == "-t")
        {
            if (i + 1 < argc)
            {
                config.accessToken = argv[++i];
            }
        }
        else if (arg == "--symbolA" || arg == "-a")
        {
            if (i + 1 < argc)
            {
                config.symbolA = argv[++i];
            }
        }
        else if (arg == "--symbolB" || arg == "-b")
        {
            if (i + 1 < argc)
            {
                config.symbolB = argv[++i];
            }
        }
        else if (arg == "--symbolC" || arg == "-c")
        {
            if (i + 1 < argc)
            {
                config.symbolC = argv[++i];
            }
        }
        else if (arg == "--amount" || arg == "-m")
        {
            if (i + 1 < argc)
            {
                config.tradeAmountA = std::stod(argv[++i]);
            }
        }
        else if (arg == "--no-websocket")
        {
            config.useWebSocket = false;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Nobitex Arbitrage Bot" << std::endl;
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --real, -r             Use real market (default: test market)" << std::endl;
            std::cout << "  --token, -t TOKEN      Set access token" << std::endl;
            std::cout << "  --symbolA, -a SYMBOL   Set symbol A (default: BTC)" << std::endl;
            std::cout << "  --symbolB, -b SYMBOL   Set symbol B (default: USDT)" << std::endl;
            std::cout << "  --symbolC, -c SYMBOL   Set symbol C (default: ETH)" << std::endl;
            std::cout << "  --amount, -m AMOUNT    Set trade amount (default: 0.001)" << std::endl;
            std::cout << "  --no-websocket         Disable WebSocket (use REST API only)" << std::endl;
            std::cout << "  --help, -h             Show this help message" << std::endl;
            return 0;
        }
    }

    // Check if access token is provided
    if (config.accessToken.empty())
    {
        std::cerr << "Error: Access token is required" << std::endl;
        std::cerr << "Use --token or -t to provide an access token" << std::endl;
        return 1;
    }

    std::cout << "Nobitex Arbitrage Bot" << std::endl;
    std::cout << "Mode: " << (config.useTestNet ? "Test" : "Real") << std::endl;
    std::cout << "Data source: " << (config.useWebSocket ? "WebSocket" : "REST API") << std::endl;
    std::cout << "Symbols: " << config.symbolA << "/" << config.symbolB << ", " << config.symbolB << "/"
              << config.symbolC << ", " << config.symbolA << "/" << config.symbolC << std::endl;
    std::cout << "Trade amount: " << config.tradeAmountA << " " << config.symbolA << std::endl;

    // Create and run the arbitrage bot
    ArbitrageBot bot(config);

    // Calculate current arbitrage profit
    auto [forwardProfitPercent, reverseProfitPercent] = bot.calculateArbitrageProfit();

    double profit = std::max(forwardProfitPercent, reverseProfitPercent);
    if (profit > 0)
    {
        std::cout << "Current arbitrage profit: " << profit << "%" << std::endl;
    }

    bot.start();

    // Start monitoring for arbitrage opportunities
    bot.monitorArbitrageOpportunities();

    return 0;
}
