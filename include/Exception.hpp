#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include "Enum.hpp"
#include <exception>
#include <string>

namespace Exception {

class EmptyPosition : public std::exception {
public:
  const char *what() const noexcept override {
    return "Empty position exception";
  }
};

class OpenPositionError : public std::exception {
public:
  const char *what() const noexcept override { return "Open position error"; }
};

class OrderNotAllowed : public std::exception {
public:
  const char *what() const noexcept override { return "Order not allowed"; }
};

class ConflictingRules : public std::exception {
public:
  const char *what() const noexcept override { return "Conflicting rules"; }
};

class InvalidStrategy : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid strategy"; }
};

class CandleNotFoundInDatabase : public std::exception {
public:
  const char *what() const noexcept override {
    return "Candle not found in database";
  }
};

class CandleNotFoundInExchange : public std::exception {
public:
  const char *what() const noexcept override {
    return "Candle not found in exchange";
  }
};

class SymbolNotFound : public std::exception {
public:
  const char *what() const noexcept override { return "Symbol not found"; }
};

class RouteNotFound : public std::exception {
  std::string message;

public:
  RouteNotFound(const std::string &symbol, Enum::Timeframe timeframe) {
    message = "Data route is required but missing: symbol='" + symbol +
              "', timeframe='" + toString(timeframe) + "'";
  }
  const char *what() const noexcept override { return message.c_str(); }
};

class InvalidRoutes : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid routes"; }
};

class ExchangeInMaintenance : public std::exception {
public:
  const char *what() const noexcept override {
    return "Exchange in maintenance";
  }
};

class ExchangeNotResponding : public std::exception {
public:
  const char *what() const noexcept override {
    return "Exchange not responding";
  }
};

class ExchangeRejectedOrder : public std::exception {
public:
  const char *what() const noexcept override {
    return "Exchange rejected order";
  }
};

class ExchangeOrderNotFound : public std::exception {
public:
  const char *what() const noexcept override {
    return "Exchange order not found";
  }
};

class InvalidShape : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid shape"; }
};

class InvalidConfig : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid config"; }
};

class InvalidTimeframe : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid timeframe"; }
};

class InvalidSymbol : public std::exception {
public:
  const char *what() const noexcept override { return "Invalid symbol"; }
};

class NegativeBalance : public std::exception {
public:
  const char *what() const noexcept override { return "Negative balance"; }
};

class InsufficientMargin : public std::exception {
public:
  const char *what() const noexcept override { return "Insufficient margin"; }
};

class InsufficientBalance : public std::exception {
public:
  const char *what() const noexcept override { return "Insufficient balance"; }
};

class Termination : public std::exception {
public:
  const char *what() const noexcept override { return "Termination"; }
};

class InvalidExchangeApiKeys : public std::exception {
public:
  const char *what() const noexcept override {
    return "Invalid exchange API keys";
  }
};

class ExchangeError : public std::exception {
public:
  const char *what() const noexcept override { return "Exchange error"; }
};

class NotSupportedError : public std::exception {
public:
  const char *what() const noexcept override { return "Not supported error"; }
};

class CandlesNotFound : public std::exception {
public:
  const char *what() const noexcept override { return "Candles not found"; }
};

} // namespace Exception

#endif
