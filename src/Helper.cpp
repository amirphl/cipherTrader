#include "Helper.hpp"
#include "Config.hpp"
#include "Info.hpp"
#include "Route.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <date/date.h>
#include <dlfcn.h>
#include <fstream>
#include <openssl/sha.h>
#include <regex>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include <gtest/gtest.h>

std::string Helper::quoteAsset(const std::string &symbol) {
  size_t pos = symbol.find('-');
  if (pos == std::string::npos) {
    throw std::invalid_argument("Symbol is invalid");
  }

  return symbol.substr(pos + 1);
}

std::string Helper::baseAsset(const std::string &symbol) {
  size_t pos = symbol.find('-');
  if (pos == std::string::npos) {
    return symbol; // Return original string if no '-' found
  }
  return symbol.substr(0, pos);
}

std::string Helper::appCurrency() {
  auto route = Route::Router::getInstance().getRoute(0);

  using namespace Info;

  auto exchange = route.exchange;
  if (EXCHANGE_INFO.find(exchange) != EXCHANGE_INFO.end() &&
      EXCHANGE_INFO.at(exchange).find("settlement_currency") !=
          EXCHANGE_INFO.at(exchange).end()) {

    auto res = EXCHANGE_INFO.at(exchange).at("settlement_currency");
    return toString(res);
  }

  return quoteAsset(route.symbol);
}

long long Helper::toTimestamp(std::chrono::system_clock::time_point tp) {
  auto duration = tp.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

template <typename T>
int Helper::binarySearch(const std::vector<T> &arr, const T &item) {
  int left = 0;
  int right = static_cast<int>(arr.size()) - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (arr[mid] == item) {
      return mid;
    } else if (arr[mid] < item) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return -1;
}

template int Helper::binarySearch(const std::vector<int> &, const int &);
template int Helper::binarySearch(const std::vector<std::string> &,
                                  const std::string &);

template <typename InputType, typename OutputType, typename Converter>
std::vector<std::vector<OutputType>>
Helper::cleanOrderbookList(const std::vector<std::vector<InputType>> &arr,
                           Converter convert) {

  std::vector<std::vector<OutputType>> result;
  result.reserve(arr.size());

  for (const auto &inner : arr) {
    if (inner.size() < 2) {
      throw std::invalid_argument(
          "Each inner vector must have at least 2 elements");
    }

    try {
      result.push_back({convert(inner[0]), convert(inner[1])});
    } catch (const std::exception &e) {
      throw std::invalid_argument("Conversion failed: " +
                                  std::string(e.what()));
    }
  }

  return result;
}

const std::function<double(const std::string &)> Helper::strToDouble =
    std::bind(static_cast<double (*)(const std::string &, size_t *)>(std::stod),
              std::placeholders::_1, nullptr);

const std::function<float(const std::string &)> Helper::strToFloat = std::bind(
    static_cast<float (*)(const std::string &, std::size_t *)>(std::stof),
    std::placeholders::_1, nullptr);

// std::string to double
template std::vector<std::vector<double>>
Helper::cleanOrderbookList(const std::vector<std::vector<std::string>> &arr,
                           decltype(strToDouble));

// std::string to float
template std::vector<std::vector<float>>
Helper::cleanOrderbookList(const std::vector<std::vector<std::string>> &arr,
                           decltype(strToFloat));

// int to double with static_cast
template std::vector<std::vector<double>>
Helper::cleanOrderbookList(const std::vector<std::vector<int>> &arr,
                           std::function<double(const int &)> convert);

// int to float with static_cast
template std::vector<std::vector<float>>
Helper::cleanOrderbookList(const std::vector<std::vector<int>> &arr,
                           std::function<float(const int &)> convert);

std::string Helper::color(const std::string &msg_text,
                          const std::string &msg_color) {
  if (msg_text.empty()) {
    return "";
  }

#ifdef _WIN32
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(hConsole, &csbi);
  WORD default_color = csbi.wAttributes;

  if (msg_color == "black")
    SetConsoleTextAttribute(hConsole, 0);
  else if (msg_color == "red")
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
  else if (msg_color == "green")
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
  else if (msg_color == "yellow")
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
  else if (msg_color == "blue")
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
  else if (msg_color == "magenta")
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE);
  else if (msg_color == "cyan")
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE);
  else if (msg_color == "white" || msg_color == "gray")
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN |
                                          FOREGROUND_BLUE);
  else
    throw std::invalid_argument("unsupported color");

  std::string result = msg_text;
  SetConsoleTextAttribute(hConsole, default_color); // Reset
  return result;
#else
  const std::string reset = "\033[0m";
  if (msg_color == "black")
    return "\033[30m" + msg_text + reset;
  if (msg_color == "red")
    return "\033[31m" + msg_text + reset;
  if (msg_color == "green")
    return "\033[32m" + msg_text + reset;
  if (msg_color == "yellow")
    return "\033[33m" + msg_text + reset;
  if (msg_color == "blue")
    return "\033[34m" + msg_text + reset;
  if (msg_color == "magenta")
    return "\033[35m" + msg_text + reset;
  if (msg_color == "cyan")
    return "\033[36m" + msg_text + reset;
  if (msg_color == "white" || msg_color == "gray")
    return "\033[37m" + msg_text + reset;
  throw std::invalid_argument("unsupported color");
#endif
}

template <typename T>
T Helper::scaleToRange(T oldMax, T oldMin, T newMax, T newMin, T oldValue) {
  static_assert(std::is_arithmetic_v<T>, "Type must be numeric");

  if (oldValue > oldMax || oldValue < oldMin) {
    throw std::invalid_argument("Value out of range");
  }
  T oldRange = oldMax - oldMin;
  if (oldRange == 0) {
    throw std::invalid_argument("Old range cannot be zero");
  }
  T newRange = newMax - newMin;
  return (((oldValue - oldMin) * newRange) / oldRange) + newMin;
}

template int Helper::scaleToRange(int oldMax, int oldMin, int newMax,
                                  int newMin, int oldValue);
template float Helper::scaleToRange(float oldMax, float oldMin, float newMax,
                                    float newMin, float oldValue);
template double Helper::scaleToRange(double oldMax, double oldMin,
                                     double newMax, double newMin,
                                     double oldValue);

std::string Helper::dashlessSymbol(const std::string &symbol) {
  std::string result = symbol;
  result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
  return result;
}

bool Helper::endsWith(const std::string &symbol, const std::string &s) {
  return symbol.length() >= s.length() &&
         symbol.substr(symbol.length() - s.length()) == s;
}

std::string Helper::dashySymbol(const std::string &symbol) {
  // If already has '-' in symbol, return symbol
  if (symbol.find('-') != std::string::npos) {
    return symbol;
  }

  // Fetch considering_symbols as a ConfigValue
  auto symbolsVariant = Config::Config::getInstance().get(
      "app.considering_symbols", std::vector<std::string>{});

  // Check if it's a vector<string> and process it
  if (std::holds_alternative<std::vector<std::string>>(symbolsVariant)) {
    const auto &symbols = std::get<std::vector<std::string>>(symbolsVariant);
    for (const auto &s : symbols) {
      std::string compare_symbol = dashlessSymbol(s);
      if (compare_symbol == symbol) {
        return s; // Return the original symbol with dashes
      }
    }
  }

  // Check suffixes and add dash accordingly
  if (endsWith(symbol, "EUR")) {
    return symbol.substr(0, symbol.length() - 3) + "-EUR";
  }
  if (endsWith(symbol, "EUT")) {
    return symbol.substr(0, symbol.length() - 3) + "-EUT";
  }
  if (endsWith(symbol, "GBP")) {
    return symbol.substr(0, symbol.length() - 3) + "-GBP";
  }
  if (endsWith(symbol, "JPY")) {
    return symbol.substr(0, symbol.length() - 3) + "-JPY";
  }
  if (endsWith(symbol, "MIM")) {
    return symbol.substr(0, symbol.length() - 3) + "-MIM";
  }
  if (endsWith(symbol, "TRY")) {
    return symbol.substr(0, symbol.length() - 3) + "-TRY";
  }
  if (endsWith(symbol, "FDUSD")) {
    return symbol.substr(0, symbol.length() - 5) + "-FDUSD";
  }
  if (endsWith(symbol, "TUSD")) {
    return symbol.substr(0, symbol.length() - 4) + "-TUSD";
  }
  if (endsWith(symbol, "UST")) {
    return symbol.substr(0, symbol.length() - 3) + "-UST";
  }
  if (endsWith(symbol, "USDT")) {
    return symbol.substr(0, symbol.length() - 4) + "-USDT";
  }
  if (endsWith(symbol, "USDC")) {
    return symbol.substr(0, symbol.length() - 4) + "-USDC";
  }
  if (endsWith(symbol, "USDS")) {
    return symbol.substr(0, symbol.length() - 4) + "-USDS";
  }
  if (endsWith(symbol, "USDP")) {
    return symbol.substr(0, symbol.length() - 4) + "-USDP";
  }
  if (endsWith(symbol, "USDU")) {
    return symbol.substr(0, symbol.length() - 4) + "-USDU";
  }
  if (endsWith(symbol, "USD")) {
    return symbol.substr(0, symbol.length() - 3) + "-USD";
  }

  // Special case for SUSDT
  if (symbol.length() > 7 && endsWith(symbol, "SUSDT")) {
    return symbol.substr(0, symbol.length() - 5) + "-" +
           symbol.substr(symbol.length() - 5);
  }

  // Default case: split into 3 and rest
  if (symbol.length() <= 3) {
    return symbol; // Avoid out-of-range substring
  }
  return symbol.substr(0, 3) + "-" + symbol.substr(3);
}

std::string Helper::underlineToDashySymbol(const std::string &symbol) {
  std::string result = symbol;
  std::replace(result.begin(), result.end(), '_', '-');
  return result;
}

std::string Helper::dashyToUnderline(const std::string &symbol) {
  std::string result = symbol;
  std::replace(result.begin(), result.end(), '-', '_');
  return result;
}

int Helper::dateDiffInDays(const std::chrono::system_clock::time_point &date1,
                           const std::chrono::system_clock::time_point &date2) {
  // Calculate difference in hours and convert to days
  auto diff = std::chrono::duration_cast<std::chrono::hours>(date2 - date1);
  int days = static_cast<int>(diff.count() / 24);
  return std::abs(days);
}

long long Helper::toTimestamp(const std::string &date) {
  // Enforce exact "YYYY-MM-DD" format (10 chars: 4-2-2 with dashes)
  if (date.length() != 10 || date[4] != '-' || date[7] != '-') {
    throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
  }

  // Check for two-digit month and day with leading zeros
  if (!((date[5] >= '0' && date[5] <= '1') &&
        (date[6] >= '0' && date[6] <= '9')) ||
      !((date[8] >= '0' && date[8] <= '3') &&
        (date[9] >= '0' && date[9] <= '9'))) {
    throw std::invalid_argument(
        "Invalid date format. Expected YYYY-MM-DD with leading zeros");
  }

  std::tm tm = {};
  std::istringstream ss(date);
  ss >> std::get_time(&tm, "%Y-%m-%d");

  if (ss.fail()) {
    throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
  }

  int year, month, day;
  char dash1, dash2;
  std::istringstream validate_ss(date);
  validate_ss >> year >> dash1 >> month >> dash2 >> day;

  if (dash1 != '-' || dash2 != '-' || year != tm.tm_year + 1900 ||
      month != tm.tm_mon + 1 || day != tm.tm_mday) {
    throw std::invalid_argument("Invalid date: day or month out of range");
  }

  if (month < 1 || month > 12) {
    throw std::invalid_argument("Invalid date: month out of range");
  }

  static const std::array<int, 12> days_in_month = {31, 28, 31, 30, 31, 30,
                                                    31, 31, 30, 31, 30, 31};
  int max_days = days_in_month[month - 1];

  if (month == 2) {
    bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (is_leap)
      max_days = 29;
  }

  if (day < 1 || day > max_days) {
    throw std::invalid_argument("Invalid date: day out of range");
  }

  // Set to UTC midnight
  tm.tm_isdst = 0;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  // Convert to UTC timestamp using date library
  auto dp = date::year_month_day(date::year(year), date::month(month),
                                 date::day(day));
  auto tp = date::sys_days(dp);
  return toTimestamp(tp);
}

std::map<std::string, std::variant<int, float>>
Helper::dnaToHp(const nlohmann::json &strategy_hp, const std::string &dna) {
  if (!strategy_hp.is_array()) {
    throw std::invalid_argument("strategy_hp must be a JSON array");
  }
  if (dna.length() != strategy_hp.size()) {
    throw std::invalid_argument("DNA length must match strategy_hp size");
  }

  std::map<std::string, std::variant<int, float>> hp;
  for (size_t i = 0; i < dna.length(); ++i) {
    const auto &h = strategy_hp[i];
    if (!h.contains("name") || !h.contains("type") || !h.contains("min") ||
        !h.contains("max")) {
      throw std::invalid_argument(
          "Each strategy_hp entry must have name, type, min, and max");
    }

    auto name = h["name"].get<std::string>();
    auto type = h["type"].get<std::string>();
    auto min = h["min"].get<float>();
    auto max = h["max"].get<float>();
    auto gene = dna[i];
    auto ord_gene = static_cast<float>(gene);

    if (type == "int") {
      auto decoded_gene = static_cast<int>(
          std::round(scaleToRange(119.0f, 40.0f, max, min, ord_gene)));
      hp[name] = decoded_gene;
    } else if (type == "float") {
      auto decoded_gene = scaleToRange(119.0f, 40.0f, max, min, ord_gene);
      hp[name] = decoded_gene;
    } else {
      throw std::runtime_error("Only int and float types are implemented");
    }
  }
  return hp;
}

float Helper::estimateAveragePrice(float order_qty, float order_price,
                                   float current_qty,
                                   float current_entry_price) {
  float abs_order_qty = std::abs(order_qty);
  float abs_current_qty = std::abs(current_qty);
  float total_qty = abs_order_qty + abs_current_qty;

  if (total_qty == 0.0f) {
    throw std::invalid_argument("Total quantity cannot be zero");
  }

  return (abs_order_qty * order_price + abs_current_qty * current_entry_price) /
         total_qty;
}

float Helper::estimatePNL(float qty, float entry_price, float exit_price,
                          const std::string &trade_type,
                          float trading_fee) noexcept(false) {
  float abs_qty = std::abs(qty);
  if (abs_qty == 0.0f) {
    throw std::invalid_argument("Quantity cannot be zero");
  }

  // Optimize: Compute profit directly with multiplier
  float multiplier = (trade_type == "short") ? -1.0f : 1.0f;
  if (trade_type != "long" && trade_type != "short") {
    throw std::invalid_argument("trade_type must be 'long' or 'short'");
  }

  float profit = abs_qty * (exit_price - entry_price) * multiplier;
  float fee = (trading_fee > 0.0f)
                  ? trading_fee * abs_qty * (entry_price + exit_price)
                  : 0.0f;

  return profit - fee;
}

float Helper::estimatePNLPercentage(
    float qty, float entry_price, float exit_price,
    const std::string &trade_type) noexcept(false) {
  float abs_qty = std::abs(qty);
  if (abs_qty == 0.0f) {
    throw std::invalid_argument("Quantity cannot be zero");
  }

  float initial_investment = abs_qty * entry_price;
  if (initial_investment == 0.0f) {
    throw std::invalid_argument(
        "Initial investment (qty * entry_price) cannot be zero");
  }

  float multiplier = (trade_type == "short") ? -1.0f : 1.0f;
  if (trade_type != "long" && trade_type != "short") {
    throw std::invalid_argument("trade_type must be 'long' or 'short'");
  }

  float profit = abs_qty * (exit_price - entry_price) * multiplier;
  return (profit / initial_investment) * 100.0f;
}

bool Helper::fileExists(const std::string &path) {
  return std::filesystem::is_regular_file(path);
}

void Helper::clearFile(const std::string &path) {
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open or create file: " + path);
  }
  file.close(); // Explicit close for clarity, though destructor would handle it
}

void Helper::makeDirectory(const std::string &path) {
  if (std::filesystem::exists(path)) {
    if (std::filesystem::is_regular_file(path)) {
      throw std::runtime_error("Path exists as a file, not a directory: " +
                               path);
    }
    // If it's a directory, do nothing (success)
    return;
  }
  if (!std::filesystem::create_directories(path)) {
    throw std::runtime_error("Failed to create directory: " + path);
  }
}

double Helper::floorWithPrecision(double num, int precision) {
  if (precision < 0) {
    throw std::invalid_argument("Precision must be non-negative");
  }
  double factor = std::pow(10.0, precision);
  return std::floor(num * factor) / factor;
}

std::string Helper::formatCurrency(double num) {
  std::stringstream ss;
  try {
    ss.imbue(std::locale("en_US.UTF-8")); // Fixed US locale for commas
  } catch (const std::runtime_error &) {
    // Fallback to default locale if "en_US.UTF-8" is unavailable
    ss.imbue(std::locale(""));
  }
  ss << std::fixed << num;
  return ss.str();
}

std::string Helper::generateUniqueId() {
  boost::uuids::random_generator gen;
  boost::uuids::uuid id = gen();
  return boost::uuids::to_string(id);
}

std::string Helper::generateShortUniqueId() {
  std::string full_id = generateUniqueId();
  if (full_id.length() != 36) {
    throw std::runtime_error("Generated UUID length is not 36");
  }
  return full_id.substr(0, 22); // 8-4-4-2 format
}

std::chrono::system_clock::time_point
Helper::timestampToTimePoint(int64_t timestamp) {
  // Convert milliseconds since epoch to chrono duration
  auto duration = std::chrono::milliseconds(timestamp);
  return std::chrono::system_clock::time_point(duration);
}

std::string Helper::timestampToDate(int64_t timestamp) {
  auto tp = timestampToTimePoint(timestamp);
  auto dp = date::floor<date::days>(tp);
  return date::format("%F", dp); // YYYY-MM-DD
}

std::string Helper::timestampToTime(int64_t timestamp) {
  auto tp = timestampToTimePoint(timestamp);
  auto dp = date::floor<std::chrono::seconds>(tp);
  return date::format("%F %T", dp); // YYYY-MM-DD HH:MM:SS
}

std::string Helper::timestampToIso8601(int64_t timestamp) {
  auto tp = timestampToTimePoint(timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tp.time_since_epoch()) %
            1000;
  std::ostringstream oss;
  oss << date::format("%FT%T", tp) << "." << std::setfill('0') << std::setw(3)
      << ms.count() << "Z";
  return oss.str();
}

int64_t Helper::iso8601ToTimestamp(const std::string &iso8601) {
  std::istringstream iss(iso8601);
  std::chrono::system_clock::time_point tp;
  std::string milliseconds;

  // Parse ISO 8601 with milliseconds (e.g., "2021-01-05T00:00:00.000Z")
  iss >> date::parse("%FT%T", tp);
  if (iss.fail()) {
    throw std::invalid_argument("Invalid ISO 8601 format: " + iso8601);
  }

  // Check for milliseconds and 'Z'
  if (iss.peek() == '.') {
    iss.ignore(); // Skip '.'
    iss >> std::setw(3) >> milliseconds;
    if (iss.fail() || milliseconds.length() != 3) {
      throw std::invalid_argument("Invalid milliseconds in ISO 8601: " +
                                  iso8601);
    }
  }
  if (iss.get() != 'Z') {
    throw std::invalid_argument("ISO 8601 must end with 'Z': " + iso8601);
  }

  auto duration = tp.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

int64_t Helper::todayToTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto dp = date::floor<date::days>(now);
  auto duration = dp.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

// FIXME: Remove following.
auto cachedTimestamp = Helper::toTimestamp(std::chrono::system_clock::now());
int64_t Helper::nowToTimestamp(bool force_fresh) {
  // If not forcing fresh timestamp and not in live trading/importing candles,
  // use cached time from config
  if (!force_fresh && !(isLive() || isImportingCandles())) {
    try {
      // TODO Read from store.
      return cachedTimestamp;
    } catch (const std::exception &) {
      // If config time not available, fall back to current time
      return toTimestamp(std::chrono::system_clock::now());
    }
  }

  // Get fresh UTC timestamp
  return toTimestamp(std::chrono::system_clock::now());
}

std::chrono::system_clock::time_point Helper::nowToDateTime() {
  return std::chrono::system_clock::now();
}

blaze::DynamicVector<double>
Helper::getCandleSource(const blaze::DynamicMatrix<double> &candles,
                        Candle::Source source_type) {
  // Check matrix dimensions (expect at least 6 columns: timestamp, open, close,
  // high, low, volume)
  if (candles.columns() < 6) {
    throw std::invalid_argument("Candles matrix must have at least 6 columns");
  }
  if (candles.rows() == 0) {
    throw std::invalid_argument("Candles matrix must have at least one row");
  }

  switch (source_type) {
  case Candle::Source::Close:
    return blaze::column(candles, 2); // Close prices
  case Candle::Source::High:
    return blaze::column(candles, 3); // High prices
  case Candle::Source::Low:
    return blaze::column(candles, 4); // Low prices
  case Candle::Source::Open:
    return blaze::column(candles, 1); // Open prices
  case Candle::Source::Volume:
    return blaze::column(candles, 5); // Volume
  case Candle::Source::HL2:
    return (blaze::column(candles, 3) + blaze::column(candles, 4)) /
           2.0; // (High + Low) / 2
  case Candle::Source::HLC3:
    return (blaze::column(candles, 3) + blaze::column(candles, 4) +
            blaze::column(candles, 2)) /
           3.0; // (High + Low + Close) / 3
  case Candle::Source::OHLC4:
    return (blaze::column(candles, 1) + blaze::column(candles, 3) +
            blaze::column(candles, 4) + blaze::column(candles, 2)) /
           4.0; // (Open + High + Low + Close) / 4
  default:
    throw std::invalid_argument("Unknown candle source type");
  }
}

Helper::StrategyLoader &Helper::StrategyLoader::getInstance() {
  static StrategyLoader loader;
  return loader;
}

std::pair<std::unique_ptr<Helper::Strategy>, void *>
Helper::StrategyLoader::getStrategy(const std::string &name) const {
  if (name.empty()) {
    throw std::invalid_argument("Strategy name cannot be empty");
  }
  return loadStrategy(name);
}

std::pair<std::unique_ptr<Helper::Strategy>, void *>
Helper::StrategyLoader::loadStrategy(const std::string &name) const {
  auto modulePath = resolveModulePath(name);
  if (!modulePath) {
    return {nullptr, nullptr};
  }

  auto [strategy, handle] = loadFromDynamicLib(*modulePath);
  if (!strategy && !is_testing_) { // TODO
    if (handle) {
      dlclose(handle);
    }

    std::filesystem::path sourcePath =
        base_path_ / "strategies" / name / "main.cpp";

    if (std::filesystem::exists(sourcePath)) {
      std::tie(strategy, handle) = adjustAndReload(name, sourcePath);
    }

    if (!strategy) {
      if (handle) {
        dlclose(handle);
      }

      return createFallback(name, *modulePath);
    }
  }

  return {std::move(strategy), std::move(handle)};
}

std::optional<std::filesystem::path>
Helper::StrategyLoader::resolveModulePath(const std::string &name) const {
  std::filesystem::path moduleDir;
  if (is_testing_) {
    moduleDir = base_path_.filename() == "ciphertrader-live"
                    ? base_path_ / "tests" / "strategies" / name
                    : base_path_ / "ciphertrader" / "strategies" / name;
  } else {
    moduleDir = base_path_ / "strategies" / name;
  }

  std::filesystem::path modulePath = moduleDir / (name + ".so");
  return std::filesystem::exists(modulePath) ? std::make_optional(modulePath)
                                             : std::nullopt;
}

std::pair<std::unique_ptr<Helper::Strategy>, void *>
Helper::StrategyLoader::loadFromDynamicLib(
    const std::filesystem::path &path) const {
  auto handle(dlopen(path.string().c_str(), RTLD_LAZY));
  if (!handle) {
    const char *error = dlerror();
    // TODO Log
    std::cerr << "dlopen error: " << (error ? error : "Unknown error")
              << std::endl;
    return {nullptr, nullptr};
  }

  using CreateFunc = Strategy *(*)();
  auto *create = reinterpret_cast<CreateFunc>(dlsym(handle, "createStrategy"));
  if (!create) {
    if (handle) {
      dlclose(handle);
    }

    const char *error = dlerror();
    // TODO Log
    std::cerr << "dlsym error: "
              << (error ? error : "Unable to find createStrategy symbol")
              << std::endl;
    return {nullptr, nullptr};
  }

  return {std::unique_ptr<Strategy>(create()), std::move(handle)};
}

std::pair<std::unique_ptr<Helper::Strategy>, void *>
Helper::StrategyLoader::adjustAndReload(
    const std::string &name, const std::filesystem::path &sourcePath) const {
  std::ifstream inFile(sourcePath);
  if (!inFile) {
    return {nullptr, nullptr};
  }

  std::string content((std::istreambuf_iterator<char>(inFile)),
                      std::istreambuf_iterator<char>());
  inFile.close();

  // Match class derived from Helper::Strategy
  std::regex classPattern(
      R"(class\s+(\w+)\s*:\s*public\s*Helper::Strategy\s*)");
  // std::regex classPattern(
  //     R"(^(class|struct)\s+(\w+(?:::\w+)*)\s*:\s*public\s*Helper::Strategy(?:\s*,.*)?)");
  std::smatch match;

  if (std::regex_search(content, match, classPattern) && match.size() > 1) {
    std::string oldClassName = match[1];
    if (oldClassName != name) {
      // std::string newContent = std::regex_replace(
      //     content, std::regex("class\\s+" + oldClassName), "class " + name);
      std::string newContent =
          std::regex_replace(content,
                             std::regex("(?:\\w+::)?class\\s+" + name +
                                        "\\s*:\\s*public\\s*Helper::Strategy"),
                             "class " + name + " : public Helper::Strategy");

      std::ofstream outFile(sourcePath);
      if (!outFile) {
        return {nullptr, nullptr};
      }
      outFile << newContent;
      outFile.close();

      // Compile with custom library and headers
      std::filesystem::path modulePath =
          sourcePath.parent_path() / (name + ".so");
      std::string includeFlag = "-I" + includePath_.string();
      std::string libFlag = "-L" + libraryPath_.string();
      // g++ -shared -pthread -ldl -fPIC -std=c++17 -Iinclude
      // -I/opt/homebrew/include   -L/opt/homebrew/lib -o libmy_trading_lib.so
      // src/*
      std::string cmd = "g++ -shared -pthread -ldl -fPIC -std=c++17 "
                        "-I/opt/homebrew/include " + // TODO
                        includeFlag +
                        " -L/opt/homebrew/lib -lssl -lcrypto " + // TODO
                        libFlag + " -o " + modulePath.string() + " " +
                        sourcePath.string();
      if (system(cmd.c_str()) == 0) {
        // Verify the .so exists after compilation
        if (std::filesystem::exists(modulePath)) {
          return loadFromDynamicLib(modulePath);
        }
      }
      // Log compilation failure in a real system; here we just return nullptr
      return {nullptr, nullptr};
    }
  }

  return {nullptr, nullptr};
}

std::pair<std::unique_ptr<Helper::Strategy>, void *>
Helper::StrategyLoader::createFallback(
    const std::string &, const std::filesystem::path &modulePath) const {
  auto handle(dlopen(modulePath.string().c_str(), RTLD_LAZY));
  if (!handle) {
    return {nullptr, nullptr};
  }

  // Try common factory names
  static const std::array<const char *, 2> factoryNames = {
      "createStrategy", "createDefaultStrategy"};

  for (const auto *factoryName : factoryNames) {
    using CreateFunc = Strategy *(*)();
    auto *create = reinterpret_cast<CreateFunc>(dlsym(handle, factoryName));
    if (create) {
      auto base = std::unique_ptr<Strategy>(create());

      // Wrapper class for fallback
      class NamedStrategy final : public Strategy {
      public:
        explicit NamedStrategy(std::unique_ptr<Strategy> &&base)
            : base_(std::move(base)) {}
        void execute() override {
          if (base_)
            base_->execute();
        }

      private:
        std::unique_ptr<Strategy> base_;
      };
      return {std::make_unique<NamedStrategy>(std::move(base)),
              std::move(handle)};
    }
  }

  if (handle) {
    dlclose(handle);
  }

  return {nullptr, nullptr};
}

std::string Helper::computeSecureHash(std::string_view msg) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, msg.data(), msg.length());
  SHA256_Final(digest, &ctx);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(digest[i]);
  }
  return ss.str();
}

template <typename T>
std::vector<T> Helper::insertList(size_t index, const T &item,
                                  const std::vector<T> &arr) {
  std::vector<T> result;
  result.reserve(arr.size() + 1); // Pre-allocate for efficiency

  if (index == static_cast<size_t>(-1)) {
    result = arr;
    result.push_back(item);
    return result;
  }

  // Throw exception if index is out of range (except for the special case of
  // appending)
  if (index > arr.size()) {
    throw std::out_of_range("Index out of range in insertList");
  }

  result.insert(result.end(), arr.begin(), arr.begin() + index);
  result.push_back(item);
  result.insert(result.end(), arr.begin() + index, arr.end());
  return result;
}

// Explicit template instantiations
template std::vector<int> Helper::insertList(size_t, const int &,
                                             const std::vector<int> &);
template std::vector<std::string>
Helper::insertList(size_t, const std::string &,
                   const std::vector<std::string> &);

template std::vector<std::pair<int, std::string>>
Helper::insertList(size_t, const std::pair<int, std::string> &,
                   const std::vector<std::pair<int, std::string>> &);

bool Helper::isBacktesting() {
  return std::get<std::string>(Config::Config::getInstance().get(
             "app.trading_mode")) == "backtest";
}

bool Helper::isDebuggable(const std::string &debugItem) {
  try {
    return isDebugging() && std::get<bool>(Config::Config::getInstance().get(
                                "env.logging." + debugItem));
  } catch (const std::exception &) {
    return false; // Default to true if key not found
  }
}

bool Helper::isDebugging() {
  return std::get<bool>(Config::Config::getInstance().get("app.debug_mode"));
}

bool Helper::isImportingCandles() {
  return std::get<std::string>(Config::Config::getInstance().get(
             "app.trading_mode")) == "candles";
}

bool Helper::isLive() { return isLiveTrading() || isPaperTrading(); }

bool Helper::isLiveTrading() {
  return std::get<std::string>(Config::Config::getInstance().get(
             "app.trading_mode")) == "livetrade";
}

bool Helper::isPaperTrading() {
  return std::get<std::string>(Config::Config::getInstance().get(
             "app.trading_mode")) == "papertrade";
}

bool Helper::isOptimizing() {
  return std::get<std::string>(Config::Config::getInstance().get(
             "app.trading_mode")) == "optimize";
}

bool Helper::isValidUUID(const std::string &uuid_to_test, int version) {
  try {
    boost::uuids::uuid uuid_obj =
        boost::uuids::string_generator()(uuid_to_test);
    return uuid_obj.version() == version &&
           uuid_to_test == boost::uuids::to_string(uuid_obj);
  } catch (const std::exception &) {
    return false;
  }
}

std::string
Helper::generateCompositeKey(const std::string &exchange,
                             const std::string &symbol,
                             const std::optional<Enum::Timeframe> &timeframe) {
  if (!timeframe) {
    return exchange + "-" + symbol;
  }
  return exchange + "-" + symbol + "-" + Enum::toString(*timeframe);
}

Enum::Timeframe
Helper::maxTimeframe(const std::vector<Enum::Timeframe> &timeframes) {
  // Define timeframe priority (higher index = higher priority)
  static const std::vector<Enum::Timeframe> timeframe_priority = {
      Enum::Timeframe::MINUTE_1,  Enum::Timeframe::MINUTE_3,
      Enum::Timeframe::MINUTE_5,  Enum::Timeframe::MINUTE_15,
      Enum::Timeframe::MINUTE_30, Enum::Timeframe::MINUTE_45,
      Enum::Timeframe::HOUR_1,    Enum::Timeframe::HOUR_2,
      Enum::Timeframe::HOUR_3,    Enum::Timeframe::HOUR_4,
      Enum::Timeframe::HOUR_6,    Enum::Timeframe::HOUR_8,
      Enum::Timeframe::HOUR_12,   Enum::Timeframe::DAY_1,
      Enum::Timeframe::DAY_3,     Enum::Timeframe::WEEK_1,
      Enum::Timeframe::MONTH_1};

  // Find the highest priority timeframe that exists in the input list
  for (auto it = timeframe_priority.rbegin(); it != timeframe_priority.rend();
       ++it) {
    if (std::find(timeframes.begin(), timeframes.end(), *it) !=
        timeframes.end()) {
      return *it;
    }
  }

  // If no timeframes found, return the lowest priority (MINUTE_1)
  return Enum::Timeframe::MINUTE_1;
}

template <typename T> T Helper::normalize(T x, T x_min, T x_max) {
  static_assert(std::is_arithmetic_v<T>, "Type must be arithmetic");
  if (x_max == x_min) {
    return T(0); // Avoid division by zero
  }
  return (x - x_min) / (x_max - x_min);
}

// Explicit template instantiations
template int Helper::normalize(int x, int x_min, int x_max);
template float Helper::normalize(float x, float x_min, float x_max);
template double Helper::normalize(double x, double x_min, double x_max);
