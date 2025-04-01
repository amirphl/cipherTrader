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

blaze::DynamicVector< double > Indicator::detail::wilderSmooth(const blaze::DynamicVector< double >& arr, int period)
{
    const size_t size = arr.size();
    blaze::DynamicVector< double > result(size, 0.0);

    if (size <= static_cast< size_t >(period))
    {
        return result;
    }

    // First value is sum of first "period" values
    double sum = 0.0;
    for (int i = 1; i <= period; ++i)
    {
        sum += arr[i];
    }
    result[period] = sum;

    // Apply smoothing formula
    for (size_t i = period + 1; i < size; ++i)
    {
        result[i] = result[i - 1] - (result[i - 1] / period) + arr[i];
    }

    return result;
}

blaze::DynamicVector< double > Indicator::ADX(const blaze::DynamicMatrix< double >& candles,
                                              int period,
                                              bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = Helper::sliceCandles(candles, sequential);
    const size_t size   = sliced_candles.rows();

    if (size <= static_cast< size_t >(period * 2))
    {
        throw std::invalid_argument("Insufficient data for ADX calculation");
    }

    // Get price data
    auto high  = Helper::getCandleSource(sliced_candles, Candle::Source::High);
    auto low   = Helper::getCandleSource(sliced_candles, Candle::Source::Low);
    auto close = Helper::getCandleSource(sliced_candles, Candle::Source::Close);

    // Initialize vectors
    blaze::DynamicVector< double > TR(size, 0.0);
    blaze::DynamicVector< double > plusDM(size, 0.0);
    blaze::DynamicVector< double > minusDM(size, 0.0);

    // Calculate True Range and Directional Movement
    for (size_t i = 1; i < size; ++i)
    {
        const double hl = high[i] - low[i];
        const double hc = std::abs(high[i] - close[i - 1]);
        const double lc = std::abs(low[i] - close[i - 1]);
        TR[i]           = std::max({hl, hc, lc});

        const double h_diff = high[i] - high[i - 1];
        const double l_diff = low[i - 1] - low[i];

        plusDM[i]  = (h_diff > l_diff && h_diff > 0) ? h_diff : 0.0;
        minusDM[i] = (l_diff > h_diff && l_diff > 0) ? l_diff : 0.0;
    }

    // Apply Wilder's smoothing
    auto tr_smooth       = detail::wilderSmooth(TR, period);
    auto plus_dm_smooth  = detail::wilderSmooth(plusDM, period);
    auto minus_dm_smooth = detail::wilderSmooth(minusDM, period);

    // Calculate DI+ and DI-
    blaze::DynamicVector< double > DI_plus(size, 0.0);
    blaze::DynamicVector< double > DI_minus(size, 0.0);
    blaze::DynamicVector< double > DX(size, 0.0);

    for (size_t i = period; i < size; ++i)
    {
        if (tr_smooth[i] > std::numeric_limits< double >::epsilon())
        {
            DI_plus[i]  = 100.0 * plus_dm_smooth[i] / tr_smooth[i];
            DI_minus[i] = 100.0 * minus_dm_smooth[i] / tr_smooth[i];

            const double di_sum = DI_plus[i] + DI_minus[i];
            if (di_sum > std::numeric_limits< double >::epsilon())
            {
                DX[i] = 100.0 * std::abs(DI_plus[i] - DI_minus[i]) / di_sum;
            }
        }
    }

    // Calculate ADX
    blaze::DynamicVector< double > ADX(size, 0.0);
    const size_t start_index = period * 2;

    if (start_index < size)
    {
        // Calculate first ADX value
        double dx_sum = 0.0;
        for (size_t i = period; i < start_index; ++i)
        {
            dx_sum += DX[i];
        }
        ADX[start_index] = dx_sum / period;

        // Calculate subsequent ADX values
        for (size_t i = start_index + 1; i < size; ++i)
        {
            ADX[i] = (ADX[i - 1] * (period - 1) + DX[i]) / period;
        }
    }

    // Return either the full sequence or just the last value
    if (!sequential)
    {
        return blaze::DynamicVector< double >{ADX[size - 1]};
    }

    return ADX;
}


blaze::DynamicVector< double > Indicator::detail::calculateADXR(const blaze::DynamicVector< double >& high,
                                                                const blaze::DynamicVector< double >& low,
                                                                const blaze::DynamicVector< double >& close,
                                                                int period)
{
    const size_t n = high.size();

    // Initialize arrays
    blaze::DynamicVector< double > TR(n, 0.0);
    blaze::DynamicVector< double > DMP(n, 0.0);
    blaze::DynamicVector< double > DMM(n, 0.0);

    // First value
    TR[0] = high[0] - low[0];

    // Calculate TR, DMP, DMM
    for (size_t i = 1; i < n; ++i)
    {
        const double hl = high[i] - low[i];
        const double hc = std::abs(high[i] - close[i - 1]);
        const double lc = std::abs(low[i] - close[i - 1]);
        TR[i]           = std::max({hl, hc, lc});

        const double up_move   = high[i] - high[i - 1];
        const double down_move = low[i - 1] - low[i];

        if (up_move > down_move && up_move > 0)
        {
            DMP[i] = up_move;
        }

        if (down_move > up_move && down_move > 0)
        {
            DMM[i] = down_move;
        }
    }

    // Smoothed TR, DMP, DMM
    blaze::DynamicVector< double > STR(n, 0.0);
    blaze::DynamicVector< double > S_DMP(n, 0.0);
    blaze::DynamicVector< double > S_DMM(n, 0.0);

    // Initialize first value
    STR[0]   = TR[0];
    S_DMP[0] = DMP[0];
    S_DMM[0] = DMM[0];

    // Calculate smoothed values using Wilder's smoothing formula
    for (size_t i = 1; i < n; ++i)
    {
        STR[i]   = STR[i - 1] - (STR[i - 1] / period) + TR[i];
        S_DMP[i] = S_DMP[i - 1] - (S_DMP[i - 1] / period) + DMP[i];
        S_DMM[i] = S_DMM[i - 1] - (S_DMM[i - 1] / period) + DMM[i];
    }

    // Calculate DI+ and DI-
    blaze::DynamicVector< double > DI_plus(n, 0.0);
    blaze::DynamicVector< double > DI_minus(n, 0.0);

    const double epsilon = std::numeric_limits< double >::epsilon();

    for (size_t i = 0; i < n; ++i)
    {
        if (STR[i] > epsilon)
        {
            DI_plus[i]  = (S_DMP[i] / STR[i]) * 100.0;
            DI_minus[i] = (S_DMM[i] / STR[i]) * 100.0;
        }
    }

    // Calculate DX
    blaze::DynamicVector< double > DX(n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const double denom = DI_plus[i] + DI_minus[i];
        if (denom > epsilon)
        {
            DX[i] = (std::abs(DI_plus[i] - DI_minus[i]) / denom) * 100.0;
        }
    }

    // Calculate ADX
    blaze::DynamicVector< double > ADX(n, 0.0);

    if (n >= static_cast< size_t >(period))
    {
        for (size_t i = period - 1; i < n; ++i)
        {
            double sum_dx = 0.0;
            for (size_t j = 0; j < static_cast< size_t >(period); ++j)
            {
                sum_dx += DX[i - j];
            }
            ADX[i] = sum_dx / period;
        }
    }

    // Calculate ADXR
    blaze::DynamicVector< double > ADXR(n, 0.0);

    if (n > static_cast< size_t >(period))
    {
        for (size_t i = period; i < n; ++i)
        {
            // Make sure we don't go out of bounds
            if (i >= static_cast< size_t >(period))
            {
                ADXR[i] = (ADX[i] + ADX[i - period]) / 2.0;
            }
        }
    }

    return ADXR;
}

blaze::DynamicVector< double > Indicator::ADXR(const blaze::DynamicMatrix< double >& candles,
                                               int period,
                                               bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = Helper::sliceCandles(candles, sequential);
    const size_t size   = sliced_candles.rows();

    // We need at least 2*period bars for a meaningful calculation
    if (size <= static_cast< size_t >(period * 2))
    {
        throw std::invalid_argument("Insufficient data for ADXR calculation");
    }

    // Get price data
    auto high  = Helper::getCandleSource(sliced_candles, Candle::Source::High);
    auto low   = Helper::getCandleSource(sliced_candles, Candle::Source::Low);
    auto close = Helper::getCandleSource(sliced_candles, Candle::Source::Close);

    // Calculate ADXR
    auto adxr = detail::calculateADXR(high, low, close, period);

    // Return either the full sequence or just the last value
    if (!sequential)
    {
        return blaze::DynamicVector< double >{adxr[size - 1]};
    }

    return adxr;
}
