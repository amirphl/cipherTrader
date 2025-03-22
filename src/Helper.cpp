#include "Helper.hpp"
#include "Config.hpp"
#include "Info.hpp"
#include "Route.hpp"
#include <chrono>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include <gtest/gtest.h>

namespace Helper {

bool is_unit_testing() {
  return ::testing::UnitTest::GetInstance()->current_test_info() != nullptr;
}

std::string quoteAsset(const std::string &symbol) {
  size_t pos = symbol.find('-');
  if (pos == std::string::npos) {
    throw std::invalid_argument("Symbol is invalid");
  }

  return symbol.substr(pos + 1);
}

std::string baseAsset(const std::string &symbol) {
  size_t pos = symbol.find('-');
  if (pos == std::string::npos) {
    return symbol; // Return original string if no '-' found
  }
  return symbol.substr(0, pos);
}

std::string appCurrency() {
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

long long toTimestamp(std::chrono::system_clock::time_point tp) {
  auto duration = tp.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

template <typename T>
int binarySearch(const std::vector<T> &arr, const T &item) {
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

template int binarySearch(const std::vector<int> &, const int &);
template int binarySearch(const std::vector<std::string> &,
                          const std::string &);

template <typename InputType, typename OutputType, typename Converter>
std::vector<std::vector<OutputType>>
cleanOrderbookList(const std::vector<std::vector<InputType>> &arr,
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

const std::function<double(const std::string &)> strToDouble =
    std::bind(static_cast<double (*)(const std::string &, size_t *)>(std::stod),
              std::placeholders::_1, nullptr);

const std::function<float(const std::string &)> strToFloat = std::bind(
    static_cast<float (*)(const std::string &, std::size_t *)>(std::stof),
    std::placeholders::_1, nullptr);

// std::string to double
template std::vector<std::vector<double>>
cleanOrderbookList(const std::vector<std::vector<std::string>> &arr,
                   decltype(strToDouble));

// std::string to float
template std::vector<std::vector<float>>
cleanOrderbookList(const std::vector<std::vector<std::string>> &arr,
                   decltype(strToFloat));

// int to double with static_cast
template std::vector<std::vector<double>>
cleanOrderbookList(const std::vector<std::vector<int>> &arr,
                   std::function<double(const int &)> convert);

// int to float with static_cast
template std::vector<std::vector<float>>
cleanOrderbookList(const std::vector<std::vector<int>> &arr,
                   std::function<float(const int &)> convert);

std::string color(const std::string &msg_text, const std::string &msg_color) {
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
T scaleToRange(T oldMax, T oldMin, T newMax, T newMin, T oldValue) {
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

template int scaleToRange(int oldMax, int oldMin, int newMax, int newMin,
                          int oldValue);
template float scaleToRange(float oldMax, float oldMin, float newMax,
                            float newMin, float oldValue);
template double scaleToRange(double oldMax, double oldMin, double newMax,
                             double newMin, double oldValue);

std::string dashlessSymbol(const std::string &symbol) {
  std::string result = symbol;
  result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
  return result;
}

bool endsWith(const std::string &symbol, const std::string &s) {
  return symbol.length() >= s.length() &&
         symbol.substr(symbol.length() - s.length()) == s;
}

std::string dashySymbol(const std::string &symbol) {
  // If already has '-' in symbol, return symbol
  if (symbol.find('-') != std::string::npos) {
    return symbol;
  }

  // Access config["app"]["considering_symbols"] (assumed to be an array of
  // strings)
  if (Config::config.contains("app") &&
      Config::config["app"].contains("considering_symbols")) {
    for (const auto &s : Config::config["app"]["considering_symbols"]) {
      std::string compare_symbol = dashlessSymbol(s.get<std::string>());
      if (compare_symbol == symbol) {
        return s.get<std::string>();
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

std::string underlineToDashySymbol(const std::string &symbol) {
  std::string result = symbol;
  std::replace(result.begin(), result.end(), '_', '-');
  return result;
}

std::string dashyToUnderline(const std::string &symbol) {
  std::string result = symbol;
  std::replace(result.begin(), result.end(), '-', '_');
  return result;
}

int dateDiffInDays(const std::chrono::system_clock::time_point &date1,
                   const std::chrono::system_clock::time_point &date2) {
  // Calculate difference in hours and convert to days
  auto diff = std::chrono::duration_cast<std::chrono::hours>(date2 - date1);
  int days = static_cast<int>(diff.count() / 24);
  return std::abs(days);
}

// long long dateToTimestamp(const std::string &date) {
//   std::tm tm = {};
//   std::istringstream ss(date);
//   ss >> std::get_time(&tm, "%Y-%m-%d");

//   if (ss.fail()) {
//     throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
//   }

//   tm.tm_isdst = 0;                          // No daylight saving
//   time_t local_time = std::mktime(&tm);     // Local time
//   auto utc_time = std::gmtime(&local_time); // UTC tm
//   // Use timegm or manual adjustment instead of mktime for UTC
// #ifdef __linux__ // timegm is POSIX, not standard C++
//   time_t utc_time_t = timegm(utc_time);
// #else
//   // Manual UTC adjustment (approximate, assumes no DST)
//   time_t utc_time_t = std::mktime(utc_time) - timezone;
// #endif
//   auto tp = std::chrono::system_clock::from_time_t(utc_time_t);
//   return toTimestamp(tp);
// }

// long long dateToTimestamp(const std::string &date) {
//   std::tm tm = {};
//   std::istringstream ss(date);
//   ss >> std::get_time(&tm, "%Y-%m-%d");

//   if (ss.fail()) {
//     throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
//   }

//   // Set to UTC explicitly
//   tm.tm_isdst = 0; // No daylight saving

// #ifdef __linux__ // Use timegm on POSIX systems
//   time_t utc_time = timegm(&tm);
// #else
//   // Portable UTC adjustment: mktime gives local time, adjust by timezone
//   offset time_t local_time = std::mktime(&tm); std::tm *utc_tm =
//   std::gmtime(&local_time); time_t utc_time = std::mktime(utc_tm);
//   // Correct for double local time application
//   utc_time = local_time + (local_time - utc_time);
// #endif

//   auto tp = std::chrono::system_clock::from_time_t(utc_time);
//   return toTimestamp(tp);
// }

// long long dateToTimestamp(const std::string &date) {
//   std::tm tm = {};
//   std::istringstream ss(date);
//   ss >> std::get_time(&tm, "%Y-%m-%d");

//   if (ss.fail()) {
//     throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
//   }

//   // Extract original input components for validation
//   int year, month, day;
//   char dash1, dash2;
//   std::istringstream validate_ss(date);
//   validate_ss >> year >> dash1 >> month >> dash2 >> day;

//   // Check format and bounds
//   if (dash1 != '-' || dash2 != '-' || year != tm.tm_year + 1900 ||
//       month != tm.tm_mon + 1 || day != tm.tm_mday) {
//     throw std::invalid_argument("Invalid date: day or month out of range");
//   }

//   // Validate month and day ranges
//   if (month < 1 || month > 12) {
//     throw std::invalid_argument("Invalid date: month out of range");
//   }

//   // Days in each month (non-leap year)
//   static const std::array<int, 12> days_in_month = {31, 28, 31, 30, 31, 30,
//                                                     31, 31, 30, 31, 30, 31};
//   int max_days = days_in_month[month - 1];

//   // Adjust February for leap years
//   if (month == 2) {
//     bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
//     if (is_leap)
//       max_days = 29;
//   }

//   if (day < 1 || day > max_days) {
//     throw std::invalid_argument("Invalid date: day out of range");
//   }

//   tm.tm_isdst = 0; // No daylight saving

// #ifdef __linux__
//   time_t utc_time = timegm(&tm);
// #else
//   time_t local_time = std::mktime(&tm);
//   std::tm *utc_tm = std::gmtime(&local_time);
//   time_t utc_time = std::mktime(utc_tm);
//   utc_time = local_time + (local_time - utc_time);
// #endif

//   auto tp = std::chrono::system_clock::from_time_t(utc_time);
//   return toTimestamp(tp);
// }

long long dateToTimestamp(const std::string &date) {
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

  tm.tm_isdst = 0;

#ifdef __linux__
  time_t utc_time = timegm(&tm);
#else
  time_t local_time = std::mktime(&tm);
  std::tm *utc_tm = std::gmtime(&local_time);
  time_t utc_time = std::mktime(utc_tm);
  utc_time = local_time + (local_time - utc_time);
#endif

  auto tp = std::chrono::system_clock::from_time_t(utc_time);
  return toTimestamp(tp);
}

std::map<std::string, std::variant<int, float>>
dnaToHp(const nlohmann::json &strategy_hp, const std::string &dna) {
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

} // namespace Helper
