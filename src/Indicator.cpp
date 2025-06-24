#include "Indicator.hpp"
#include "Candle.hpp"
#include "Helper.hpp"
#include <blaze/math/TransposeFlag.h>

void debugVector(const blaze::DynamicVector< double, blaze::rowVector >& vec, const std::string& name)
{
    std::cout << name << " [size=" << vec.size() << "]: ";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        std::cout << vec[i] << " ";
    }
    std::cout << std::endl << "------------------------" << std::endl;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::sma(
    const blaze::DynamicVector< double, blaze::rowVector >& arr, size_t period)
{
    if (arr.size() < period)
    {
        // Return vector of NaN with same size as input
        return blaze::DynamicVector< double, blaze::rowVector >(arr.size(), std::numeric_limits< double >::quiet_NaN());
    }

    blaze::DynamicVector< double, blaze::rowVector > result(arr.size(), std::numeric_limits< double >::quiet_NaN());

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

    // debugVector(result, "ct::indicator::sma::result");

    return result;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::momentum(
    const blaze::DynamicVector< double, blaze::rowVector >& arr, size_t period)
{
    if (arr.size() < 2)
    {
        return blaze::DynamicVector< double, blaze::rowVector >(arr.size(), std::numeric_limits< double >::quiet_NaN());
    }

    blaze::DynamicVector< double, blaze::rowVector > result(arr.size(), std::numeric_limits< double >::quiet_NaN());

    // Calculate differences
    for (size_t i = period; i < arr.size(); ++i)
    {
        result[i] = arr[i] - arr[i - period];
    }

    // debugVector(result, "ct::indicator::momentum::result");

    return result;
}

ct::indicator::ACResult ct::indicator::ACOSC(const blaze::DynamicMatrix< double >& candles, bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    if (sliced_candles.rows() < 34)
    { // Minimum required periods
        throw std::invalid_argument("Not enough candles for AC calculation (minimum 34 required)");
    }

    // Calculate median price (HL2)
    auto median = candle::getCandleSource(sliced_candles, candle::Source::HL2);

    // Calculate AO (Awesome Oscillator)
    auto sma5_med  = sma(median, 5);
    auto sma34_med = sma(median, 34);
    auto ao        = sma5_med - sma34_med;

    // Calculate AC
    auto sma5_ao = sma(ao, 5);
    auto ac      = ao - sma5_ao;

    // Calculate momentum
    auto mom_value = momentum(ac, 1);

    // debugVector(median, "ct::indicator::ACOSC::median");
    // debugVector(sma5_med, "ct::indicator::ACOSC::sma5_med");
    // debugVector(sma34_med, "ct::indicator::ACOSC::sma34_med");
    // debugVector(ao, "ct::indicator::ACOSC::ao");
    // debugVector(sma5_ao, "ct::indicator::ACOSC::sma5_ao");
    // debugVector(ac, "ct::indicator::ACOSC::ac");
    // debugVector(mom_value, "ct::indicator::ACOSC::mom_value");

    if (sequential)
    {
        return ACResult(std::move(ac), std::move(mom_value));
    }

    // Return last values if not sequential
    return ACResult(ac[ac.size() - 1], mom_value[mom_value.size() - 1]);
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::AD(const blaze::DynamicMatrix< double >& candles,
                                                                   bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get required price data
    auto high   = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto low    = candle::getCandleSource(sliced_candles, candle::Source::Low);
    auto close  = candle::getCandleSource(sliced_candles, candle::Source::Close);
    auto volume = candle::getCandleSource(sliced_candles, candle::Source::Volume);

    const size_t size = sliced_candles.rows();
    blaze::DynamicVector< double, blaze::rowVector > mfm(size, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > ad_line(size, 0.0);

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
        return blaze::DynamicVector< double, blaze::rowVector >{ad_line[size - 1]};
    }

    return ad_line;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::detail::computeMultiplier(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    const blaze::DynamicVector< double, blaze::rowVector >& close)
{
    const size_t size = high.size();
    blaze::DynamicVector< double, blaze::rowVector > multiplier(size, 0.0);

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

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::detail::calculateEMA(
    const blaze::DynamicVector< double, blaze::rowVector >& values, int period)
{
    const size_t size = values.size();
    blaze::DynamicVector< double, blaze::rowVector > result(size);

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

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::ADOSC(const blaze::DynamicMatrix< double >& candles,
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
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get required price data
    auto high   = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto low    = candle::getCandleSource(sliced_candles, candle::Source::Low);
    auto close  = candle::getCandleSource(sliced_candles, candle::Source::Close);
    auto volume = candle::getCandleSource(sliced_candles, candle::Source::Volume);

    // Calculate money flow multiplier
    auto multiplier = detail::computeMultiplier(high, low, close);

    // Calculate money flow volume
    auto mf_volume = multiplier * volume;

    // Calculate AD line (cumulative sum)
    const size_t size = mf_volume.size();
    blaze::DynamicVector< double, blaze::rowVector > ad_line(size);
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
        return blaze::DynamicVector< double, blaze::rowVector >{adosc[size - 1]};
    }

    return adosc;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::detail::wilderSmooth(
    const blaze::DynamicVector< double, blaze::rowVector >& arr, int period)
{
    const size_t size = arr.size();
    blaze::DynamicVector< double, blaze::rowVector > result(size, 0.0);

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

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::ADX(const blaze::DynamicMatrix< double >& candles,
                                                                    int period,
                                                                    bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);
    const size_t size   = sliced_candles.rows();

    if (size <= static_cast< size_t >(period * 2))
    {
        throw std::invalid_argument("Insufficient data for ADX calculation");
    }

    // Get price data
    auto high  = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto low   = candle::getCandleSource(sliced_candles, candle::Source::Low);
    auto close = candle::getCandleSource(sliced_candles, candle::Source::Close);

    // Initialize vectors
    blaze::DynamicVector< double, blaze::rowVector > TR(size, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > plusDM(size, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > minusDM(size, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > DI_plus(size, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > DI_minus(size, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > DX(size, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > ADX(size, 0.0);
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
        return blaze::DynamicVector< double, blaze::rowVector >{ADX[size - 1]};
    }

    return ADX;
}


blaze::DynamicVector< double, blaze::rowVector > ct::indicator::detail::calculateADXR(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    const blaze::DynamicVector< double, blaze::rowVector >& close,
    int period)
{
    const size_t n = high.size();

    // Initialize arrays
    blaze::DynamicVector< double, blaze::rowVector > TR(n, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > DMP(n, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > DMM(n, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > STR(n, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > S_DMP(n, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > S_DMM(n, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > DI_plus(n, 0.0);
    blaze::DynamicVector< double, blaze::rowVector > DI_minus(n, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > DX(n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const double denom = DI_plus[i] + DI_minus[i];
        if (denom > epsilon)
        {
            DX[i] = (std::abs(DI_plus[i] - DI_minus[i]) / denom) * 100.0;
        }
    }

    // Calculate ADX
    blaze::DynamicVector< double, blaze::rowVector > ADX(n, 0.0);

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
    blaze::DynamicVector< double, blaze::rowVector > ADXR(n, 0.0);

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

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::ADXR(const blaze::DynamicMatrix< double >& candles,
                                                                     int period,
                                                                     bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);
    const size_t size   = sliced_candles.rows();

    // We need at least 2*period bars for a meaningful calculation
    if (size <= static_cast< size_t >(period * 2))
    {
        throw std::invalid_argument("Insufficient data for ADXR calculation");
    }

    // Get price data
    auto high  = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto low   = candle::getCandleSource(sliced_candles, candle::Source::Low);
    auto close = candle::getCandleSource(sliced_candles, candle::Source::Close);

    // Calculate ADXR
    auto adxr = detail::calculateADXR(high, low, close, period);

    // Return either the full sequence or just the last value
    if (!sequential)
    {
        return blaze::DynamicVector< double, blaze::rowVector >{adxr[size - 1]};
    }

    return adxr;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::SMMA(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int length)
{
    if (length <= 0)
    {
        throw std::invalid_argument("SMMA length must be positive");
    }

    const size_t N = source.size();
    blaze::DynamicVector< double, blaze::rowVector > result(N, 0.0);

    // Calculate initial value (SMA of first 'length' elements)
    double total = 0.0;
    for (int i = 0; i < std::min(length, static_cast< int >(N)); ++i)
    {
        total += source[i];
    }
    double init_val = total / length;

    // Apply SMMA formula: SMMA(i) = (SMMA(i-1) * (length-1) + source(i)) / length
    // Which can be rewritten as: SMMA(i) = source(i) * alpha + SMMA(i-1) * (1-alpha)
    // Where alpha = 1/length
    const double alpha = 1.0 / length;
    const double beta  = 1.0 - alpha;

    // Initialize first value
    result[0] = alpha * source[0] + (init_val * beta);

    // Calculate the rest of the values
    for (size_t i = 1; i < N; ++i)
    {
        result[i] = alpha * source[i] + beta * result[i - 1];
    }

    return result;
}

ct::indicator::Alligator ct::indicator::ALLIGATOR(const blaze::DynamicMatrix< double >& candles,
                                                  candle::Source source_type,
                                                  bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get price source
    auto source = candle::getCandleSource(sliced_candles, source_type);

    // Calculate SMAs for the three lines
    auto jaw_base   = SMMA(source, 13);
    auto teeth_base = SMMA(source, 8);
    auto lips_base  = SMMA(source, 5);

    // Apply shifts
    // Note: helper::shift would need to be adapted to work with vectors instead of matrices
    auto jaw   = helper::shift(jaw_base, 8, std::numeric_limits< double >::quiet_NaN());
    auto teeth = helper::shift(teeth_base, 5, std::numeric_limits< double >::quiet_NaN());
    auto lips  = helper::shift(lips_base, 3, std::numeric_limits< double >::quiet_NaN());

    // Return sequential or last value
    if (sequential)
    {
        return Alligator(jaw, teeth, lips);
    }
    else
    {
        return Alligator(jaw[jaw.size() - 1], teeth[teeth.size() - 1], lips[lips.size() - 1]);
    }
}

blaze::DynamicMatrix< double > ct::indicator::detail::createSlidingWindows(
    const blaze::DynamicVector< double, blaze::rowVector >& source, size_t window_size)
{
    const size_t n = source.size();

    if (n < window_size)
    {
        return blaze::DynamicMatrix< double >();
    }

    // Create output matrix with dimensions [n - window_size + 1, window_size]
    const size_t num_windows = n - window_size + 1;
    blaze::DynamicMatrix< double > result(num_windows, window_size);

    // Fill the matrix with sliding windows
    for (size_t i = 0; i < num_windows; ++i)
    {
        for (size_t j = 0; j < window_size; ++j)
        {
            result(i, j) = source[i + j];
        }
    }

    return result;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::ALMA(
    const blaze::DynamicVector< double, blaze::rowVector >& source,
    int period,
    double sigma,
    double distribution_offset,
    bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }
    if (sigma <= 0)
    {
        throw std::invalid_argument("Sigma must be positive");
    }
    if (distribution_offset < 0 || distribution_offset > 1)
    {
        throw std::invalid_argument("Distribution offset must be between 0 and 1");
    }

    const size_t n = source.size();

    if (n < static_cast< size_t >(period))
    {
        throw std::invalid_argument("Input vector length must be at least equal to period");
    }

    // Create result vector
    blaze::DynamicVector< double, blaze::rowVector > result(n, std::numeric_limits< double >::quiet_NaN());

    // Calculate Gaussian weights
    blaze::DynamicVector< double, blaze::rowVector > weights(period);
    const double m   = distribution_offset * (period - 1);
    const double s   = period / sigma;
    const double dss = 2 * s * s;

    for (int i = 0; i < period; ++i)
    {
        weights[i] = std::exp(-((i - m) * (i - m)) / dss);
    }

    // Normalize weights
    const double weight_sum = blaze::sum(weights);
    weights                 = weights / weight_sum;

    // Create sliding windows of the source data
    auto windows = detail::createSlidingWindows(source, period);

    // Calculate ALMA values
    for (size_t i = 0; i < windows.rows(); ++i)
    {
        // Calculate weighted sum for this window
        double weighted_sum = 0.0;
        for (int j = 0; j < period; ++j)
        {
            weighted_sum += windows(i, j) * weights[j];
        }
        result[i + period - 1] = weighted_sum;
    }

    return sequential ? result : blaze::DynamicVector< double, blaze::rowVector >{result[n - 1]};
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::ALMA(const blaze::DynamicMatrix< double >& candles,
                                                                     int period,
                                                                     double sigma,
                                                                     double distribution_offset,
                                                                     candle::Source source_type,
                                                                     bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get price source
    auto source = candle::getCandleSource(sliced_candles, source_type);

    // Calculate ALMA on the source
    return ALMA(source, period, sigma, distribution_offset, sequential);
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::SMA(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period, bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("SMA period must be positive");
    }

    const size_t size = source.size();

    if (size < static_cast< size_t >(period))
    {
        throw std::invalid_argument("Source data length must be at least equal to period");
    }

    blaze::DynamicVector< double, blaze::rowVector > result(size, std::numeric_limits< double >::quiet_NaN());

    // Calculate first SMA value
    double sum = 0.0;
    for (int i = 0; i < period; ++i)
    {
        sum += source[i];
    }
    result[period - 1] = sum / period;

    // Calculate remaining SMA values using a sliding window approach
    for (size_t i = period; i < size; ++i)
    {
        // Add new value and subtract oldest value from sum
        sum       = sum + source[i] - source[i - period];
        result[i] = sum / period;
    }

    return sequential ? result : blaze::DynamicVector< double, blaze::rowVector >{result[size - 1]};
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::Momentum(
    const blaze::DynamicVector< double, blaze::rowVector >& source)
{
    const size_t size = source.size();
    blaze::DynamicVector< double, blaze::rowVector > result(size, std::numeric_limits< double >::quiet_NaN());

    // Calculate momentum as difference between consecutive values
    if (size > 1)
    {
        for (size_t i = 1; i < size; ++i)
        {
            result[i] = source[i] - source[i - 1];
        }
    }

    return result;
}

ct::indicator::AOResult ct::indicator::AO(const blaze::DynamicMatrix< double >& candles, bool sequential)
{
    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get HL2 (High + Low)/2 price data
    auto hl2 = candle::getCandleSource(sliced_candles, candle::Source::HL2);

    // Calculate SMAs for periods 5 and 34
    auto sma5  = SMA(hl2, 5, true);
    auto sma34 = SMA(hl2, 34, true);

    // Calculate the oscillator as the difference between the two SMAs
    auto oscillator = sma5 - sma34;

    // Calculate momentum as the difference between consecutive oscillator values
    auto momentum = Momentum(oscillator);

    // Return result based on sequential flag
    if (sequential)
    {
        return AOResult(oscillator, momentum);
    }
    else
    {
        return AOResult(oscillator[oscillator.size() - 1], momentum[momentum.size() - 1]);
    }
}

ct::indicator::AroonResult ct::indicator::AROON(const blaze::DynamicMatrix< double >& candles,
                                                int period,
                                                bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);
    const size_t size   = sliced_candles.rows();

    if (size <= static_cast< size_t >(period))
    {
        if (sequential)
        {
            // Return vectors filled with NaN
            blaze::DynamicVector< double, blaze::rowVector > nan_vector(size,
                                                                        std::numeric_limits< double >::quiet_NaN());
            return AroonResult(nan_vector, nan_vector);
        }
        else
        {
            // Return single NaN values
            return AroonResult(std::numeric_limits< double >::quiet_NaN(), std::numeric_limits< double >::quiet_NaN());
        }
    }

    // Get high and low prices
    auto highs = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto lows  = candle::getCandleSource(sliced_candles, candle::Source::Low);

    if (sequential)
    {
        // Initialize result vectors with NaN
        blaze::DynamicVector< double, blaze::rowVector > aroon_up(size, std::numeric_limits< double >::quiet_NaN());
        blaze::DynamicVector< double, blaze::rowVector > aroon_down(size, std::numeric_limits< double >::quiet_NaN());

        // Calculate Aroon values for each window
        for (size_t i = period; i < size; ++i)
        {
            // Extract window of period+1 elements
            blaze::DynamicVector< double, blaze::rowVector > window_high(period + 1);
            blaze::DynamicVector< double, blaze::rowVector > window_low(period + 1);

            for (int j = 0; j <= period; ++j)
            {
                window_high[j] = highs[i - period + j];
                window_low[j]  = lows[i - period + j];
            }

            // Find index of max high and min low in the window
            size_t max_idx = 0;
            size_t min_idx = 0;
            double max_val = window_high[0];
            double min_val = window_low[0];

            for (size_t j = 1; j <= static_cast< size_t >(period); ++j)
            {
                if (window_high[j] > max_val)
                {
                    max_val = window_high[j];
                    max_idx = j;
                }
                if (window_low[j] < min_val)
                {
                    min_val = window_low[j];
                    min_idx = j;
                }
            }

            // Calculate Aroon values
            // The formula should be: period_position / period * 100
            // where period_position is the position from the most recent bar (not from the start of the window)
            aroon_up[i]   = 100.0 * (static_cast< double >(max_idx) / period);
            aroon_down[i] = 100.0 * (static_cast< double >(min_idx) / period);
        }

        return AroonResult(aroon_down, aroon_up);
    }
    else
    {
        // Non-sequential mode - calculate only for the last period+1 values
        if (size < static_cast< size_t >(period + 1))
        {
            return AroonResult(std::numeric_limits< double >::quiet_NaN(), std::numeric_limits< double >::quiet_NaN());
        }

        // Extract last period+1 elements
        blaze::DynamicVector< double, blaze::rowVector > window_high(period + 1);
        blaze::DynamicVector< double, blaze::rowVector > window_low(period + 1);

        for (int i = 0; i <= period; ++i)
        {
            window_high[i] = highs[size - period - 1 + i];
            window_low[i]  = lows[size - period - 1 + i];
        }

        // Find index of max high and min low
        size_t max_idx = 0;
        size_t min_idx = 0;
        double max_val = window_high[0];
        double min_val = window_low[0];

        for (size_t i = 1; i <= static_cast< size_t >(period); ++i)
        {
            if (window_high[i] > max_val)
            {
                max_val = window_high[i];
                max_idx = i;
            }
            if (window_low[i] < min_val)
            {
                min_val = window_low[i];
                min_idx = i;
            }
        }

        // Calculate Aroon values
        double up_val   = 100.0 * (static_cast< double >(max_idx) / period);
        double down_val = 100.0 * (static_cast< double >(min_idx) / period);

        return AroonResult(down_val, up_val);
    }
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::detail::computeAroonOsc(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    int period)
{
    const size_t n = high.size();
    blaze::DynamicVector< double, blaze::rowVector > result(n, std::numeric_limits< double >::quiet_NaN());

    // Return early if not enough data
    if (n < static_cast< size_t >(period))
    {
        return result;
    }

    // Calculate Aroon Oscillator for each valid position
    for (size_t i = period - 1; i < n; ++i)
    {
        const size_t start = i - period + 1;

        // Find highest high and lowest low in the period
        double best_val  = high[start];
        size_t best_idx  = 0;
        double worst_val = low[start];
        size_t worst_idx = 0;

        for (size_t j = 1; j < static_cast< size_t >(period); ++j)
        {
            const double cur_high = high[start + j];
            const double cur_low  = low[start + j];

            if (cur_high > best_val)
            {
                best_val = cur_high;
                best_idx = j;
            }

            if (cur_low < worst_val)
            {
                worst_val = cur_low;
                worst_idx = j;
            }
        }

        // Calculate Aroon Oscillator value: (AroonUp - AroonDown)
        // where AroonUp = 100 * (period - days since highest high) / period
        // and AroonDown = 100 * (period - days since lowest low) / period
        // Simplified to: 100 * (best_idx - worst_idx) / period
        result[i] = 100.0 * (static_cast< double >(best_idx) - static_cast< double >(worst_idx)) / period;
    }

    return result;
}

blaze::DynamicVector< double, blaze::rowVector > ct::indicator::AROONOSC(const blaze::DynamicMatrix< double >& candles,
                                                                         int period,
                                                                         bool sequential)
{
    // Input validation
    if (period <= 0)
    {
        throw std::invalid_argument("Period must be positive");
    }

    // Slice candles if needed
    auto sliced_candles = helper::sliceCandles(candles, sequential);

    // Get high and low price data
    auto high = candle::getCandleSource(sliced_candles, candle::Source::High);
    auto low  = candle::getCandleSource(sliced_candles, candle::Source::Low);

    // Compute Aroon Oscillator
    auto result = detail::computeAroonOsc(high, low, period);

    // Return either the full sequence or just the last value
    if (sequential)
    {
        return result;
    }
    else
    {
        // Check if we have a valid last value
        const size_t size = result.size();
        if (size == 0 || std::isnan(result[size - 1]))
        {
            return blaze::DynamicVector< double, blaze::rowVector >{std::numeric_limits< double >::quiet_NaN()};
        }
        return blaze::DynamicVector< double, blaze::rowVector >{result[size - 1]};
    }
}
