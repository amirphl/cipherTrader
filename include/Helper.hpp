#ifndef HELPER_HPP
#define HELPER_HPP

#include <functional>

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

} // namespace Helper

#endif
