#ifndef MARKET_DATA_HPP
#define MARKET_DATA_HPP

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <boost/json.hpp>

// Structure to hold market data for a trading pair
struct MarketData
{
    std::string symbol; // Trading pair symbol (e.g., BTCUSDT)
    double bestBid;     // Best bid price
    double bestAsk;     // Best ask price
    double bidVolume;   // Volume at best bid
    double askVolume;   // Volume at best ask
    int64_t timestamp;  // Last update timestamp

    MarketData() : bestBid(0.0), bestAsk(0.0), bidVolume(0.0), askVolume(0.0), timestamp(0) {}

    MarketData(const std::string &sym, double bid, double ask, double bidVol = 0.0, double askVol = 0.0)
        : symbol(sym), bestBid(bid), bestAsk(ask), bidVolume(bidVol), askVolume(askVol), timestamp(0)
    {
    }

    // Calculate mid price
    double midPrice() const
    {
        if (bestBid > 0.0 && bestAsk > 0.0)
        {
            return (bestBid + bestAsk) / 2.0;
        }
        return 0.0;
    }

    // Calculate spread
    double spread() const
    {
        if (bestBid > 0.0 && bestAsk > 0.0)
        {
            return (bestAsk - bestBid) / bestAsk * 100.0; // Spread as percentage
        }
        return 0.0;
    }

    bool isValid() const { return bestBid > 0.0 && bestAsk > 0.0; }
};

// Class to handle triangular arbitrage calculations
class ArbitrageCalculator
{
   public:
    struct ArbitrageResult
    {
        bool isValid;                    // Whether the calculation is valid
        bool isForwardPath;              // Whether forward path is more profitable
        double profitPercentage;         // Profit percentage
        double startAmount;              // Starting amount
        double endAmount;                // Ending amount
        double profit;                   // Absolute profit
        std::vector< std::string > path; // The path taken (symbols)

        ArbitrageResult()
            : isValid(false), isForwardPath(false), profitPercentage(0.0), startAmount(0.0), endAmount(0.0), profit(0.0)
        {
        }
    };

    // Calculate triangular arbitrage opportunity
    static ArbitrageResult calculateTriangularArbitrage(const MarketData &dataAB,
                                                        const MarketData &dataBC,
                                                        const MarketData &dataCA,
                                                        double startAmount,
                                                        double feePercentage = 0.35)
    {
        ArbitrageResult result;
        result.startAmount = startAmount;

        // Check if all market data is valid
        if (!dataAB.isValid() || !dataBC.isValid() || !dataCA.isValid())
        {
            std::cerr << "Invalid market data for arbitrage calculation" << std::endl;
            return result;
        }

        // Extract symbols from market data
        std::string symbolA = dataAB.symbol.substr(0, 3);
        std::string symbolB = dataBC.symbol.substr(0, 3);
        std::string symbolC = dataCA.symbol.substr(0, 3);

        // Calculate forward path: A -> B -> C -> A
        double amountB = startAmount / dataAB.bestAsk; // Buy B with A
        amountB *= (1.0 - feePercentage / 100.0);      // Apply fee

        double amountC = amountB / dataBC.bestAsk; // Buy C with B
        amountC *= (1.0 - feePercentage / 100.0);  // Apply fee

        double endAmountForward = amountC * dataCA.bestBid; // Sell C for A
        endAmountForward *= (1.0 - feePercentage / 100.0);  // Apply fee

        double forwardProfit           = endAmountForward - startAmount;
        double forwardProfitPercentage = (forwardProfit / startAmount) * 100.0;

        // Calculate reverse path: A -> C -> B -> A
        double amountC_rev = startAmount * dataCA.bestBid; // Buy C with A
        amountC_rev *= (1.0 - feePercentage / 100.0);      // Apply fee

        double amountB_rev = amountC_rev * dataBC.bestBid; // Buy B with C
        amountB_rev *= (1.0 - feePercentage / 100.0);      // Apply fee

        double endAmountReverse = amountB_rev * dataAB.bestBid; // Sell B for A
        endAmountReverse *= (1.0 - feePercentage / 100.0);      // Apply fee

        double reverseProfit           = endAmountReverse - startAmount;
        double reverseProfitPercentage = (reverseProfit / startAmount) * 100.0;

        // Determine which path is more profitable
        bool useForwardPath         = forwardProfitPercentage > reverseProfitPercentage;
        double bestProfit           = useForwardPath ? forwardProfit : reverseProfit;
        double bestProfitPercentage = useForwardPath ? forwardProfitPercentage : reverseProfitPercentage;
        double endAmount            = useForwardPath ? endAmountForward : endAmountReverse;

        // Set result
        result.isValid          = true;
        result.isForwardPath    = useForwardPath;
        result.profitPercentage = bestProfitPercentage;
        result.endAmount        = endAmount;
        result.profit           = bestProfit;

        // Set path
        if (useForwardPath)
        {
            result.path = {symbolA, symbolB, symbolC, symbolA};
        }
        else
        {
            result.path = {symbolA, symbolC, symbolB, symbolA};
        }

        return result;
    }

    // Format arbitrage result for display
    static void printArbitrageResult(const ArbitrageResult &result)
    {
        if (!result.isValid)
        {
            std::cout << "Invalid arbitrage calculation" << std::endl;
            return;
        }

        std::cout << "Path: ";
        for (size_t i = 0; i < result.path.size(); ++i)
        {
            std::cout << result.path[i];
            if (i < result.path.size() - 1)
            {
                std::cout << " -> ";
            }
        }
        std::cout << std::endl;

        std::cout << "Start amount: " << result.startAmount << " " << result.path[0] << std::endl;
        std::cout << "End amount: " << result.endAmount << " " << result.path[0] << std::endl;
        std::cout << "Profit: " << result.profit << " " << result.path[0] << " (" << result.profitPercentage << "%)"
                  << std::endl;
    }
};

// Thread-safe market data manager
class MarketDataManager
{
   private:
    std::mutex dataMutex;
    std::map< std::string, MarketData > marketDataMap;

   public:
    // Update market data
    void updateMarketData(const MarketData &data)
    {
        std::lock_guard< std::mutex > lock(dataMutex);
        marketDataMap[data.symbol] = data;
    }

    // Update market data from orderbook
    void updateFromOrderbook(const std::string &symbol, const boost::json::value &bids, const boost::json::value &asks)
    {
        try
        {
            if (bids.is_array() && bids.as_array().size() > 0 && asks.is_array() && asks.as_array().size() > 0)
            {
                double bestBid   = std::stod(bids.at(0).at(0).as_string().c_str());
                double bestAsk   = std::stod(asks.at(0).at(0).as_string().c_str());
                double bidVolume = std::stod(bids.at(0).at(0).as_string().c_str());
                double askVolume = std::stod(asks.at(0).at(0).as_string().c_str());

                MarketData data(symbol, bestBid, bestAsk, bidVolume, askVolume);
                data.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

                updateMarketData(data);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error updating market data from orderbook: " << e.what() << std::endl;
        }
    }

    // Get market data for a symbol
    MarketData getMarketData(const std::string &symbol)
    {
        std::lock_guard< std::mutex > lock(dataMutex);
        auto it = marketDataMap.find(symbol);
        if (it != marketDataMap.end())
        {
            return it->second;
        }
        return MarketData(); // Return empty data if not found
    }

    // Check if we have valid data for a symbol
    bool hasValidData(const std::string &symbol)
    {
        std::lock_guard< std::mutex > lock(dataMutex);
        auto it = marketDataMap.find(symbol);
        return (it != marketDataMap.end() && it->second.isValid());
    }

    // Get all market data
    std::map< std::string, MarketData > getAllMarketData()
    {
        std::lock_guard< std::mutex > lock(dataMutex);
        return marketDataMap;
    }
};

#endif // MARKET_DATA_HPP
