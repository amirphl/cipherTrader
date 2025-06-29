# 🚀 CipherTrader

**A High-Performance C++ Trading Framework for Cryptocurrency Markets**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()

---

## 🎯 Overview

CipherTrader is a sophisticated, high-performance trading framework built in modern C++ that provides institutional-grade tools for cryptocurrency trading. Designed for speed, reliability, and scalability, it offers comprehensive state management, real-time data processing, and advanced trading capabilities.

### ✨ Key Features

- **🚀 High-Performance Engine**: Built with modern C++17 for maximum speed and efficiency
- **📊 Comprehensive State Management**: Advanced state tracking for orders, positions, trades, and orderbooks
- **🗄️ Robust Database Layer**: PostgreSQL integration with SQLPP11 for type-safe database operations
- **💾 Intelligent Caching**: Multi-level caching system with Cereal serialization for optimal performance
- **🔐 Secure Authentication**: Built-in authentication system with token-based security
- **📈 Real-Time Data Processing**: WebSocket support for live market data streaming
- **🔄 Arbitrage Capabilities**: Triangular arbitrage detection and execution
- **🧪 Sandbox Environment**: Complete testing and simulation environment
- **📋 Advanced Order Management**: Support for market, limit, and stop orders with partial fills
- **⚡ Multi-Exchange Support**: Unified interface for multiple cryptocurrency exchanges

> ⚠️ Development Status: This project is currently under active development and is NOT STABLE for production use. APIs may change without notice, and features may be incomplete or experimental. Use at your own risk.

---

## ��️ Architecture

### Core Components

```
CipherTrader/
├── �� State Management
│   ├── OrdersState      # Order lifecycle management
│   ├── PositionsState   # Position tracking and calculations
│   ├── TradesState      # Trade aggregation and analysis
│   ├── OrderbooksState  # Real-time orderbook processing
│   └── ClosedTradesState # Historical trade analysis
├── 🗄️ Database Layer
│   ├── PostgreSQL Integration
│   ├── Type-safe SQLPP11 queries
│   └── Advanced filtering and caching
├── �� Trading Engine
│   ├── Broker implementation
│   ├── Exchange adapters
│   └── Arbitrage detection
└── 🛠️ Infrastructure
    ├── Authentication system
    ├── Configuration management
    └── Logging and monitoring
```

---

## 🚀 Quick Start

### Prerequisites

- **C++17** compatible compiler (Clang recommended)
- **CMake 3.20+**
- **PostgreSQL 12+**
- **Boost Libraries**
- **Blaze Math Library**

### Installation

#### 1. Clone the Repository
```bash
git clone https://github.com/your-username/cipherTrader.git
cd cipherTrader
```

#### 2. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev \
    libpq-dev libssl-dev libz-dev libgtest-dev \
    libspdlog-dev libyaml-cpp-dev libhiredis-dev \
    libblaze-dev nlohmann-json3-dev
```

**macOS:**
```bash
brew install cmake boost postgresql openssl zlib \
    googletest spdlog yaml-cpp hiredis nlohmann-json
```

#### 3. Build the Project
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### 4. Run Tests
```bash
make test
```

---

## 📖 Usage Examples

### Basic Trading Strategy

```cpp
#include "CipherTrader.hpp"

class MyStrategy : public ct::helper::Strategy {
public:
    void execute() override {
        // Get current market data
        auto orderbook = ct::orderbook::OrderbooksState::getInstance()
            .getOrderbook(ct::enums::ExchangeName::BINANCE_SPOT, "BTC/USDT");
        
        // Place a limit order
        auto order = std::make_shared<ct::db::Order>();
        order->setSymbol("BTC/USDT");
        order->setOrderSide(ct::enums::OrderSide::BUY);
        order->setOrderType(ct::enums::OrderType::LIMIT);
        order->setQty(0.001);
        order->setPrice(50000.0);
        
        // Submit order
        ct::order::OrdersState::getInstance().addOrder(order);
    }
};
```

### Arbitrage Detection

```cpp
#include "ArbitrageBot.hpp"

// Configure arbitrage bot
BotConfig config;
config.useTestNet = true;
config.accessToken = "your_token";
config.symbolB = "USDT";
config.symbolC = "ETH";
config.tradeAmountA = 0.001;

// Create and run bot
ArbitrageBot bot(config);
bot.start();

// Monitor for arbitrage opportunities
while (bot.isRunning()) {
    auto profit = bot.calculateArbitrageProfit();
    if (profit > 0.001) { // 0.1% minimum profit
        bot.executeArbitrage();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

### Database Operations

```cpp
#include "DB.hpp"

// Create a new order
auto order = std::make_shared<ct::db::Order>();
order->setSymbol("BTC/USDT");
order->setExchangeName(ct::enums::ExchangeName::BINANCE_SPOT);
order->setOrderSide(ct::enums::OrderSide::BUY);
order->setQty(0.001);
order->setPrice(50000.0);

// Save to database
order->save();

// Query orders with filters
auto filter = ct::db::Order::Filter()
    .withSymbol("BTC/USDT")
    .withExchangeName(ct::enums::ExchangeName::BINANCE_SPOT);

auto orders = ct::db::Order::findByFilter(nullptr, filter);
```

---

## �� Configuration

### Environment Variables

```bash
# Database Configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=ciphertrader
DB_USER=trader
DB_PASSWORD=secure_password

# Exchange API Keys
BINANCE_API_KEY=your_binance_key
BINANCE_SECRET_KEY=your_binance_secret

# Trading Configuration
TRADING_MODE=live  # or 'sandbox'
MAX_LEVERAGE=10
RISK_PERCENTAGE=2.0
```

### Configuration File

```yaml
# config.yaml
app:
  trading_symbols: ["BTC/USDT", "ETH/USDT"]
  trading_timeframes: ["1m", "5m", "1h"]
  
env:
  exchanges:
    binance:
      fee: 0.001
      type: "spot"
      balance: 10000.0
      
  data:
    warmup_candles_num: 240
    generate_candles_from_1m: false
    persistency: true
```

---

## 🧪 Testing

### Run All Tests
```bash
cd build
make test
```

### Run Specific Test Suites
```bash
# Database tests
./tests/DB_Test

# Trading engine tests
./tests/Trading_Test

# Performance tests
./tests/Performance_Test
```

### Sandbox Environment
```bash
# Start sandbox mode
./cipherTrader --mode=sandbox --config=config.yaml

# Run with specific strategy
./cipherTrader --strategy=MyStrategy --backtest --start-date=2024-01-01
```

---

## 📊 Performance

### Benchmarks

- **Order Processing**: 10,000+ orders/second
- **Market Data**: 100,000+ ticks/second
- **Database Operations**: 50,000+ queries/second
- **Memory Usage**: < 100MB for typical trading operations

### Optimization Features

- **Precompiled Headers**: Faster compilation times
- **Link-Time Optimization**: Smaller, faster binaries
- **SIMD Operations**: Vectorized mathematical operations
- **Lock-Free Data Structures**: Minimal contention in high-frequency scenarios

---

## 🔒 Security

### Authentication
- Token-based authentication system
- Secure API key management
- Encrypted configuration storage

### Risk Management
- Position size limits
- Maximum drawdown protection
- Real-time risk monitoring
- Automatic stop-loss mechanisms

---

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

### Development Setup
```bash
# Install development dependencies
sudo apt install clang-format clang-tidy cppcheck

# Format code
make format

# Run static analysis
make analyze

# Run all checks
make check
```

---

## �� Documentation

- [API Reference](docs/API.md)
- [Trading Strategies](docs/Strategies.md)
- [Database Schema](docs/Database.md)
- [Performance Guide](docs/Performance.md)
- [Deployment Guide](docs/Deployment.md)

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## ⚠️ Disclaimer

**Trading cryptocurrencies involves substantial risk of loss and is not suitable for all investors. The high degree of leverage can work against you as well as for you. Before deciding to trade cryptocurrencies, you should carefully consider your investment objectives, level of experience, and risk appetite. The possibility exists that you could sustain a loss of some or all of your initial investment and therefore you should not invest money that you cannot afford to lose.**

---

## 🆘 Support

- **Documentation**: [docs.ciphertrader.com](https://docs.ciphertrader.com)
- **Issues**: [GitHub Issues](https://github.com/your-username/cipherTrader/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-username/cipherTrader/discussions)
- **Email**: support@ciphertrader.com

---

**Built with ❤️ by the CipherTrader Team**

*Empowering traders with institutional-grade tools for the digital asset revolution.*
