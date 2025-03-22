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

long long dateToTimestamp(const std::string &date);

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

} // namespace Helper

#endif
