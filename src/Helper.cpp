#include "Helper.hpp"
#include "Info.hpp"
#include "Route.hpp"

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
  auto seconds =
      std::chrono::duration_cast<std::chrono::seconds>(duration).count();
  return seconds * 1000;
}

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

} // namespace Helper
