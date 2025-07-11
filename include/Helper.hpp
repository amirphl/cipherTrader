#ifndef CIPHER_HELPER_HPP
#define CIPHER_HELPER_HPP

#include "Enum.hpp"
#include "Timeframe.hpp"

class StrategyLoaderTest;

namespace ct
{
namespace helper
{

// 50 decimal‐digit precision decimal type
using Decimal = boost::multiprecision::cpp_dec_float_50;

std::string getQuoteAsset(const std::string &symbol);

std::string getBaseAsset(const std::string &symbol);

template < typename T >
int binarySearch(const std::vector< T > &arr, const T &item);

std::string color(const std::string &msg_text, const std::string &msg_color);

std::string style(const std::string &msg_text, const std::string &msg_style);

void error(const std::string &msg, bool force_print);

template < typename... Args >
void debug(const Args &...items);

void clearOutput();

template < typename... Args >
std::string joinItems(const Args &...items);

bool endsWith(const std::string &symbol, const std::string &s);

std::string dashlessSymbol(const std::string &symbol);

std::string dashySymbol(const std::string &symbol);

std::string underlineToDashySymbol(const std::string &symbol);

std::string dashyToUnderline(const std::string &symbol);

int dateDiffInDays(const std::chrono::system_clock::time_point &date1,
                   const std::chrono::system_clock::time_point &date2);

long long toTimestamp(std::chrono::system_clock::time_point tp);

long long toTimestamp(const std::string &date);

std::map< std::string, std::variant< int, float > > dnaToHp(const nlohmann::json &strategy_hp, const std::string &dna);

std::string stringAfterCharacter(const std::string &s, char character);

float estimateAveragePrice(float order_qty, float order_price, float current_qty, float current_entry_price);

// Estimates the profit/loss (PNL) for a trade.
// Parameters:
//   qty: Quantity of the trade (absolute value used)
//   entry_price: Price at trade entry
//   exit_price: Price at trade exit
//   position_type: "long" or "short"
//   trading_fee: Fee per unit qty per price (default 0)
// Returns: PNL in currency units (profit - fees)
// Throws: std::invalid_argument if position_type is invalid or qty is zero after
// abs
float estimatePNL(
    float qty, float entry_price, float exit_price, const enums::PositionType &position_type, float trading_fee = 0.0f);

// Estimates the PNL as a percentage of the initial investment.
// Parameters:
//   qty: Quantity of the trade (absolute value used)
//   entry_price: Price at trade entry
//   exit_price: Price at trade exit
//   position_type: "long" or "short"
// Returns: PNL as a percentage
// Throws: std::invalid_argument if position_type is invalid or qty * entry_price
// is zero
float estimatePNLPercentage(float qty, float entry_price, float exit_price, const enums::PositionType &position_type);

// Checks if a file exists at the given path.
// Parameters:
//   path: The file path to check
// Returns: True if the path is a regular file, false otherwise
bool fileExists(const std::string &path);

// Clears the contents of a file at the given path, creating it if it doesn't
// exist. Parameters:
//   path: The file path to clear
// Throws: std::runtime_error if the file cannot be opened or written to
void clearFile(const std::string &path);

// Creates a directory at the given path if it doesn't already exist.
// Parameters:
//   path: The directory path to create
// Throws: std::runtime_error if the directory cannot be created
void makeDirectory(const std::string &path);

/**
 * @brief Convert relative path to absolute
 * @param path Relative path
 * @return std::string Absolute path
 */
std::string relativeToAbsolute(const std::string &path);

// Floors a number to the specified precision.
// Parameters:
//   num: The number to floor
//   precision: Number of decimal places (non-negative)
// Returns: Floored number
// Throws: std::invalid_argument if precision < 0
double floorWithPrecision(double num, int precision = 0);

/**
 * @brief Round number
 * @param n Input number
 * @param digits Number of digits to round to
 * @return double Rounded number or nullopt
 */
double round(double n, int digits = 0);

/**
 * @brief Round quantity for live mode
 * @param qty Input quantity
 * @param precision Number of decimal places
 * @return double Rounded quantity
 * @throws std::invalid_argument if quantity is too small
 */
double roundQtyForLiveMode(double qty, int precision);

double roundQtyForLiveMode(double roundable_qty, int precision);

double roundDecimalsDown(double number, int decimals);

// Convert a double→string with full precision, then to Decimal
Decimal toDecimal(double v);

// Add two doubles without binary‐fp rounding artifacts
double addFloatsMaintainPrecesion(double float1, double float2);

// Subtract two doubles without binary‐fp rounding artifacts
double subtractFloatsMaintainPrecesion(double float1, double float2);

std::optional< double > doubleOrNone(const std::string &item);

std::optional< double > doubleOrNone(double item);

std::optional< std::string > strOrNone(const std::string &item, const std::string &encoding = "utf-8");

std::optional< std::string > strOrNone(double item, const std::string &encoding = "utf-8");

std::optional< std::string > strOrNone(const char *item, const std::string &encoding = "utf-8");

std::string convertToEnvName(const std::string &name);

// Formats a number as a currency string with thousands separators (US locale).
// Parameters:
//   num: The number to format
// Returns: Formatted string (e.g., "1,234,567.89")
std::string formatCurrency(double num);

// Generates a unique identifier using Boost UUID v4.
// Returns: 36-character string (e.g., "550e8400-e29b-41d4-a716-446655440000")
const boost::uuids::uuid generateUUID();

// Generates a short unique identifier (first 22 characters of a UUID).
// Returns: 22-character string (e.g., "550e8400-e29b-41d4-a7")
std::string generateShortUniqueId();

bool isValidUUID(const std::string &uuid_to_test, int version = 4);

/**
 * @brief Generate random string
 * @param numCharacters Length of string
 * @return std::string Random string
 */
std::string randomStr(size_t num_characters = 8);

// Converts a timestamp (milliseconds) to a UTC time point (equivalent to Arrow
// object). Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Time point (std::chrono::system_clock::time_point)
std::chrono::system_clock::time_point timestampToTimePoint(int64_t timestamp);

// Converts a timestamp (milliseconds) to a date (YYYY-MM-DD).
// Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Date (e.g., "2021-01-05")
std::chrono::time_point< std::chrono::system_clock, date::days > timestampToDate(int64_t timestamp);

// Converts a timestamp (milliseconds) to a date string (YYYY-MM-DD).
// Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Date string (e.g., "2021-01-05")
std::string timestampToDateStr(int64_t timestamp);

// Converts a timestamp (milliseconds) to a full datetime string (YYYY-MM-DD
// HH:MM:SS). Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Datetime string (e.g., "2021-01-05 00:00:00")
std::string timestampToTime(int64_t timestamp);

// Converts a timestamp (milliseconds) to an ISO 8601 string.
// Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: ISO 8601 string (e.g., "2021-01-05T00:00:00.000Z")
std::string timestampToIso8601(int64_t timestamp);

// Converts an ISO 8601 string to a timestamp (milliseconds).
// Parameters:
//   iso8601: ISO 8601 string (e.g., "2021-01-05T00:00:00.000Z")
// Returns: Milliseconds since Unix epoch
// Throws: std::invalid_argument if format is invalid
int64_t iso8601ToTimestamp(const std::string &iso8601);

// Returns today's UTC timestamp (beginning of day) in milliseconds.
// Returns: Milliseconds since Unix epoch
int64_t todayToTimestamp();

// Returns the current UTC timestamp in milliseconds.
// Parameters:
//   force_fresh: If true, always get fresh timestamp. If false, use cached time
//               when not in live trading or importing candles.
// Returns: Current UTC timestamp in milliseconds
int64_t nowToTimestamp(bool force_fresh = false);

// Returns the current UTC datetime as a system_clock time point.
// Returns: Current UTC datetime
std::chrono::system_clock::time_point nowToDateTime();

/**
 * @brief Convert seconds to human readable duration
 * @param seconds Number of seconds
 * @param granularity Number of units to include
 * @return std::string Human readable duration
 */
std::string readableDuration(int64_t seconds, size_t granularity = 2);

// Abstract Strategy base class
class Strategy
{
   public:
    virtual ~Strategy()    = default;
    virtual void execute() = 0; // Example method; extend as needed
};

// Factory for loading strategies
class StrategyLoader
{
   public:
    static StrategyLoader &getInstance();

    [[nodiscard]]
    std::pair< std::unique_ptr< Strategy >, void * > getStrategy(const std::string &name) const;

    void setBasePath(const std::filesystem::path &path) { base_path_ = path; }

    void setTestingMode(bool is_testing) { is_testing_ = is_testing; }

    // Add include/library paths for custom builds
    void setIncludePath(const std::filesystem::path &path) { include_path_ = path; }

    void setLibraryPath(const std::filesystem::path &path) { library_path_ = path; }

    // Grant test suite access to private members
    friend class ::StrategyLoaderTest;

   private:
    StrategyLoader() = default;

    [[nodiscard]]
    std::pair< std::unique_ptr< Strategy >, void * > loadStrategy(const std::string &name) const;

    [[nodiscard]] std::optional< std::filesystem::path > resolveModulePath(const std::string &name) const;

    [[nodiscard]]
    std::pair< std::unique_ptr< Strategy >, void * > loadFromDynamicLib(const std::filesystem::path &path) const;

    [[nodiscard]]
    std::pair< std::unique_ptr< Strategy >, void * > adjustAndReload(const std::string &name,
                                                                     const std::filesystem::path &source_path) const;

    [[nodiscard]]
    std::pair< std::unique_ptr< Strategy >, void * > createFallback(const std::string &name,
                                                                    const std::filesystem::path &module_path) const;

    std::filesystem::path base_path_    = std::filesystem::current_path();
    bool is_testing_                    = false;
    std::filesystem::path include_path_ = "include"; // Default include path
    std::filesystem::path library_path_ = "lib";     // Default lib path
};

[[nodiscard]] std::string computeSecureHash(std::string_view msg);

template < typename T >
[[nodiscard]] std::vector< T > insertList(size_t index, const T &item, const std::vector< T > &arr);

template < typename MapType >
[[nodiscard]] MapType mergeMaps(const MapType &d1, const MapType &d2);

[[nodiscard]] std::string getAppMode();

[[nodiscard]] bool isBacktesting();

[[nodiscard]] bool isDebuggable(const std::string &debug_item);

[[nodiscard]] bool isDebugging();

[[nodiscard]] bool isImportingCandles();

[[nodiscard]] bool isLive();

[[nodiscard]] bool isLiveTrading();

[[nodiscard]] bool isPaperTrading();

[[nodiscard]] bool isOptimizing();

[[nodiscard]] bool shouldExecuteSilently();

std::string makeKey(const enums::ExchangeName &exchange_name,
                    const std::string &symbol,
                    const std::optional< timeframe::Timeframe > &timeframe = std::nullopt);

timeframe::Timeframe maxTimeframe(const std::vector< timeframe::Timeframe > &timeframes);

template < typename T >
T scaleToRange(T old_max, T old_min, T new_max, T new_min, T old_value);

template < typename T >
T normalize(T x, T x_min, T x_max);

/**
 * @brief Get opposite side of a trade
 * @param side Trade side ("buy" or "sell")
 * @return enums::Side Opposite side
 * @throws std::invalid_argument if side is invalid
 */
enums::OrderSide oppositeSide(const enums::OrderSide &side);

/**
 * @brief Get opposite trade type
 * @param type PositionType type ("long" or "short")
 * @return enums::PositionType Opposite type
 * @throws std::invalid_argument if type is invalid
 */
enums::PositionType oppositePositionType(const enums::PositionType &position_type);

enums::PositionType orderSideToPositionType(const enums::OrderSide &order_side);

enums::OrderSide positionTypeToOrderSide(const enums::PositionType &position_type);

enums::OrderSide closingSide(const enums::PositionType &position_type);

/**
 * @brief Get current 1-minute candle timestamp in UTC
 * @return int64_t Timestamp in milliseconds
 */
int64_t current1mCandleTimestamp();

/**
 * @brief Forward fill NaN values in a matrix along specified axis
 * @param matrix Input matrix
 * @param axis Axis along which to fill (0 for rows, 1 for columns)
 * @return blaze::DynamicMatrix<T> Matrix with forward-filled values
 */
template < typename T >
blaze::DynamicMatrix< T > forwardFill(const blaze::DynamicMatrix< T > &matrix, size_t axis = 0);

/**
 * @brief Shift matrix elements by specified positions
 * @param matrix Input matrix
 * @param shift Number of positions to shift (positive for forward, negative
 * for backward)
 * @param fillValue Value to fill empty positions
 * @return blaze::DynamicMatrix<T> Shifted matrix
 */
template < typename T >
blaze::DynamicMatrix< T > shift(const blaze::DynamicMatrix< T > &matrix, int shift, T fill_value = T());

template < typename T >
blaze::DynamicVector< T, blaze::rowVector > shift(const blaze::DynamicVector< T, blaze::rowVector > &vector,
                                                  int shift,
                                                  T fill_value = T());

template < typename T >
blaze::DynamicMatrix< T > sameLength(const blaze::DynamicMatrix< T > &bigger, const blaze::DynamicMatrix< T > &shorter);

template < typename MT >
bool matricesEqualWithTolerance(const MT &a, const MT &b, double tolerance = 1e-9);
// TODO: matricesEqualWithNaN function

/**
 * @brief Binary search for orderbook insertion index
 * @param arr Orderbook array
 * @param target Target price
 * @param ascending Sort order
 * @return std::tuple<bool, size_t> {found, index}
 */
template < typename MT >
std::tuple< bool, size_t > findOrderbookInsertionIndex(const MT &arr, double target, bool ascending = true);

extern const std::function< double(const std::string &) > strToDouble;
extern const std::function< float(const std::string &) > strToFloat;

template < typename InputType,
           typename OutputType,
           typename Converter = std::function< OutputType(const InputType &) > >
std::vector< std::vector< OutputType > > cleanOrderbookList(
    const std::vector< std::vector< InputType > > &arr,
    Converter convert = [](const InputType &x) { return static_cast< OutputType >(x); });

template < typename T >
blaze::DynamicMatrix< T > sliceCandles(const blaze::DynamicMatrix< T > &candles, bool sequential);

int64_t getCandleStartTimestampBasedOnTimeframe(const timeframe::Timeframe &timeframe, int64_t num_candles_to_fetch);

/**
 * @brief Prepare quantity based on side
 * @param qty Input quantity
 * @param side enums::Side Trade side // TODO:
 * @return double Prepared quantity
 * @throws std::invalid_argument if side is invalid
 */
double prepareQty(double qty, const std::string &side);

bool isPriceNear(double order_price, double price_to_compare, double percentage_threshold = 0.00015);

std::string getSessionId();

void terminateApp();

void dump();
template < typename T >
void dump(const T &item);

template < typename T >
void dump(const std::vector< T > &vec);

template < typename T, typename... Args >
void dump(const T &first, const Args &...rest);

template < typename... Args >
void dump(const Args &...items);

void dumpAndTerminate(const std::string &item);

bool isCiphertraderProject();

std::string getOs();

bool isDocker();

pid_t getPid();

size_t getCpuCoresCount();

template < typename T >
std::string getClassName();

std::string gzipCompress(const std::string &data);

nlohmann::json compressedResponse(const std::string &content);

bool isUnitTesting();

} // namespace helper
} // namespace ct

#endif
