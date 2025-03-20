#ifndef HELPER_HPP
#define HELPER_HPP

#include <functional>

namespace Helper {

bool is_unit_testing();

std::string quoteAsset(const std::string &symbol);

std::string baseAsset(const std::string &symbol);

std::string appCurrency();

long long toTimestamp(std::chrono::system_clock::time_point tp);

std::string color(const std::string &msg_text, const std::string &msg_color);

template <typename InputType, typename OutputType,
          typename Converter = std::function<OutputType(const InputType &)>>
std::vector<std::vector<OutputType>> cleanOrderbookList(
    const std::vector<std::vector<InputType>> &arr,
    Converter convert = [](const InputType &x) {
      return static_cast<OutputType>(x);
    });

template <typename T>
T scaleToRange(T oldMax, T oldMin, T newMax, T newMin, T oldValue);

} // namespace Helper

#endif
