#ifndef HELPER_HPP
#define HELPER_HPP

#include <functional>
#include <map>
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

} // namespace Helper

#endif
