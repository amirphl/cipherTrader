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

blaze::DynamicVector< double > Indicator::AD(const blaze::DynamicMatrix< double >& candles, bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = Helper::sliceCandles(candles, sequential);

    // Get required price data
    auto high   = Helper::getCandleSource(sliced_candles, Candle::Source::High);
    auto low    = Helper::getCandleSource(sliced_candles, Candle::Source::Low);
    auto close  = Helper::getCandleSource(sliced_candles, Candle::Source::Close);
    auto volume = Helper::getCandleSource(sliced_candles, Candle::Source::Volume);

    const size_t size = sliced_candles.rows();
    blaze::DynamicVector< double > mfm(size, 0.0);
    blaze::DynamicVector< double > ad_line(size, 0.0);

    // Calculate Money Flow Multiplier
    for (size_t i = 0; i < size; ++i)
    {
        double high_low_diff = high[i] - low[i];
        mfm[i]               = std::abs(high_low_diff) > std::numeric_limits< double >::epsilon()
                                   ? ((close[i] - low[i]) - (high[i] - close[i])) / high_low_diff
                                   : 0.0;
    }

    // Calculate Money Flow Volume
    auto mfv = mfm * volume;

    // Calculate cumulative sum for AD line
    ad_line[0] = mfv[0];
    for (size_t i = 1; i < size; ++i)
    {
        ad_line[i] = ad_line[i - 1] + mfv[i];
    }

    // Return either the full sequence or just the last value
    if (!sequential)
    {
        return blaze::DynamicVector< double >{ad_line[size - 1]};
    }

    return ad_line;
}

blaze::DynamicVector< double > Indicator::detail::computeMultiplier(const blaze::DynamicVector< double >& high,
                                                                    const blaze::DynamicVector< double >& low,
                                                                    const blaze::DynamicVector< double >& close)
{
    const size_t size = high.size();
    blaze::DynamicVector< double > multiplier(size, 0.0);

    for (size_t i = 0; i < size; ++i)
    {
        const double range = high[i] - low[i];
        if (std::abs(range) > std::numeric_limits< double >::epsilon())
        {
            multiplier[i] = ((close[i] - low[i]) - (high[i] - close[i])) / range;
        }
    }

    return multiplier;
}

blaze::DynamicVector< double > Indicator::detail::calculateEMA(const blaze::DynamicVector< double >& values, int period)
{
    const size_t size = values.size();
    blaze::DynamicVector< double > result(size);

    const double alpha = 2.0 / (period + 1.0);
    const double beta  = 1.0 - alpha;

    // Initialize first value
    result[0] = values[0];

    // Calculate EMA
    for (size_t i = 1; i < size; ++i)
    {
        result[i] = alpha * values[i] + beta * result[i - 1];
    }

    return result;
}

blaze::DynamicVector< double > Indicator::ADOSC(const blaze::DynamicMatrix< double >& candles,
                                                int fast_period,
                                                int slow_period,
                                                bool sequential)
{
    // Input validation
    if (fast_period <= 0 || slow_period <= 0 || fast_period >= slow_period)
    {
        throw std::invalid_argument("Invalid period parameters");
    }

    // Slice candles if needed
    auto sliced_candles = Helper::sliceCandles(candles, sequential);

    // Get required price data
    auto high   = Helper::getCandleSource(sliced_candles, Candle::Source::High);
    auto low    = Helper::getCandleSource(sliced_candles, Candle::Source::Low);
    auto close  = Helper::getCandleSource(sliced_candles, Candle::Source::Close);
    auto volume = Helper::getCandleSource(sliced_candles, Candle::Source::Volume);

    // Calculate money flow multiplier
    auto multiplier = detail::computeMultiplier(high, low, close);

    // Calculate money flow volume
    auto mf_volume = multiplier * volume;

    // Calculate AD line (cumulative sum)
    const size_t size = mf_volume.size();
    blaze::DynamicVector< double > ad_line(size);
    ad_line[0] = mf_volume[0];
    for (size_t i = 1; i < size; ++i)
    {
        ad_line[i] = ad_line[i - 1] + mf_volume[i];
    }

    // Calculate fast and slow EMAs
    auto fast_ema = detail::calculateEMA(ad_line, fast_period);
    auto slow_ema = detail::calculateEMA(ad_line, slow_period);

    // Calculate ADOSC
    auto adosc = fast_ema - slow_ema;

    // Return either the full sequence or just the last value
    if (!sequential)
    {
        return blaze::DynamicVector< double >{adosc[size - 1]};
    }

    return adosc;
}
