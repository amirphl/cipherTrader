#include "Trade.hpp"

namespace ct
{
namespace trade
{

void TradesState::init()
{
    auto routes = ct::route::Router::getInstance().formattedRoutes();
    for (const auto& route : routes)
    {
        auto exchange_name = route["exchange_name"].get< enums::ExchangeName >();
        auto symbol        = route["symbol"].get< std::string >();
        std::string key    = helper::generateCompositeKey(exchange_name, symbol);

        // Create a dynamic array with 60 rows and 6 columns, dropping at 120
        std::array< size_t, 2 > storageShape = {60, 6};
        storage_.at(key) = std::make_shared< datastructure::DynamicBlazeArray< double > >(storageShape, 120);

        // Create a temporary storage with 100 rows and 4 columns
        std::array< size_t, 2 > tempShape = {100, 4};
        temp_storage_.at(key)             = std::make_shared< datastructure::DynamicBlazeArray< double > >(tempShape);
    }
}

void TradesState::addTrade(const blaze::DynamicVector< double >& trade,
                           const enums::ExchangeName& exchange_name,
                           const std::string& symbol)
{
    std::string key = helper::generateCompositeKey(exchange_name, symbol);

    // Check if we need to aggregate trades
    if (temp_storage_.at(key)->size() > 0 && trade[0] - (*temp_storage_.at(key))[0][0] >= 1000)
    {
        const auto& arr = *temp_storage_.at(key);

        size_t timestampColumn      = 0;
        size_t priceColumn          = 1;
        size_t qtyColumn            = 2;
        size_t orderSidecolumnIndex = 3;

        auto timestamp = arr[0][timestampColumn];

        auto buyTrades  = arr.filter(orderSidecolumnIndex, 1);
        auto sellTrades = arr.filter(orderSidecolumnIndex, 0);

        double avgPrice = arr.applyFunction(
            [priceColumn, qtyColumn](const blaze::DynamicMatrix< double > data)
            {
                auto col1 = blaze::column(data, priceColumn);
                auto col2 = blaze::column(data, qtyColumn);

                // Compute element-wise product and its sum
                auto turnOver = blaze::sum(col1 * col2);

                // Compute sum of column 2
                auto qtySum = blaze::sum(col2);

                // Check for division by zero
                if (qtySum == 0)
                {
                    throw std::runtime_error("Sum of quantities is zero");
                }

                return turnOver / qtySum;
            });

        auto buyQty  = blaze::sum(blaze::column(buyTrades, qtyColumn));
        auto sellQty = blaze::sum(blaze::column(sellTrades, qtyColumn));

        blaze::DynamicVector< double > generated{
            timestamp,
            avgPrice,
            buyQty,
            sellQty,
            static_cast< double >(buyTrades.rows()),
            static_cast< double >(sellTrades.rows()),
        };

        // Append to storage and clear temp storage
        storage_.at(key)->append(generated);
        temp_storage_.at(key)->flush();
    }

    // Add the trade to temporary storage
    temp_storage_.at(key)->append(trade);
}

blaze::DynamicMatrix< double > TradesState::getTrades(const enums::ExchangeName& exchange_name,
                                                      const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    return storage_.at(key)->slice(0, -1);
}

blaze::DynamicVector< double > TradesState::getCurrentTrade(const enums::ExchangeName& exchange_name,
                                                            const std::string& symbol) const
{
    std::string key = makeKey(exchange_name, symbol);
    return storage_.at(key)->getRow(-1);
}

blaze::DynamicVector< double > TradesState::getPastTrade(const enums::ExchangeName& exchange_name,
                                                         const std::string& symbol,
                                                         int number_of_trades_ago) const
{
    if (number_of_trades_ago > 120)
    {
        throw std::invalid_argument("Max accepted value for number_of_trades_ago is 120");
    }

    number_of_trades_ago = std::abs(number_of_trades_ago);
    std::string key      = makeKey(exchange_name, symbol);

    return storage_.at(key)->getRow(-1 - number_of_trades_ago);
}

} // namespace trade
} // namespace ct
