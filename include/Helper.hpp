#ifndef HELPER_HPP
#define HELPER_HPP

#include <nlohmann/json.hpp>

namespace Helper {

bool is_unit_testing();

std::string quoteAsset(const std::string &symbol);

std::string baseAsset(const std::string &symbol);

std::string appCurrency();

long long toTimestamp(std::chrono::system_clock::time_point tp);

template <typename T>
int binarySearch(const std::vector<T> &arr, const T &item);

extern const std::function<double(const std::string &)> strToDouble;
extern const std::function<float(const std::string &)> strToFloat;

template <typename InputType, typename OutputType,
          typename Converter = std::function<OutputType(const InputType &)>>
std::vector<std::vector<OutputType>> cleanOrderbookList(
    const std::vector<std::vector<InputType>> &arr,
    Converter convert = [](const InputType &x) {
      return static_cast<OutputType>(x);
    });

std::string color(const std::string &msg_text, const std::string &msg_color);

template <typename T>
T scaleToRange(T oldMax, T oldMin, T newMax, T newMin, T oldValue);

std::string dashlessSymbol(const std::string &symbol);

bool endsWith(const std::string &symbol, const std::string &s);

std::string dashySymbol(const std::string &symbol);

std::string underlineToDashySymbol(const std::string &symbol);

std::string dashyToUnderline(const std::string &symbol);

int dateDiffInDays(const std::chrono::system_clock::time_point &date1,
                   const std::chrono::system_clock::time_point &date2);

long long toTimestamp(const std::string &date);

std::map<std::string, std::variant<int, float>>
dnaToHp(const nlohmann::json &strategy_hp, const std::string &dna);

float estimateAveragePrice(float order_qty, float order_price,
                           float current_qty, float current_entry_price);

// Estimates the profit/loss (PNL) for a trade.
// Parameters:
//   qty: Quantity of the trade (absolute value used)
//   entry_price: Price at trade entry
//   exit_price: Price at trade exit
//   trade_type: "long" or "short"
//   trading_fee: Fee per unit qty per price (default 0)
// Returns: PNL in currency units (profit - fees)
// Throws: std::invalid_argument if trade_type is invalid or qty is zero after
// abs
float estimatePNL(float qty, float entry_price, float exit_price,
                  const std::string &trade_type, float trading_fee = 0.0f);

// Estimates the PNL as a percentage of the initial investment.
// Parameters:
//   qty: Quantity of the trade (absolute value used)
//   entry_price: Price at trade entry
//   exit_price: Price at trade exit
//   trade_type: "long" or "short"
// Returns: PNL as a percentage
// Throws: std::invalid_argument if trade_type is invalid or qty * entry_price
// is zero
float estimatePNLPercentage(float qty, float entry_price, float exit_price,
                            const std::string &trade_type);

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

// Floors a number to the specified precision.
// Parameters:
//   num: The number to floor
//   precision: Number of decimal places (non-negative)
// Returns: Floored number
// Throws: std::invalid_argument if precision < 0
double floorWithPrecision(double num, int precision = 0);

// Formats a number as a currency string with thousands separators (US locale).
// Parameters:
//   num: The number to format
// Returns: Formatted string (e.g., "1,234,567.89")
std::string formatCurrency(double num);

// Generates a unique identifier using Boost UUID v4.
// Returns: 36-character string (e.g., "550e8400-e29b-41d4-a716-446655440000")
std::string generateUniqueId();

// Generates a short unique identifier (first 22 characters of a UUID).
// Returns: 22-character string (e.g., "550e8400-e29b-41d4-a7")
std::string generateShortUniqueId();

// Converts a timestamp (milliseconds) to a UTC time point (equivalent to Arrow
// object). Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Time point (std::chrono::system_clock::time_point)
std::chrono::system_clock::time_point timestampToTimePoint(int64_t timestamp);

// Converts a timestamp (milliseconds) to a date string (YYYY-MM-DD).
// Parameters:
//   timestamp: Milliseconds since Unix epoch
// Returns: Date string (e.g., "2021-01-05")
std::string timestampToDate(int64_t timestamp);

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

} // namespace Helper

#endif
