#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/json.hpp>

namespace beast = boost::beast;
// namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
using tcp           = boost::asio::ip::tcp;

class WebSocketClient
{
   public:
    using MessageCallback = std::function< void(const boost::json::value &) >;

    WebSocketClient(const std::string &host, const std::string &port, const std::string &target)
        : host_(host)
        , port_(port)
        , target_(target)
        , ioc_()
        , ssl_ctx_(ssl::context::tlsv12_client)
        , is_connected_(false)
        , should_reconnect_(true)
        , reconnect_attempts_(0)
        , max_reconnect_attempts_(5)
        , reconnect_interval_ms_(2000)
    {
        // Set up SSL context
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_default_verify_paths();
    }

    ~WebSocketClient() { disconnect(); }

    void connect()
    {
        std::lock_guard< std::mutex > lock(mutex_);
        if (is_connected_)
            return;

        try
        {
            // Create the socket and SSL stream
            auto socket = std::make_shared< tcp::socket >(ioc_);
            ws_ =
                std::make_unique< websocket::stream< beast::ssl_stream< tcp::socket > > >(std::move(*socket), ssl_ctx_);

            // Set up SNI
            if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str()))
            {
                throw boost::system::system_error(boost::system::error_code(static_cast< int >(ERR_get_error()),
                                                                            boost::asio::error::get_ssl_category()));
            }

            // Look up the domain name
            tcp::resolver resolver(ioc_);
            auto const results = resolver.resolve(host_, port_);

            // Connect to the IP address
            net::connect(ws_->next_layer().next_layer(), results);

            // Perform SSL handshake
            ws_->next_layer().handshake(ssl::stream_base::client);

            // Set up the websocket handshake
            ws_->handshake(host_, target_);

            is_connected_       = true;
            reconnect_attempts_ = 0;

            // Start reading messages
            start_reading();
        }
        catch (const std::exception &e)
        {
            std::cerr << "WebSocket connection error: " << e.what() << std::endl;
            handle_disconnect();
        }
    }

    void disconnect()
    {
        {
            std::lock_guard< std::mutex > lock(mutex_);
            should_reconnect_ = false;
        }

        if (ws_ && is_connected_)
        {
            try
            {
                ws_->close(websocket::close_code::normal);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error during WebSocket disconnect: " << e.what() << std::endl;
            }
        }

        std::lock_guard< std::mutex > lock(mutex_);
        is_connected_ = false;
        ws_.reset();
    }

    void send(const std::string &message)
    {
        std::lock_guard< std::mutex > lock(mutex_);
        if (!is_connected_ || !ws_)
        {
            throw std::runtime_error("Cannot send message: WebSocket not connected");
        }

        try
        {
            ws_->write(net::buffer(message));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error sending WebSocket message: " << e.what() << std::endl;
            handle_disconnect();
        }
    }

    void setMessageCallback(MessageCallback callback)
    {
        std::lock_guard< std::mutex > lock(mutex_);
        callback_ = std::move(callback);
    }

    bool isConnected() const
    {
        std::lock_guard< std::mutex > lock(mutex_);
        return is_connected_;
    }

   private:
    void start_reading()
    {
        auto buffer = std::make_shared< beast::flat_buffer >();
        ws_->async_read(*buffer,
                        [this, buffer](beast::error_code ec,
                                       std::size_t _) { // bytes_transferred
                            if (ec)
                            {
                                handle_disconnect();
                                return;
                            }

                            try
                            {
                                std::string message{static_cast< char * >(buffer->data().data()),
                                                    buffer->data().size()};

                                auto json = boost::json::parse(message);

                                MessageCallback cb;
                                {
                                    std::lock_guard< std::mutex > lock(mutex_);
                                    cb = callback_;
                                }

                                if (cb)
                                {
                                    cb(json);
                                }
                            }
                            catch (const std::exception &e)
                            {
                                std::cerr << "Error processing message: " << e.what() << std::endl;
                            }

                            // Continue reading
                            start_reading();
                        });
    }

    void handle_disconnect()
    {
        bool should_try_reconnect = false;
        {
            std::lock_guard< std::mutex > lock(mutex_);
            is_connected_ = false;

            if (should_reconnect_ && reconnect_attempts_ < max_reconnect_attempts_)
            {
                should_try_reconnect = true;
                reconnect_attempts_++;
            }
        }

        if (should_try_reconnect)
        {
            std::cout << "Attempting to reconnect (attempt " << reconnect_attempts_ << " of " << max_reconnect_attempts_
                      << ")..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_ms_));
            connect();
        }
    }

    std::string host_;
    std::string port_;
    std::string target_;
    net::io_context ioc_;
    ssl::context ssl_ctx_;
    std::unique_ptr< websocket::stream< beast::ssl_stream< tcp::socket > > > ws_;
    MessageCallback callback_;
    std::atomic< bool > is_connected_;
    std::atomic< bool > should_reconnect_;
    int reconnect_attempts_;
    const int max_reconnect_attempts_;
    const int reconnect_interval_ms_;
    mutable std::mutex mutex_;
};

class NobitexWebSocketClient
{
   public:
    using OrderbookCallback = std::function< void(const std::string &symbol, const boost::json::value &orderbook) >;

    NobitexWebSocketClient(bool useTestNet = false)
    {
        if (useTestNet)
        {
            client_ = std::make_unique< WebSocketClient >("testnetapi.nobitex.ir", "443", "/ws");
        }
        else
        {
            client_ = std::make_unique< WebSocketClient >("wss.nobitex.ir", "443", "/connection/websocket");
        }

        client_->setMessageCallback([this](const boost::json::value &json) { processMessage(json); });
    }

    void connect()
    {
        client_->connect();
        connected_ = true;

        // Wait a moment for the connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Send initial connection message
        boost::json::object connectMsg;
        connectMsg["connect"] = boost::json::object();
        connectMsg["id"]      = 1;

        sendMessage(connectMsg);
    }

    void disconnect()
    {
        if (client_)
        {
            client_->disconnect();
        }
        connected_ = false;
    }

    void subscribeToOrderbook(const std::string &symbol)
    {
        if (!connected_)
        {
            std::cerr << "Cannot subscribe: WebSocket not connected" << std::endl;
            return;
        }

        boost::json::object subscribe;
        subscribe["channel"] = "public:orderbook-" + symbol;

        boost::json::object subMsg;
        subMsg["subscribe"] = subscribe;
        subMsg["id"]        = static_cast< int >(subscriptions_.size()) + 2;

        sendMessage(subMsg);
        subscriptions_.push_back(symbol);
    }

    void setOrderbookCallback(OrderbookCallback callback) { orderbook_callback_ = std::move(callback); }

   private:
    void sendMessage(const boost::json::value &message)
    {
        std::string messageStr = boost::json::serialize(message);
        client_->send(messageStr);
    }

    // TODO: Make const.
    void processMessage(const boost::json::value &json)
    {
        try
        {
            if (json.is_object())
            {
                auto &obj = json.as_object();
                if (obj.contains("push"))
                {
                    auto &push = obj.at("push").as_object();

                    if (push.contains("channel") && push.contains("pub"))
                    {
                        std::string channel = push.at("channel").as_string().c_str();

                        if (channel.find("public:orderbook-") == 0)
                        {
                            std::string symbol = channel.substr(17);

                            std::string dataStr = push.at("pub").at("data").as_string().c_str();
                            auto data           = boost::json::parse(dataStr);

                            if (data.is_object())
                            {
                                auto &dataObj = data.as_object();
                                if (orderbook_callback_)
                                {
                                    orderbook_callback_(symbol, dataObj);
                                }
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
        }
    }

    std::unique_ptr< WebSocketClient > client_;
    bool connected_ = false;
    std::vector< std::string > subscriptions_;
    OrderbookCallback orderbook_callback_;
};

#endif // WEBSOCKET_CLIENT_HPP
