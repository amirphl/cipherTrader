#ifndef CIPHER_INDICATOR_HPP
#define CIPHER_INDICATOR_HPP

#include "Precompiled.hpp"

#include "Candle.hpp"

namespace ct
{
namespace indicator
{

// Structure to hold AC oscillator results
struct ACResult
{
    // Data members - can store either single values or full vectors
    double osc;                                                  // Single oscillator value
    double change;                                               // Single change value
    blaze::DynamicVector< double, blaze::rowVector > osc_vec;    // Vector of oscillator values
    blaze::DynamicVector< double, blaze::rowVector > change_vec; // Vector of change values
    bool is_sequential;                                          // Flag to indicate if vectors are used

    // Constructor for single value results
    ACResult(double osc_val, double chg_val) : osc(osc_val), change(chg_val), is_sequential(false) {}

    // Constructor for sequential results - efficiently move vectors instead of copying
    ACResult(blaze::DynamicVector< double, blaze::rowVector >&& osc_vector,
             blaze::DynamicVector< double, blaze::rowVector >&& chg_vector)
        : osc(osc_vector.size() > 0 ? osc_vector[osc_vector.size() - 1] : 0.0)
        , change(chg_vector.size() > 0 ? chg_vector[chg_vector.size() - 1] : 0.0)
        , osc_vec(std::move(osc_vector))
        , change_vec(std::move(chg_vector))
        , is_sequential(true)
    {
    }

    // Alternative constructor that accepts const references but uses less efficient copying
    ACResult(const blaze::DynamicVector< double, blaze::rowVector >& osc_vector,
             const blaze::DynamicVector< double, blaze::rowVector >& chg_vector)
        : osc(osc_vector.size() > 0 ? osc_vector[osc_vector.size() - 1] : 0.0)
        , change(chg_vector.size() > 0 ? chg_vector[chg_vector.size() - 1] : 0.0)
        , osc_vec(osc_vector)
        , change_vec(chg_vector)
        , is_sequential(true)
    {
    }
};

// Simple Moving Average
blaze::DynamicVector< double, blaze::rowVector > sma(const blaze::DynamicVector< double, blaze::rowVector >& arr,
                                                     size_t period);

// Momentum
blaze::DynamicVector< double, blaze::rowVector > momentum(const blaze::DynamicVector< double, blaze::rowVector >& arr,
                                                          size_t period = 1);

// Acceleration/Deceleration Oscillator
ACResult ACOSC(const blaze::DynamicMatrix< double >& candles, bool sequential = false);

/**
 * @brief Calculates the Chaikin A/D Line (Accumulation/Distribution Line)
 *
 * @param candles Matrix containing OHLCV data
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing AD line values
 */
blaze::DynamicVector< double, blaze::rowVector > AD(const blaze::DynamicMatrix< double >& candles,
                                                    bool sequential = false);

/**
 * @brief Calculate the Chaikin A/D Oscillator
 *
 * @param candles Matrix containing OHLCV data
 * @param fast_period Fast EMA period (default: 3)
 * @param slow_period Slow EMA period (default: 10)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing ADOSC values
 */
blaze::DynamicVector< double, blaze::rowVector > ADOSC(const blaze::DynamicMatrix< double >& candles,
                                                       int fast_period = 3,
                                                       int slow_period = 10,
                                                       bool sequential = false);

namespace detail
{
blaze::DynamicVector< double, blaze::rowVector > computeMultiplier(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    const blaze::DynamicVector< double, blaze::rowVector >& close);

blaze::DynamicVector< double, blaze::rowVector > calculateEMA(
    const blaze::DynamicVector< double, blaze::rowVector >& values, int period);

} // namespace detail

/**
 * @brief Calculate the Average Directional Movement Index (ADX)
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing ADX values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > ADX(const blaze::DynamicMatrix< double >& candles,
                                                     int period      = 14,
                                                     bool sequential = false);

namespace detail
{
blaze::DynamicVector< double, blaze::rowVector > wilderSmooth(
    const blaze::DynamicVector< double, blaze::rowVector >& arr, int period);

} // namespace detail

/**
 * @brief Calculate the Average Directional Movement Index Rating (ADXR)
 *
 * The ADXR represents the average of the current ADX and the ADX from 'period' bars ago,
 * providing a measure of the strength of a trend regardless of direction.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing ADXR values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > ADXR(const blaze::DynamicMatrix< double >& candles,
                                                      int period      = 14,
                                                      bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to calculate the ADXR values
 */
blaze::DynamicVector< double, blaze::rowVector > calculateADXR(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    const blaze::DynamicVector< double, blaze::rowVector >& close,
    int period);

} // namespace detail

/**
 * @brief Alligator indicator structure
 *
 * Contains the three lines of the Alligator indicator: jaw, teeth, and lips
 */
struct Alligator
{
    blaze::DynamicVector< double, blaze::rowVector > jaw;   // 13-period SMMA shifted 8 bars into the future
    blaze::DynamicVector< double, blaze::rowVector > teeth; // 8-period SMMA shifted 5 bars into the future
    blaze::DynamicVector< double, blaze::rowVector > lips;  // 5-period SMMA shifted 3 bars into the future

    // Constructor for single values (non-sequential mode)
    Alligator(double jaw_val, double teeth_val, double lips_val)
        : jaw(1, jaw_val), teeth(1, teeth_val), lips(1, lips_val)
    {
    }

    // Constructor for vector values (sequential mode)
    Alligator(const blaze::DynamicVector< double, blaze::rowVector >& jaw_vec,
              const blaze::DynamicVector< double, blaze::rowVector >& teeth_vec,
              const blaze::DynamicVector< double, blaze::rowVector >& lips_vec)
        : jaw(jaw_vec), teeth(teeth_vec), lips(lips_vec)
    {
    }
};

/**
 * @brief Calculate Smoothed Moving Average for Alligator indicator
 *
 * @param source Source price data
 * @param length Length of the SMMA
 * @return blaze::DynamicVector<double> Vector containing SMMA values
 */
blaze::DynamicVector< double, blaze::rowVector > SMMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int length);

/**
 * @brief Calculate the Alligator indicator
 *
 * @param candles Matrix containing OHLCV data
 * @param source_type Price source type (default: HL2)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Alligator structure containing jaw, teeth, and lips lines
 */
Alligator ALLIGATOR(const blaze::DynamicMatrix< double >& candles,
                    candle::Source source_type = candle::Source::HL2,
                    bool sequential            = false);

/**
 * @brief Calculate the Arnaud Legoux Moving Average (ALMA)
 *
 * ALMA is a moving average designed to reduce lag while maintaining smoothness.
 *
 * @param candles Matrix containing OHLCV data, or a vector of price data if already extracted
 * @param period The period for the moving average (default: 9)
 * @param sigma Controls the smoothness (default: 6.0)
 * @param distribution_offset Controls the shape of the Gaussian distribution (default: 0.85)
 * @param source_type The price source to use from candles (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing ALMA values
 * @throws std::invalid_argument if parameters are invalid
 */
blaze::DynamicVector< double, blaze::rowVector > ALMA(const blaze::DynamicMatrix< double >& candles,
                                                      int period                 = 9,
                                                      double sigma               = 6.0,
                                                      double distribution_offset = 0.85,
                                                      candle::Source source_type = candle::Source::Close,
                                                      bool sequential            = false);

/**
 * @brief Overloaded ALMA function that takes a price vector directly
 */
blaze::DynamicVector< double, blaze::rowVector > ALMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int period                 = 9,
                                                      double sigma               = 6.0,
                                                      double distribution_offset = 0.85,
                                                      bool sequential            = false);

namespace detail
{
/**
 * @brief Creates a sliding window view of a vector
 *
 * @param source Source vector
 * @param window_size Size of the sliding window
 * @return blaze::DynamicMatrix<double> Matrix where each row is a window
 */
blaze::DynamicMatrix< double > createSlidingWindows(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                    size_t window_size);

} // namespace detail

/**
 * @brief Awesome Oscillator result structure
 *
 * Contains the oscillator value and its change (momentum)
 */
struct AOResult
{
    blaze::DynamicVector< double, blaze::rowVector > osc;    // Oscillator values
    blaze::DynamicVector< double, blaze::rowVector > change; // Momentum of oscillator

    // Constructor for single values (non-sequential mode)
    AOResult(double osc_val, double change_val) : osc(1, osc_val), change(1, change_val) {}

    // Constructor for vector values (sequential mode)
    AOResult(const blaze::DynamicVector< double, blaze::rowVector >& osc_vec,
             const blaze::DynamicVector< double, blaze::rowVector >& change_vec)
        : osc(osc_vec), change(change_vec)
    {
    }
};

/**
 * @brief Calculate the Simple Moving Average
 *
 * @param source Vector of source values
 * @param period Period for SMA calculation
 * @param sequential If true, returns the entire sequence
 * @return blaze::DynamicVector<double> Vector containing SMA values
 */
blaze::DynamicVector< double, blaze::rowVector > SMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                     int period,
                                                     bool sequential = false);

/**
 * @brief Calculate momentum (difference between consecutive values)
 *
 * @param source Vector of source values
 * @return blaze::DynamicVector<double> Vector containing momentum values
 */
blaze::DynamicVector< double, blaze::rowVector > Momentum(
    const blaze::DynamicVector< double, blaze::rowVector >& source);

/**
 * @brief Calculate the Awesome Oscillator
 *
 * @param candles Matrix containing OHLCV data
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return AOResult Structure containing oscillator and momentum values
 */
AOResult AO(const blaze::DynamicMatrix< double >& candles, bool sequential = false);

/**
 * @brief Aroon indicator result structure
 *
 * Contains Aroon Down and Aroon Up values
 */
struct AroonResult
{
    blaze::DynamicVector< double, blaze::rowVector > down; // Aroon Down values
    blaze::DynamicVector< double, blaze::rowVector > up;   // Aroon Up values

    // Constructor for single values (non-sequential mode)
    AroonResult(double down_val, double up_val) : down(1, down_val), up(1, up_val) {}

    // Constructor for vector values (sequential mode)
    AroonResult(const blaze::DynamicVector< double, blaze::rowVector >& down_vec,
                const blaze::DynamicVector< double, blaze::rowVector >& up_vec)
        : down(down_vec), up(up_vec)
    {
    }
};

/**
 * @brief Calculate the Aroon indicator
 *
 * The Aroon indicator measures the time between highs and lows over a specified time period.
 * Aroon Up indicates the strength of the uptrend, while Aroon Down indicates the strength
 * of the downtrend.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return AroonResult Structure containing Aroon Down and Aroon Up values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
AroonResult AROON(const blaze::DynamicMatrix< double >& candles, int period = 14, bool sequential = false);

/**
 * @brief Calculate the Aroon Oscillator
 *
 * The Aroon Oscillator measures the strength of a trend and the likelihood of its continuation.
 * It is calculated as the difference between Aroon Up and Aroon Down.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing Aroon Oscillator values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > AROONOSC(const blaze::DynamicMatrix< double >& candles,
                                                          int period      = 14,
                                                          bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute Aroon Oscillator values
 */
blaze::DynamicVector< double, blaze::rowVector > computeAroonOsc(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    int period);

} // namespace detail

/**
 * @brief Calculate the Average True Range (ATR)
 *
 * ATR measures market volatility by calculating the moving average of the true range.
 * The true range is the greatest of:
 * 1. Current high minus current low
 * 2. Absolute value of current high minus previous close
 * 3. Absolute value of current low minus previous close
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the moving average (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing ATR values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > ATR(const blaze::DynamicMatrix< double >& candles,
                                                     int period      = 14,
                                                     bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute ATR values
 */
blaze::DynamicVector< double, blaze::rowVector > computeATR(
    const blaze::DynamicVector< double, blaze::rowVector >& high,
    const blaze::DynamicVector< double, blaze::rowVector >& low,
    const blaze::DynamicVector< double, blaze::rowVector >& close,
    int period);

} // namespace detail

/**
 * @brief Calculate the Average Price
 *
 * The Average Price is calculated as (Open + High + Low + Close) / 4
 *
 * @param candles Matrix containing OHLCV data
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing Average Price values
 * @throws std::invalid_argument if input data is invalid
 */
blaze::DynamicVector< double, blaze::rowVector > AVGPRICE(const blaze::DynamicMatrix< double >& candles,
                                                          bool sequential = false);

/**
 * @brief Calculate the Beta coefficient
 *
 * Beta compares the given candles' close price to its benchmark.
 * It measures the volatility of a security in comparison to its benchmark.
 *
 * @param candles Matrix containing OHLCV data for the security
 * @param benchmark_candles Matrix containing OHLCV data for the benchmark (same timeframe)
 * @param period The period for the calculation (default: 5)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing Beta values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > BETA(const blaze::DynamicMatrix< double >& candles,
                                                      const blaze::DynamicMatrix< double >& benchmark_candles,
                                                      int period      = 5,
                                                      bool sequential = false);

namespace detail
{
/**
 * @brief Create a sliding window view of a vector
 *
 * Similar to NumPy's sliding_window_view function
 *
 * @param source Source vector
 * @param window_size Size of the sliding window
 * @return blaze::DynamicMatrix<double> Matrix where each row is a window
 */
blaze::DynamicMatrix< double > slidingWindowView(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                 size_t window_size);

/**
 * @brief Calculate the mean of each row in a matrix
 *
 * @param matrix Input matrix
 * @return blaze::DynamicVector<double> Vector of row means
 */
blaze::DynamicVector< double, blaze::rowVector > rowMean(const blaze::DynamicMatrix< double >& matrix);

} // namespace detail

/**
 * @brief Calculate the Bollinger Bands Width (BBW)
 *
 * BBW measures the percentage difference between the upper and lower Bollinger Bands
 * relative to the middle band, indicating volatility.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the moving average (default: 20)
 * @param mult Multiplier for standard deviation (default: 2.0)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing BBW values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > BBW(const blaze::DynamicMatrix< double >& candles,
                                                     int period                 = 20,
                                                     double mult                = 2.0,
                                                     candle::Source source_type = candle::Source::Close,
                                                     bool sequential            = false);

namespace detail
{
/**
 * @brief Internal function to compute Bollinger Bands Width
 */
blaze::DynamicVector< double, blaze::rowVector > computeBBWidth(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period, double mult);

} // namespace detail

/**
 * @brief Calculate the Balance of Power (BOP) indicator
 *
 * BOP measures the strength of buyers vs sellers by comparing closing prices
 * to opening prices, normalized by the high-low range.
 *
 * Formula: BOP = (Close - Open) / (High - Low)
 *
 * @param candles Matrix containing OHLCV data
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing BOP values
 * @throws std::invalid_argument if input data is invalid
 */
blaze::DynamicVector< double, blaze::rowVector > BOP(const blaze::DynamicMatrix< double >& candles,
                                                     bool sequential = false);

/**
 * @brief Calculate the Commodity Channel Index (CCI)
 *
 * CCI measures the current price level relative to an average price level over a given period.
 * It can be used to identify cyclical trends and overbought/oversold conditions.
 *
 * Formula: CCI = (TP - SMA(TP, period)) / (0.015 * MD)
 * Where:
 * - TP (Typical Price) = (High + Low + Close) / 3
 * - SMA = Simple Moving Average
 * - MD = Mean Deviation (average of absolute deviations from the mean)
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CCI values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CCI(const blaze::DynamicMatrix< double >& candles,
                                                     int period      = 14,
                                                     bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute CCI values
 */
blaze::DynamicVector< double, blaze::rowVector > calculateCCI(
    const blaze::DynamicVector< double, blaze::rowVector >& tp, int period);

} // namespace detail

/**
 * @brief Calculate the Chande Forecast Oscillator (CFO)
 *
 * CFO is a momentum oscillator that measures the percentage difference between
 * the actual price and a linear regression forecast value.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for linear regression (default: 14)
 * @param scalar Scaling factor for the result (default: 100.0)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CFO values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CFO(const blaze::DynamicMatrix< double >& candles,
                                                     int period                 = 14,
                                                     double scalar              = 100.0,
                                                     candle::Source source_type = candle::Source::Close,
                                                     bool sequential            = false);

namespace detail
{
/**
 * @brief Internal function to compute CFO values
 */
blaze::DynamicVector< double, blaze::rowVector > computeCFO(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period, double scalar);

} // namespace detail

/**
 * @brief Calculate the Center of Gravity (CG) indicator
 *
 * The Center of Gravity is a momentum oscillator that measures the degree to which
 * recent closes are located in the center or at the extremes of recent price action.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 10)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CG values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CG(const blaze::DynamicMatrix< double >& candles,
                                                    int period                 = 10,
                                                    candle::Source source_type = candle::Source::Close,
                                                    bool sequential            = false);

namespace detail
{
/**
 * @brief Internal function to compute CG values
 */
blaze::DynamicVector< double, blaze::rowVector > calculateCG(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period);

} // namespace detail

/**
 * @brief Calculate the Choppiness Index (CHOP)
 *
 * The Choppiness Index is designed to determine if the market is choppy (trading sideways)
 * or trending. It ranges from 0 to 100, with higher values indicating choppier markets.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param scalar Scaling factor (default: 100.0)
 * @param drift Parameter for ATR calculation (default: 1)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CHOP values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CHOP(const blaze::DynamicMatrix< double >& candles,
                                                      int period      = 14,
                                                      double scalar   = 100.0,
                                                      int drift       = 1,
                                                      bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute the Choppiness Index
 */
blaze::DynamicVector< double, blaze::rowVector > calculateCHOP(const blaze::DynamicMatrix< double >& candles,
                                                               int period,
                                                               double scalar,
                                                               int drift);

} // namespace detail

/**
 * @brief Chande Kroll Stop (CKSP) result structure
 */
struct CKSPResult
{
    blaze::DynamicVector< double, blaze::rowVector > longStop;  // Long stop values
    blaze::DynamicVector< double, blaze::rowVector > shortStop; // Short stop values

    // Constructor for single values (non-sequential mode)
    CKSPResult(double longVal, double shortVal) : longStop(1, longVal), shortStop(1, shortVal) {}

    // Constructor for vector values (sequential mode)
    CKSPResult(const blaze::DynamicVector< double, blaze::rowVector >& longVals,
               const blaze::DynamicVector< double, blaze::rowVector >& shortVals)
        : longStop(longVals), shortStop(shortVals)
    {
    }
};

/**
 * @brief Calculate Average True Range (ATR)
 *
 * @param high High prices
 * @param low Low prices
 * @param close Close prices
 * @param period ATR period
 * @return blaze::DynamicVector<double> Vector containing ATR values
 */
blaze::DynamicVector< double, blaze::rowVector > ATR(const blaze::DynamicVector< double, blaze::rowVector >& high,
                                                     const blaze::DynamicVector< double, blaze::rowVector >& low,
                                                     const blaze::DynamicVector< double, blaze::rowVector >& close,
                                                     int period);

/**
 * @brief Calculate rolling maximum of a series
 *
 * @param arr Input array
 * @param window Rolling window size
 * @return blaze::DynamicVector<double> Rolling maximum values
 */
blaze::DynamicVector< double, blaze::rowVector > RollingMax(const blaze::DynamicVector< double, blaze::rowVector >& arr,
                                                            int window);

/**
 * @brief Calculate rolling minimum of a series
 *
 * @param arr Input array
 * @param window Rolling window size
 * @return blaze::DynamicVector<double> Rolling minimum values
 */
blaze::DynamicVector< double, blaze::rowVector > RollingMin(const blaze::DynamicVector< double, blaze::rowVector >& arr,
                                                            int window);

/**
 * @brief Calculate the Chande Kroll Stop (CKSP) indicator
 *
 * @param candles Matrix containing OHLCV data
 * @param p ATR period (default: 10)
 * @param x ATR multiplier (default: 1.0)
 * @param q Rolling window period (default: 9)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return CKSPResult Structure containing long and short stop values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
CKSPResult CKSP(
    const blaze::DynamicMatrix< double >& candles, int p = 10, double x = 1.0, int q = 9, bool sequential = false);

/**
 * @brief Calculate the Chande Momentum Oscillator (CMO)
 *
 * CMO is a technical momentum indicator that measures the trend strength by
 * calculating the ratio between the sum of recent gains and the sum of recent losses.
 * It oscillates between -100 and +100.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for calculations (default: 14)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CMO values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CMO(const blaze::DynamicMatrix< double >& candles,
                                                     int period                 = 14,
                                                     candle::Source source_type = candle::Source::Close,
                                                     bool sequential            = false);

namespace detail
{
/**
 * @brief Internal function to compute CMO values
 */
blaze::DynamicVector< double, blaze::rowVector > calculateCMO(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period);

} // namespace detail

/**
 * @brief Calculate Pearson's Correlation Coefficient (r)
 *
 * Pearson's correlation measures the linear relationship between two variables.
 * It ranges from -1 to 1, where:
 * 1 indicates perfect positive correlation
 * 0 indicates no correlation
 * -1 indicates perfect negative correlation
 *
 * @param candles Matrix containing OHLCV data (uses high and low prices)
 * @param period The period for the correlation calculation (default: 5)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing correlation values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CORREL(const blaze::DynamicMatrix< double >& candles,
                                                        int period      = 5,
                                                        bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute sliding window view
 *
 * @param source Source vector
 * @param window_size Size of the sliding window
 * @return blaze::DynamicMatrix<double> Matrix where each row is a window
 */
blaze::DynamicMatrix< double > slidingWindowView(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                 size_t window_size);

/**
 * @brief Calculate the mean of each row in a matrix
 *
 * @param matrix Input matrix
 * @return blaze::DynamicVector<double> Vector of means
 */
blaze::DynamicVector< double, blaze::rowVector > rowMean(const blaze::DynamicMatrix< double >& matrix);

} // namespace detail

/**
 * @brief Structure to hold Correlation Cycle results
 */
struct CCResult
{
    blaze::DynamicVector< double, blaze::rowVector > real;  // Real part of correlation cycle
    blaze::DynamicVector< double, blaze::rowVector > imag;  // Imaginary part of correlation cycle
    blaze::DynamicVector< double, blaze::rowVector > angle; // Correlation angle
    blaze::DynamicVector< int > state;                      // Market state

    // Constructor for single values (non-sequential mode)
    CCResult(double realVal, double imagVal, double angleVal, int stateVal)
        : real(1, realVal), imag(1, imagVal), angle(1, angleVal), state(1, stateVal)
    {
    }

    // Constructor for vector values (sequential mode)
    CCResult(const blaze::DynamicVector< double, blaze::rowVector >& realVals,
             const blaze::DynamicVector< double, blaze::rowVector >& imagVals,
             const blaze::DynamicVector< double, blaze::rowVector >& angleVals,
             const blaze::DynamicVector< int >& stateVals)
        : real(realVals), imag(imagVals), angle(angleVals), state(stateVals)
    {
    }
};

/**
 * @brief Calculate the Correlation Cycle indicator (John Ehlers)
 *
 * Provides correlation cycle components, correlation angle, and market state.
 *
 * @param candles Matrix containing OHLCV data
 * @param period Lookback period (default: 20)
 * @param threshold Angle threshold for state change (default: 9)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return CCResult Structure containing real, imaginary, angle, and state components
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
CCResult CORRELATION_CYCLE(const blaze::DynamicMatrix< double >& candles,
                           int period                 = 20,
                           int threshold              = 9,
                           candle::Source source_type = candle::Source::Close,
                           bool sequential            = false);

namespace detail
{
/**
 * @brief Internal function to compute correlation cycle components
 */
std::tuple< blaze::DynamicVector< double, blaze::rowVector >,
            blaze::DynamicVector< double, blaze::rowVector >,
            blaze::DynamicVector< double, blaze::rowVector > >
calculateCorrelationCycle(const blaze::DynamicVector< double, blaze::rowVector >& source, int period);

/**
 * @brief Create a shifted vector (similar to np_shift in Python)
 */
blaze::DynamicVector< double, blaze::rowVector > shiftVector(
    const blaze::DynamicVector< double, blaze::rowVector >& vector,
    int shift,
    double fill_value = std::numeric_limits< double >::quiet_NaN());

} // namespace detail

/**
 * @brief Calculate the Cubic Weighted Moving Average (CWMA)
 *
 * The CWMA gives higher weight to more recent data points with weights
 * being proportional to the cube of their position in the window.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the moving average (default: 14)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing CWMA values
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > CWMA(const blaze::DynamicMatrix< double >& candles,
                                                      int period                 = 14,
                                                      candle::Source source_type = candle::Source::Close,
                                                      bool sequential            = false);

/**
 * @brief Calculate CWMA from a source vector directly
 */
blaze::DynamicVector< double, blaze::rowVector > CWMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int period      = 14,
                                                      bool sequential = false);

namespace detail
{
/**
 * @brief Internal function to compute CWMA values
 */
blaze::DynamicVector< double, blaze::rowVector > calculateCWMA(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int period);

} // namespace detail

/**
 * @brief Structure to hold Damiani Volatmeter results
 */
struct DamianiVolatmeterResult
{
    blaze::DynamicVector< double, blaze::rowVector > vol;  // Volatility component
    blaze::DynamicVector< double, blaze::rowVector > anti; // Anti-correlation component

    // Constructor for single values (non-sequential mode)
    DamianiVolatmeterResult(double volVal, double antiVal) : vol(1, volVal), anti(1, antiVal) {}

    // Constructor for vector values (sequential mode)
    DamianiVolatmeterResult(const blaze::DynamicVector< double, blaze::rowVector >& volVals,
                            const blaze::DynamicVector< double, blaze::rowVector >& antiVals)
        : vol(volVals), anti(antiVals)
    {
    }
};

/**
 * @brief Calculate Average True Range (ATR)
 *
 * @param high High prices
 * @param low Low prices
 * @param close Close prices
 * @param period ATR period
 * @return blaze::DynamicVector<double> Vector containing ATR values
 */
blaze::DynamicVector< double, blaze::rowVector > ATR(const blaze::DynamicVector< double, blaze::rowVector >& high,
                                                     const blaze::DynamicVector< double, blaze::rowVector >& low,
                                                     const blaze::DynamicVector< double, blaze::rowVector >& close,
                                                     int period);

/**
 * @brief Calculate the Damiani Volatmeter indicator
 *
 * @param candles Matrix containing OHLCV data
 * @param vis_atr Visible ATR period (default: 13)
 * @param vis_std Visible std period (default: 20)
 * @param sed_atr Sedative ATR period (default: 40)
 * @param sed_std Sedative std period (default: 100)
 * @param threshold Threshold value (default: 1.4)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return DamianiVolatmeterResult Structure containing vol and anti components
 * @throws std::invalid_argument if parameters are invalid or data is insufficient
 */
DamianiVolatmeterResult DAMIANI_VOLATMETER(const blaze::DynamicMatrix< double >& candles,
                                           int vis_atr                = 13,
                                           int vis_std                = 20,
                                           int sed_atr                = 40,
                                           int sed_std                = 100,
                                           double threshold           = 1.4,
                                           candle::Source source_type = candle::Source::Close,
                                           bool sequential            = false);

namespace detail
{
/**
 * @brief Implementation of the Damiani Volatmeter calculation
 */
std::tuple< blaze::DynamicVector< double, blaze::rowVector >, blaze::DynamicVector< double, blaze::rowVector > >
calculateDamianiVolatmeter(const blaze::DynamicVector< double, blaze::rowVector >& source,
                           int sed_std,
                           const blaze::DynamicVector< double, blaze::rowVector >& atrvis,
                           const blaze::DynamicVector< double, blaze::rowVector >& atrsed,
                           int vis_std,
                           double threshold);

/**
 * @brief Linear filtering function similar to SciPy's lfilter
 */
blaze::DynamicVector< double, blaze::rowVector > linearFilter(
    const blaze::DynamicVector< double, blaze::rowVector >& b,
    const blaze::DynamicVector< double, blaze::rowVector >& a,
    const blaze::DynamicVector< double, blaze::rowVector >& x);

/**
 * @brief Calculate standard deviation of a sliding window
 */
blaze::DynamicVector< double, blaze::rowVector > slidingStd(
    const blaze::DynamicVector< double, blaze::rowVector >& source, int window_size);

/**
 * @brief Create a sliding window view of a vector
 */
blaze::DynamicMatrix< double > slidingWindowView(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                 size_t window_size);
} // namespace detail

/**
 * @brief Calculate the Exponential Moving Average (EMA)
 *
 * @param source Source price data
 * @param period EMA period
 * @return blaze::DynamicVector<double> Vector containing EMA values
 */
blaze::DynamicVector< double, blaze::rowVector > EMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                     int period);

/**
 * @brief Calculate the Double Exponential Moving Average (DEMA)
 *
 * DEMA = 2 * EMA(price, period) - EMA(EMA(price, period), period)
 * This provides a smoother and more responsive moving average with reduced lag.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the moving average (default: 30)
 * @param source_type The price source to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return blaze::DynamicVector<double> Vector containing DEMA values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > DEMA(const blaze::DynamicMatrix< double >& candles,
                                                      int period                 = 30,
                                                      candle::Source source_type = candle::Source::Close,
                                                      bool sequential            = false);

/**
 * @brief Calculate DEMA from a source vector directly
 */
blaze::DynamicVector< double, blaze::rowVector > DEMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int period      = 30,
                                                      bool sequential = false);

/**
 * @brief Structure to hold Directional Indicator results
 */
struct DIResult
{
    blaze::DynamicVector< double, blaze::rowVector > plus;  // +DI values
    blaze::DynamicVector< double, blaze::rowVector > minus; // -DI values

    // Constructor for single values (non-sequential mode)
    DIResult(double plusVal, double minusVal) : plus(1, plusVal), minus(1, minusVal) {}

    // Constructor for vector values (sequential mode)
    DIResult(const blaze::DynamicVector< double, blaze::rowVector >& plusVals,
             const blaze::DynamicVector< double, blaze::rowVector >& minusVals)
        : plus(plusVals), minus(minusVals)
    {
    }
};

/**
 * @brief Calculate the Directional Indicator (DI)
 *
 * The Directional Indicator consists of +DI and -DI components that measure
 * the strength of price movement in positive and negative directions.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the calculations (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return DIResult Structure containing plus and minus DI values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
DIResult DI(const blaze::DynamicMatrix< double >& candles, int period = 14, bool sequential = false);

/**
 * @brief Structure to hold Directional Movement results
 */
struct DMResult
{
    blaze::DynamicVector< double, blaze::rowVector > plus;  // +DM values
    blaze::DynamicVector< double, blaze::rowVector > minus; // -DM values

    // Constructor for single values (non-sequential mode)
    DMResult(double plusVal, double minusVal) : plus(1, plusVal), minus(1, minusVal) {}

    // Constructor for vector values (sequential mode)
    DMResult(const blaze::DynamicVector< double, blaze::rowVector >& plusVals,
             const blaze::DynamicVector< double, blaze::rowVector >& minusVals)
        : plus(plusVals), minus(minusVals)
    {
    }
};

/**
 * @brief Calculate the Directional Movement (DM)
 *
 * Directional Movement measures the strength of price movement in positive
 * and negative directions. It's a component used in several other indicators
 * like ADX.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for the Wilder's smoothing (default: 14)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return DMResult Structure containing plus and minus DM values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
DMResult DM(const blaze::DynamicMatrix< double >& candles, int period = 14, bool sequential = false);

/**
 * @brief Structure to hold Donchian Channel results
 */
struct DonchianResult
{
    blaze::DynamicVector< double, blaze::rowVector > upperband;  // Upper band values
    blaze::DynamicVector< double, blaze::rowVector > middleband; // Middle band values
    blaze::DynamicVector< double, blaze::rowVector > lowerband;  // Lower band values

    // Constructor for single values (non-sequential mode)
    DonchianResult(double upper, double middle, double lower)
        : upperband(1, upper), middleband(1, middle), lowerband(1, lower)
    {
    }

    // Constructor for vector values (sequential mode)
    DonchianResult(const blaze::DynamicVector< double, blaze::rowVector >& upper,
                   const blaze::DynamicVector< double, blaze::rowVector >& middle,
                   const blaze::DynamicVector< double, blaze::rowVector >& lower)
        : upperband(upper), middleband(middle), lowerband(lower)
    {
    }
};

/**
 * @brief Calculate the Donchian Channels
 *
 * Donchian Channels consist of an upper band at the highest high,
 * a lower band at the lowest low, and a middle band that is the average
 * of the upper and lower bands.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The lookback period for determining highs and lows (default: 20)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return DonchianResult Structure containing upperband, middleband, and lowerband values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
DonchianResult DONCHIAN(const blaze::DynamicMatrix< double >& candles, int period = 20, bool sequential = false);

/**
 * @brief Calculate Dynamic Trend Indicator (DTI) by William Blau
 *
 * DTI measures the direction and strength of price trends by analyzing
 * price movements and applying multiple EMA smoothing operations.
 *
 * @param candles Matrix containing OHLCV data
 * @param r Period for the first EMA smoothing (default: 14)
 * @param s Period for the second EMA smoothing (default: 10)
 * @param u Period for the third EMA smoothing (default: 5)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Calculated DTI values (single value or vector, depending on sequential parameter)
 * @throws std::invalid_argument if period parameters are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > DTI(
    const blaze::DynamicMatrix< double >& candles, int r = 14, int s = 10, int u = 5, bool sequential = false);

/**
 * @brief Calculate Ehlers Distance Coefficient Filter (EDCF)
 *
 * The Ehlers Distance Coefficient Filter uses a distance-weighted algorithm
 * to smooth price data, giving more weight to values that are more distant
 * from surrounding values.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The lookback period for the calculation (default: 15)
 * @param source_type The candle source to use for calculations (default: "hl2")
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EDCF values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EDCF(const blaze::DynamicMatrix< double >& candles,
                                                      int period                 = 15,
                                                      candle::Source source_type = candle::Source::HL2,
                                                      bool sequential            = false);

/**
 * @brief Calculate EDCF from a price series
 *
 * Overloaded version that accepts a price vector directly instead of candles
 *
 * @param source Vector of price data
 * @param period The lookback period for the calculation (default: 15)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EDCF values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EDCF(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int period      = 15,
                                                      bool sequential = false);

/**
 * @brief Calculate Elder's Force Index (EFI)
 *
 * The Elder's Force Index measures the power behind price movements by
 * considering both price change and volume. It helps identify potential
 * reversals and confirms trends.
 *
 * @param candles Matrix containing OHLCV data
 * @param period EMA period for smoothing (default: 13)
 * @param source_type Source price data to use (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EFI values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EFI(const blaze::DynamicMatrix< double >& candles,
                                                     int period                 = 13,
                                                     candle::Source source_type = candle::Source::Close,
                                                     bool sequential            = false);

/**
 * @brief Calculate Exponential Moving Average (EMA)
 *
 * The Exponential Moving Average gives more weight to recent prices
 * and less weight to older prices, making it more responsive to recent
 * price changes compared to a Simple Moving Average.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for EMA calculation (default: 5)
 * @param source_type The candle source to use for calculations (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EMA values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EMA(const blaze::DynamicMatrix< double >& candles,
                                                     int period                 = 5,
                                                     candle::Source source_type = candle::Source::Close,
                                                     bool sequential            = false);

/**
 * @brief Calculate EMA directly from a price series
 *
 * Overloaded version that accepts a price vector directly instead of candles
 *
 * @param source Vector of price data
 * @param period The period for EMA calculation (default: 5)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EMA values
 * @throws std::invalid_argument if period is invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                     int period      = 5,
                                                     bool sequential = false);

/**
 * @brief Calculate End Point Moving Average (EPMA)
 *
 * The End Point Moving Average is a linear weighted moving average where the
 * weighting is based on the distance from the end point adjusted by an offset.
 *
 * @param candles Matrix containing OHLCV data
 * @param period The period for EPMA calculation (default: 11)
 * @param offset The offset value that adjusts the weighting (default: 4)
 * @param source_type The candle source to use for calculations (default: Close)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EPMA values
 * @throws std::invalid_argument if period or offset are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EPMA(const blaze::DynamicMatrix< double >& candles,
                                                      int period                 = 11,
                                                      int offset                 = 4,
                                                      candle::Source source_type = candle::Source::Close,
                                                      bool sequential            = false);

/**
 * @brief Calculate EPMA directly from a price series
 *
 * Overloaded version that accepts a price vector directly instead of candles
 *
 * @param source Vector of price data
 * @param period The period for EPMA calculation (default: 11)
 * @param offset The offset value that adjusts the weighting (default: 4)
 * @param sequential If true, returns the entire sequence; if false, returns only the last value
 * @return Vector of EPMA values
 * @throws std::invalid_argument if period or offset are invalid or data is insufficient
 */
blaze::DynamicVector< double, blaze::rowVector > EPMA(const blaze::DynamicVector< double, blaze::rowVector >& source,
                                                      int period      = 11,
                                                      int offset      = 4,
                                                      bool sequential = false);

} // namespace indicator
} // namespace ct

#endif
