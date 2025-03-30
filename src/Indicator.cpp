#include "Indicator.hpp"
#include <stdexcept>
#include "Helper.hpp"

void debugVector(const blaze::DynamicVector< double >& vec, const std::string& name)
{
    std::cout << name << " [size=" << vec.size() << "]: ";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        std::cout << vec[i] << " ";
    }
    std::cout << std::endl << "------------------------" << std::endl;
}

blaze::DynamicVector< double > Indicator::sma(const blaze::DynamicVector< double >& arr, size_t period)
{
    if (arr.size() < period)
    {
        // Return vector of NaN with same size as input
        return blaze::DynamicVector< double >(arr.size(), std::numeric_limits< double >::quiet_NaN());
    }

    blaze::DynamicVector< double > result(arr.size(), std::numeric_limits< double >::quiet_NaN());

    // Calculate sum for first window, skipping NaN values
    double sum         = 0.0;
    size_t valid_count = 0;

    for (size_t i = 0; i < period; ++i)
    {
        if (!std::isnan(arr[i]))
        {
            sum += arr[i];
            valid_count++;
        }
    }

    // First valid SMA value (only if we have valid values)
    if (valid_count > 0)
    {
        result[period - 1] = sum / valid_count;
    }

    // Calculate remaining values using sliding window, handling NaN values
    for (size_t i = period; i < arr.size(); ++i)
    {
        // Remove the oldest value from sum if it was valid
        if (!std::isnan(arr[i - period]))
        {
            sum -= arr[i - period];
            valid_count--;
        }

        // Add the newest value to sum if valid
        if (!std::isnan(arr[i]))
        {
            sum += arr[i];
            valid_count++;
        }

        // Only calculate average if we have valid values
        if (valid_count > 0)
        {
            result[i] = sum / valid_count;
        }
    }

    // debugVector(result, "Indicator::sma::result");

    return result;
}

blaze::DynamicVector< double > Indicator::momentum(const blaze::DynamicVector< double >& arr, size_t period)
{
    if (arr.size() < 2)
    {
        return blaze::DynamicVector< double >(arr.size(), std::numeric_limits< double >::quiet_NaN());
    }

    blaze::DynamicVector< double > result(arr.size(), std::numeric_limits< double >::quiet_NaN());

    // Calculate differences
    for (size_t i = period; i < arr.size(); ++i)
    {
        result[i] = arr[i] - arr[i - period];
    }

    // debugVector(result, "Indicator::momentum::result");

    return result;
}

Indicator::ACResult Indicator::ACOSC(const blaze::DynamicMatrix< double >& candles, bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = Helper::sliceCandles(candles, sequential);

    if (sliced_candles.rows() < 34)
    { // Minimum required periods
        throw std::invalid_argument("Not enough candles for AC calculation (minimum 34 required)");
    }

    // Calculate median price (HL2)
    auto median = Helper::getCandleSource(sliced_candles, Candle::Source::HL2);

    // Calculate AO (Awesome Oscillator)
    auto sma5_med  = sma(median, 5);
    auto sma34_med = sma(median, 34);
    auto ao        = sma5_med - sma34_med;

    // Calculate AC
    auto sma5_ao = sma(ao, 5);
    auto ac      = ao - sma5_ao;

    // Calculate momentum
    auto mom_value = momentum(ac, 1);

    // debugVector(median, "Indicator::ACOSC::median");
    // debugVector(sma5_med, "Indicator::ACOSC::sma5_med");
    // debugVector(sma34_med, "Indicator::ACOSC::sma34_med");
    // debugVector(ao, "Indicator::ACOSC::ao");
    // debugVector(sma5_ao, "Indicator::ACOSC::sma5_ao");
    // debugVector(ac, "Indicator::ACOSC::ac");
    // debugVector(mom_value, "Indicator::ACOSC::mom_value");

    if (sequential)
    {
        return ACResult(std::move(ac), std::move(mom_value));
    }

    // Return last values if not sequential
    return ACResult(ac[ac.size() - 1], mom_value[mom_value.size() - 1]);
}
