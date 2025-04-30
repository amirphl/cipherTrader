#ifndef CIPHER_EXCEPTIONS_HPP
#define CIPHER_EXCEPTIONS_HPP

#include <exception>
#include <string>
#include "Enum.hpp"

namespace ct
{
namespace exception
{

class EmptyPosition : public std::exception
{
   public:
    // Default constructor
    EmptyPosition() : message_("Empty position") {}

    // Constructor with custom message
    EmptyPosition(const std::string &message) : message_(message) {}

    // Constructor with C-style string (optional, for convenience)
    EmptyPosition(const char *message) : message_(message) {}

    // Override what() to return the stored message
    const char *what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_; // Store the message as a string
};

class OpenPositionError : public std::exception
{
   public:
    // Default constructor
    OpenPositionError() : message_("Open position error") {}

    // Constructor with custom message
    OpenPositionError(const std::string &message) : message_(message) {}

    // Constructor with C-style string (optional, for convenience)
    OpenPositionError(const char *message) : message_(message) {}

    // Override what() to return the stored message
    const char *what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_; // Store the message as a string
};

class OrderNotAllowed : public std::exception
{
   public:
    const char *what() const noexcept override { return "Order not allowed"; }
};

class ConflictingRules : public std::exception
{
   public:
    const char *what() const noexcept override { return "Conflicting rules"; }
};

class InvalidStrategy : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid strategy"; }
};

class CandleNotFoundInDatabase : public std::exception
{
   public:
    const char *what() const noexcept override { return "Candle not found in database"; }
};

class CandleNotFoundInExchange : public std::exception
{
   public:
    const char *what() const noexcept override { return "Candle not found in exchange"; }
};

class SymbolNotFound : public std::exception
{
   public:
    const char *what() const noexcept override { return "Symbol not found"; }
};

class RouteNotFound : public std::exception
{
    std::string message;

   public:
    RouteNotFound(const std::string &symbol, enums::Timeframe timeframe)
    {
        message =
            "Data route is required but missing: symbol='" + symbol + "', timeframe='" + toString(timeframe) + "'";
    }
    const char *what() const noexcept override { return message.c_str(); }
};

class InvalidRoutes : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid routes"; }
};

class ExchangeInMaintenance : public std::exception
{
   public:
    const char *what() const noexcept override { return "Exchange in maintenance"; }
};

class ExchangeNotResponding : public std::exception
{
   public:
    const char *what() const noexcept override { return "Exchange not responding"; }
};

class ExchangeRejectedOrder : public std::exception
{
   public:
    const char *what() const noexcept override { return "Exchange rejected order"; }
};

class ExchangeOrderNotFound : public std::exception
{
   public:
    const char *what() const noexcept override { return "Exchange order not found"; }
};

class InvalidShape : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid shape"; }
};

class InvalidConfig : public std::exception
{
   public:
    // Default constructor
    InvalidConfig() : message_("Invalid Config") {}

    // Constructor with custom message
    InvalidConfig(const std::string &message) : message_(message) {}

    // Constructor with C-style string (optional, for convenience)
    InvalidConfig(const char *message) : message_(message) {}

    // Override what() to return the stored message
    const char *what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_; // Store the message as a string
};

class InvalidTimeframe : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid timeframe"; }
};

class InvalidSymbol : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid symbol"; }
};

class NegativeBalance : public std::exception
{
   public:
    const char *what() const noexcept override { return "Negative balance"; }
};

class InsufficientMargin : public std::exception
{
   public:
    // Default constructor
    InsufficientMargin() : message_("Insufficient margin") {}

    // Constructor with custom message
    InsufficientMargin(const std::string &message) : message_(message) {}

    // Constructor with C-style string (optional, for convenience)
    InsufficientMargin(const char *message) : message_(message) {}

    // Override what() to return the stored message
    const char *what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_; // Store the message as a string
};

class InsufficientBalance : public std::exception
{
   public:
    // Default constructor
    InsufficientBalance() : message_("Insufficient balance") {}

    // Constructor with custom message
    InsufficientBalance(const std::string &message) : message_(message) {}

    // Constructor with C-style string (optional, for convenience)
    InsufficientBalance(const char *message) : message_(message) {}

    // Override what() to return the stored message
    const char *what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_; // Store the message as a string
};

class Termination : public std::exception
{
   public:
    const char *what() const noexcept override { return "Termination"; }
};

class InvalidExchangeApiKeys : public std::exception
{
   public:
    const char *what() const noexcept override { return "Invalid exchange API keys"; }
};

class ExchangeError : public std::exception
{
   public:
    const char *what() const noexcept override { return "Exchange error"; }
};

class NotSupportedError : public std::exception
{
   public:
    const char *what() const noexcept override { return "Not supported error"; }
};

class CandlesNotFound : public std::exception
{
   public:
    const char *what() const noexcept override { return "Candles not found"; }
};

} // namespace exception
} // namespace ct

#endif
