#include "Helper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <zlib.h>
#include "Config.hpp"
#include "Enum.hpp"
#include "Exception.hpp"
#include "Helper.hpp"
#include "Info.hpp"
#include "Route.hpp"
#include <boost/beast/core/detail/base64.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <date/date.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <gtest/gtest.h>

std::string CipherHelper::quoteAsset(const std::string &symbol)
{
    size_t pos = symbol.find('-');
    if (pos == std::string::npos)
    {
        throw std::invalid_argument("Symbol is invalid");
    }

    return symbol.substr(pos + 1);
}

std::string CipherHelper::baseAsset(const std::string &symbol)
{
    size_t pos = symbol.find('-');
    if (pos == std::string::npos)
    {
        return symbol; // Return original string if no '-' found
    }
    return symbol.substr(0, pos);
}

std::string CipherHelper::appCurrency()
{
    auto route = CipherRoute::Router::getInstance().getRoute(0);

    auto exchange = route.exchange;
    if (CipherInfo::EXCHANGE_INFO.find(exchange) != CipherInfo::EXCHANGE_INFO.end() &&
        CipherInfo::EXCHANGE_INFO.at(exchange).find("settlement_currency") !=
            CipherInfo::EXCHANGE_INFO.at(exchange).end())
    {
        auto res = CipherInfo::EXCHANGE_INFO.at(exchange).at("settlement_currency");
        return CipherInfo::toString(res);
    }

    return quoteAsset(route.symbol);
}

template < typename T >
int CipherHelper::binarySearch(const std::vector< T > &arr, const T &item)
{
    int left  = 0;
    int right = static_cast< int >(arr.size()) - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        if (arr[mid] == item)
        {
            return mid;
        }
        else if (arr[mid] < item)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return -1;
}

template int CipherHelper::binarySearch(const std::vector< int > &, const int &);

template int CipherHelper::binarySearch(const std::vector< std::string > &, const std::string &);

std::string CipherHelper::color(const std::string &msg_text, const std::string &msg_color)
{
    if (msg_text.empty())
    {
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
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
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

std::string CipherHelper::style(const std::string &msg_text, const std::string &msg_style)
{
    if (msg_style.empty())
    {
        return msg_text;
    }

    std::string lower_style = msg_style;
    std::transform(lower_style.begin(), lower_style.end(), lower_style.begin(), ::tolower);

    if (lower_style == "bold" || lower_style == "b")
    {
        // ANSI escape codes for bold
        return "\033[1m" + msg_text + "\033[0m";
    }
    else if (lower_style == "underline" || lower_style == "u")
    {
        // ANSI escape codes for underline
        return "\033[4m" + msg_text + "\033[0m";
    }
    else
    {
        throw std::invalid_argument("Unsupported style: " + msg_style);
    }
}

void CipherHelper::error(const std::string &msg, bool force_print)
{
    if (isLive() && !force_print)
    {
        // TODO: Log error in live mode
        // Note: Logging service should be implemented separately
        if (force_print)
        {
            std::cerr << "\n========== CRITICAL ERROR ==========\n"
                      << msg << "\n"
                      << "====================================\n";
        }
    }
    else
    {
        std::cerr << "\n========== CRITICAL ERROR ==========\n"
                  << msg << "\n"
                  << "====================================\n";
    }
}

template < typename... Args >
void CipherHelper::debug(const Args &...items)
{
    // Print to terminal using dump
    if constexpr (sizeof...(items) == 1)
    {
        dump(joinItems(items...));
    }
    else
    {
        dump(joinItems(items...));
    }

    // TODO: Implement logging to file (replacement for logger.info)
    // For now, simulate with a cout statement; replace with actual logging later
    std::cout << "TODO: Log this - " << joinItems(items...) << std::endl;
}

template void CipherHelper::debug();
template void CipherHelper::debug(char const(&));
template void CipherHelper::debug(char const (&)[1]);
template void CipherHelper::debug(char const (&)[5]);
template void CipherHelper::debug(char const (&)[7]);
template void CipherHelper::debug(char const *const &);
template void CipherHelper::debug(const std::string &);
template void CipherHelper::debug(char const (&)[5], int const &, double const &);
template void CipherHelper::debug(const std::string &, const int &, const float &);
template void CipherHelper::debug(const std::string &, const int &, const double &);
template void CipherHelper::debug(const int &, const unsigned int &, const long long &);

void CipherHelper::clearOutput()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// TODO: Clean this function.
template < typename... Args >
std::string CipherHelper::joinItems(const Args &...items)
{
    std::ostringstream oss;
    oss << "==> ";

    // Helper function to handle nullptr and output items
    auto outputItem = [&oss](const auto &item)
    {
        if constexpr (std::is_pointer_v< std::decay_t< decltype(item) > >)
        {
            if (item == nullptr)
            {
                oss << "(null)";
            }
            else
            {
                oss << item;
            }
        }
        else
        {
            oss << item;
        }
    };

    // Process each item
    int dummy[sizeof...(Args)] = {(outputItem(items), oss << (sizeof...(items) > 1 ? ", " : ""), 0)...};

    std::string result = oss.str();
    if (sizeof...(items) > 1 && !result.empty())
    {
        result = result.substr(0, result.length() - 2); // Remove trailing ", "
    }
    return result;
}

template std::string CipherHelper::joinItems();
template std::string CipherHelper::joinItems(char const(&));
template std::string CipherHelper::joinItems(const std::string &);
template std::string CipherHelper::joinItems(char const (&)[6], char const (&)[6]);
template std::string CipherHelper::joinItems(char const (&)[6], char const (&)[7]);
template std::string CipherHelper::joinItems(const std::string &, const std::string &);
template std::string CipherHelper::joinItems(const std::string &, const int &, const float &);
template std::string CipherHelper::joinItems(const std::string &, const int &, const double &);
template std::string CipherHelper::joinItems(const int &, const int &);
template std::string CipherHelper::joinItems(const int &, const unsigned int &, const long long &);

bool CipherHelper::endsWith(const std::string &symbol, const std::string &s)
{
    return symbol.length() >= s.length() && symbol.substr(symbol.length() - s.length()) == s;
}

std::string CipherHelper::dashlessSymbol(const std::string &symbol)
{
    std::string result = symbol;
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    return result;
}

std::string CipherHelper::dashySymbol(const std::string &symbol)
{
    // If already has '-' in symbol, return symbol
    if (symbol.find('-') != std::string::npos)
    {
        return symbol;
    }

    // Fetch considering_symbols as a ConfigValue
    auto symbols_variant =
        CipherConfig::Config::getInstance().get("app_considering_symbols", std::vector< std::string >{});

    // Check if it's a vector<string> and process it
    if (std::holds_alternative< std::vector< std::string > >(symbols_variant))
    {
        const auto &symbols = std::get< std::vector< std::string > >(symbols_variant);
        for (const auto &s : symbols)
        {
            std::string compare_symbol = dashlessSymbol(s);
            if (compare_symbol == symbol)
            {
                return s; // Return the original symbol with dashes
            }
        }
    }

    // Check suffixes and add dash accordingly
    if (endsWith(symbol, "EUR"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-EUR";
    }
    if (endsWith(symbol, "EUT"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-EUT";
    }
    if (endsWith(symbol, "GBP"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-GBP";
    }
    if (endsWith(symbol, "JPY"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-JPY";
    }
    if (endsWith(symbol, "MIM"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-MIM";
    }
    if (endsWith(symbol, "TRY"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-TRY";
    }
    if (endsWith(symbol, "FDUSD"))
    {
        return symbol.substr(0, symbol.length() - 5) + "-FDUSD";
    }
    if (endsWith(symbol, "TUSD"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-TUSD";
    }
    if (endsWith(symbol, "UST"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-UST";
    }
    if (endsWith(symbol, "USDT"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-USDT";
    }
    if (endsWith(symbol, "USDC"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-USDC";
    }
    if (endsWith(symbol, "USDS"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-USDS";
    }
    if (endsWith(symbol, "USDP"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-USDP";
    }
    if (endsWith(symbol, "USDU"))
    {
        return symbol.substr(0, symbol.length() - 4) + "-USDU";
    }
    if (endsWith(symbol, "USD"))
    {
        return symbol.substr(0, symbol.length() - 3) + "-USD";
    }

    // Special case for SUSDT
    if (symbol.length() > 7 && endsWith(symbol, "SUSDT"))
    {
        return symbol.substr(0, symbol.length() - 5) + "-" + symbol.substr(symbol.length() - 5);
    }

    // Default case: split into 3 and rest
    if (symbol.length() <= 3)
    {
        return symbol; // Avoid out-of-range substring
    }
    return symbol.substr(0, 3) + "-" + symbol.substr(3);
}

std::string CipherHelper::underlineToDashySymbol(const std::string &symbol)
{
    std::string result = symbol;
    std::replace(result.begin(), result.end(), '_', '-');
    return result;
}

std::string CipherHelper::dashyToUnderline(const std::string &symbol)
{
    std::string result = symbol;
    std::replace(result.begin(), result.end(), '-', '_');
    return result;
}

int CipherHelper::dateDiffInDays(const std::chrono::system_clock::time_point &date1,
                                 const std::chrono::system_clock::time_point &date2)
{
    // Calculate difference in hours and convert to days
    auto diff = std::chrono::duration_cast< std::chrono::hours >(date2 - date1);
    int days  = static_cast< int >(diff.count() / 24);
    return std::abs(days);
}

long long CipherHelper::toTimestamp(std::chrono::system_clock::time_point tp)
{
    auto duration = tp.time_since_epoch();
    return std::chrono::duration_cast< std::chrono::milliseconds >(duration).count();
}

// TODO: Make sure the following function works correctly.
long long CipherHelper::toTimestamp(const std::string &date)
{
    // Enforce exact "YYYY-MM-DD" format (10 chars: 4-2-2 with dashes)
    if (date.length() != 10 || date[4] != '-' || date[7] != '-')
    {
        throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
    }

    // Check for two-digit month and day with leading zeros
    if (!((date[5] >= '0' && date[5] <= '1') && (date[6] >= '0' && date[6] <= '9')) ||
        !((date[8] >= '0' && date[8] <= '3') && (date[9] >= '0' && date[9] <= '9')))
    {
        throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD with leading zeros");
    }

    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail())
    {
        throw std::invalid_argument("Invalid date format. Expected YYYY-MM-DD");
    }

    int year, month, day;
    char dash1, dash2;
    std::istringstream validate_ss(date);
    validate_ss >> year >> dash1 >> month >> dash2 >> day;

    if (dash1 != '-' || dash2 != '-' || year != tm.tm_year + 1900 || month != tm.tm_mon + 1 || day != tm.tm_mday)
    {
        throw std::invalid_argument("Invalid date: day or month out of range");
    }

    if (month < 1 || month > 12)
    {
        throw std::invalid_argument("Invalid date: month out of range");
    }

    static const std::array< int, 12 > days_in_month = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_days                                     = days_in_month[month - 1];

    if (month == 2)
    {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap)
            max_days = 29;
    }

    if (day < 1 || day > max_days)
    {
        throw std::invalid_argument("Invalid date: day out of range");
    }

    // Set to UTC midnight
    tm.tm_isdst = 0;
    tm.tm_hour  = 0;
    tm.tm_min   = 0;
    tm.tm_sec   = 0;

    // Convert to UTC timestamp using date library
    auto dp = date::year_month_day(date::year(year), date::month(month), date::day(day));
    auto tp = date::sys_days(dp);

    return toTimestamp(tp);
}

std::map< std::string, std::variant< int, float > > CipherHelper::dnaToHp(const nlohmann::json &strategy_hp,
                                                                          const std::string &dna)
{
    if (!strategy_hp.is_array())
    {
        throw std::invalid_argument("strategy_hp must be a JSON array");
    }
    if (dna.length() != strategy_hp.size())
    {
        throw std::invalid_argument("DNA length must match strategy_hp size");
    }

    std::map< std::string, std::variant< int, float > > hp;
    for (size_t i = 0; i < dna.length(); ++i)
    {
        const auto &h = strategy_hp[i];
        if (!h.contains("name") || !h.contains("type") || !h.contains("min") || !h.contains("max"))
        {
            throw std::invalid_argument("Each strategy_hp entry must have name, type, min, and max");
        }

        auto name     = h["name"].get< std::string >();
        auto type     = h["type"].get< std::string >();
        auto min      = h["min"].get< float >();
        auto max      = h["max"].get< float >();
        auto gene     = dna[i];
        auto ord_gene = static_cast< float >(gene);

        if (type == "int")
        {
            auto decoded_gene = static_cast< int >(std::round(scaleToRange(119.0f, 40.0f, max, min, ord_gene)));
            hp[name]          = decoded_gene;
        }
        else if (type == "float")
        {
            auto decoded_gene = scaleToRange(119.0f, 40.0f, max, min, ord_gene);
            hp[name]          = decoded_gene;
        }
        else
        {
            throw std::runtime_error("Only int and float types are implemented");
        }
    }
    return hp;
}

std::string CipherHelper::stringAfterCharacter(const std::string &s, char character)
{
    size_t pos = s.find(character);
    if (pos == std::string::npos)
    {
        return "";
    }
    return s.substr(pos + 1);
}

float CipherHelper::estimateAveragePrice(float order_qty,
                                         float order_price,
                                         float current_qty,
                                         float current_entry_price)
{
    float abs_order_qty   = std::abs(order_qty);
    float abs_current_qty = std::abs(current_qty);
    float total_qty       = abs_order_qty + abs_current_qty;

    if (total_qty == 0.0f)
    {
        throw std::invalid_argument("Total quantity cannot be zero");
    }

    return (abs_order_qty * order_price + abs_current_qty * current_entry_price) / total_qty;
}

float CipherHelper::estimatePNL(float qty,
                                float entry_price,
                                float exit_price,
                                const CipherEnum::TradeType &trade_type,
                                float trading_fee) noexcept(false)
{
    float abs_qty = std::abs(qty);
    if (abs_qty == 0.0f)
    {
        throw std::invalid_argument("Quantity cannot be zero");
    }

    // Optimize: Compute profit directly with multiplier
    float multiplier = (trade_type == CipherEnum::TradeType::SHORT) ? -1.0f : 1.0f;
    if (trade_type != CipherEnum::TradeType::LONG && trade_type != CipherEnum::TradeType::SHORT)
    {
        throw std::invalid_argument("trade_type must be 'long' or 'short'");
    }

    float profit = abs_qty * (exit_price - entry_price) * multiplier;
    float fee    = (trading_fee > 0.0f) ? trading_fee * abs_qty * (entry_price + exit_price) : 0.0f;

    return profit - fee;
}

float CipherHelper::estimatePNLPercentage(float qty,
                                          float entry_price,
                                          float exit_price,
                                          const CipherEnum::TradeType &trade_type) noexcept(false)
{
    float abs_qty = std::abs(qty);
    if (abs_qty == 0.0f)
    {
        throw std::invalid_argument("Quantity cannot be zero");
    }

    float initial_investment = abs_qty * entry_price;
    if (initial_investment == 0.0f)
    {
        throw std::invalid_argument("Initial investment (qty * entry_price) cannot be zero");
    }

    float multiplier = (trade_type == CipherEnum::TradeType::SHORT) ? -1.0f : 1.0f;
    if (trade_type != CipherEnum::TradeType::LONG && trade_type != CipherEnum::TradeType::SHORT)
    {
        throw std::invalid_argument("trade_type must be 'long' or 'short'");
    }

    float profit = abs_qty * (exit_price - entry_price) * multiplier;
    return (profit / initial_investment) * 100.0f;
}

bool CipherHelper::fileExists(const std::string &path)
{
    return std::filesystem::is_regular_file(path);
}

void CipherHelper::clearFile(const std::string &path)
{
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open or create file: " + path);
    }
    file.close(); // Explicit close for clarity, though destructor would handle it
}

void CipherHelper::makeDirectory(const std::string &path)
{
    if (std::filesystem::exists(path))
    {
        if (std::filesystem::is_regular_file(path))
        {
            throw std::runtime_error("Path exists as a file, not a directory: " + path);
        }
        // If it's a directory, do nothing (success)
        return;
    }
    if (!std::filesystem::create_directories(path))
    {
        throw std::runtime_error("Failed to create directory: " + path);
    }
}

std::string CipherHelper::relativeToAbsolute(const std::string &path)
{
    return std::filesystem::absolute(path).string();
}

double CipherHelper::floorWithPrecision(double num, int precision)
{
    if (precision < 0)
    {
        throw std::invalid_argument("Precision must be non-negative");
    }
    double factor = std::pow(10.0, precision);
    return std::floor(num * factor) / factor;
}

std::optional< double > CipherHelper::round(std::optional< double > x, int digits)
{
    if (!x.has_value())
    {
        return std::nullopt;
    }
    return std::round(x.value() * std::pow(10.0, digits)) / std::pow(10.0, digits);
}

double CipherHelper::roundPriceForLiveMode(double price, int precision)
{
    return std::round(price * std::pow(10.0, precision)) / std::pow(10.0, precision);
}

double CipherHelper::roundQtyForLiveMode(double roundable_qty, int precision)
{
    if (precision < 0)
    {
        throw std::invalid_argument("Precision must be non-negative");
    }

    // Round down to prevent insufficient margin
    double rounded = roundDecimalsDown(roundable_qty, precision);

    // If rounded value is 0, make it the minimum possible value
    if (rounded == 0.0)
    {
        if (precision >= 0)
        {
            rounded = 1.0 / std::pow(10.0, precision);
        }
        else
        {
            throw std::invalid_argument("Quantity is too small");
        }
    }

    return rounded;
}

double CipherHelper::roundDecimalsDown(double number, int decimals)
{
    if (decimals == 0)
    {
        return std::floor(number);
    }
    else if (decimals > 0)
    {
        double factor = std::pow(10.0, decimals);
        return std::floor(number * factor) / factor;
    }
    else
    {
        // For negative decimals, round down to nearest power of 10
        double factor = std::pow(10.0, -decimals);
        return std::floor(number / factor) * factor;
    }
}

std::optional< double > CipherHelper::doubleOrNone(const std::string &item)
{
    if (item.empty())
    {
        return std::nullopt;
    }
    try
    {
        size_t pos;
        double value = std::stod(item, &pos);
        if (pos != item.size())
        { // Ensure full string is consumed
            return std::nullopt;
        }
        return value;
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::optional< double > CipherHelper::doubleOrNone(double item)
{
    return item;
}

std::optional< std::string > CipherHelper::strOrNone(const std::string &item, const std::string &encoding)
{
    if (item.empty())
    {
        return std::nullopt;
    }
    return item;
}

// Overload for double
std::optional< std::string > CipherHelper::strOrNone(double item, const std::string &encoding)
{
    return std::to_string(item);
}

// Overload for raw bytes (simulating encoded input)
std::optional< std::string > CipherHelper::strOrNone(const char *item, const std::string &encoding)
{
    if (item == nullptr)
    {
        return std::nullopt;
    }
    try
    {
        // Assume UTF-8 for simplicity; encoding ignored here
        return std::string(item);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::string CipherHelper::convertToEnvName(const std::string &name)
{
    std::string env_name = name;
    std::replace(env_name.begin(), env_name.end(), ' ', '_');
    std::transform(env_name.begin(), env_name.end(), env_name.begin(), [](unsigned char c) { return std::toupper(c); });
    return env_name;
}

std::string CipherHelper::formatCurrency(double num)
{
    std::stringstream ss;
    try
    {
        ss.imbue(std::locale("en_US.UTF-8")); // Fixed US locale for commas
    }
    catch (const std::runtime_error &)
    {
        // Fallback to default locale if "en_US.UTF-8" is unavailable
        ss.imbue(std::locale(""));
    }
    ss << std::fixed << num;
    return ss.str();
}

std::string CipherHelper::generateUniqueId()
{
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    return boost::uuids::to_string(id);
}

std::string CipherHelper::generateShortUniqueId()
{
    std::string full_id = generateUniqueId();
    if (full_id.length() != 36)
    {
        throw std::runtime_error("Generated UUID length is not 36");
    }
    return full_id.substr(0, 22); // 8-4-4-2 format
}

bool CipherHelper::isValidUUID(const std::string &uuid_to_test, int version)
{
    try
    {
        boost::uuids::uuid uuid_obj = boost::uuids::string_generator()(uuid_to_test);
        return uuid_obj.version() == version && uuid_to_test == boost::uuids::to_string(uuid_obj);
    }
    catch (const std::exception &)
    {
        return false;
    }
}

std::string CipherHelper::randomStr(size_t num_characters)
{
    static const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string result;
    result.reserve(num_characters);

    for (size_t i = 0; i < num_characters; ++i)
    {
        result += chars[dis(gen)];
    }

    return result;
}

std::chrono::system_clock::time_point CipherHelper::timestampToTimePoint(int64_t timestamp)
{
    // Convert milliseconds since epoch to chrono duration
    auto duration = std::chrono::milliseconds(timestamp);
    return std::chrono::system_clock::time_point(duration);
}

std::string CipherHelper::timestampToDate(int64_t timestamp)
{
    auto tp = timestampToTimePoint(timestamp);
    auto dp = date::floor< date::days >(tp);
    return date::format("%F", dp); // YYYY-MM-DD
}

std::string CipherHelper::timestampToTime(int64_t timestamp)
{
    auto tp = timestampToTimePoint(timestamp);
    auto dp = date::floor< std::chrono::seconds >(tp);
    return date::format("%F %T", dp); // YYYY-MM-DD HH:MM:SS
}

std::string CipherHelper::timestampToIso8601(int64_t timestamp)
{
    auto tp = timestampToTimePoint(timestamp);
    auto ms = std::chrono::duration_cast< std::chrono::milliseconds >(tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << date::format("%FT%T", tp) << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

int64_t CipherHelper::iso8601ToTimestamp(const std::string &iso8601)
{
    std::istringstream iss(iso8601);
    std::chrono::system_clock::time_point tp;
    std::string milliseconds;

    // Parse ISO 8601 with milliseconds (e.g., "2021-01-05T00:00:00.000Z")
    iss >> date::parse("%FT%T", tp);
    if (iss.fail())
    {
        throw std::invalid_argument("Invalid ISO 8601 format: " + iso8601);
    }

    // Check for milliseconds and 'Z'
    if (iss.peek() == '.')
    {
        iss.ignore(); // Skip '.'
        iss >> std::setw(3) >> milliseconds;
        if (iss.fail() || milliseconds.length() != 3)
        {
            throw std::invalid_argument("Invalid milliseconds in ISO 8601: " + iso8601);
        }
    }
    if (iss.get() != 'Z')
    {
        throw std::invalid_argument("ISO 8601 must end with 'Z': " + iso8601);
    }

    auto duration = tp.time_since_epoch();
    return std::chrono::duration_cast< std::chrono::milliseconds >(duration).count();
}

int64_t CipherHelper::todayToTimestamp()
{
    auto now      = std::chrono::system_clock::now();
    auto dp       = date::floor< date::days >(now);
    auto duration = dp.time_since_epoch();
    return std::chrono::duration_cast< std::chrono::milliseconds >(duration).count();
}

// FIXME: Remove cachedTimestamp.
auto cachedTimestamp = CipherHelper::toTimestamp(std::chrono::system_clock::now());
int64_t CipherHelper::nowToTimestamp(bool force_fresh)
{
    // If not forcing fresh timestamp and not in live trading/importing candles,
    // use cached time from config
    if (!force_fresh && !(isLive() || isImportingCandles()))
    {
        try
        {
            // TODO: Read from store.
            return cachedTimestamp;
        }
        catch (const std::exception &)
        {
            // If config time not available, fall back to current time
            return toTimestamp(std::chrono::system_clock::now());
        }
    }

    // Get fresh UTC timestamp
    return toTimestamp(std::chrono::system_clock::now());
}

std::chrono::system_clock::time_point CipherHelper::nowToDateTime()
{
    return std::chrono::system_clock::now();
}

std::string CipherHelper::readableDuration(int64_t seconds, size_t granularity)
{
    static const std::vector< std::pair< std::string, int64_t > > intervals = {{"weeks", 604800}, // 60 * 60 * 24 * 7
                                                                               {"days", 86400},   // 60 * 60 * 24
                                                                               {"hours", 3600},   // 60 * 60
                                                                               {"minutes", 60},
                                                                               {"seconds", 1}};

    std::vector< std::string > result;
    int64_t remaining = seconds;

    for (const auto &[name, count] : intervals)
    {
        int64_t value = remaining / count;
        if (value > 0)
        {
            remaining -= value * count;
            std::string unit = (value == 1) ? name.substr(0, name.length() - 1) : name;
            result.push_back(std::to_string(value) + " " + unit);
        }
    }

    size_t end = std::min(granularity, result.size());
    std::string output;
    for (size_t i = 0; i < end; ++i)
    {
        if (i > 0)
            output += ", ";
        output += result[i];
    }

    return output;
}

CipherHelper::StrategyLoader &CipherHelper::StrategyLoader::getInstance()
{
    static StrategyLoader loader;
    return loader;
}

std::pair< std::unique_ptr< CipherHelper::Strategy >, void * > CipherHelper::StrategyLoader::getStrategy(
    const std::string &name) const
{
    if (name.empty())
    {
        throw std::invalid_argument("Strategy name cannot be empty");
    }
    return loadStrategy(name);
}

std::pair< std::unique_ptr< CipherHelper::Strategy >, void * > CipherHelper::StrategyLoader::loadStrategy(
    const std::string &name) const
{
    auto module_path = resolveModulePath(name);
    if (!module_path)
    {
        return {nullptr, nullptr};
    }

    auto [strategy, handle] = loadFromDynamicLib(*module_path);
    if (!strategy && !is_testing_)
    { // TODO:
        if (handle)
        {
            dlclose(handle);
        }

        std::filesystem::path source_path = base_path_ / "strategies" / name / "main.cpp";

        if (std::filesystem::exists(source_path))
        {
            std::tie(strategy, handle) = adjustAndReload(name, source_path);
        }

        if (!strategy)
        {
            if (handle)
            {
                dlclose(handle);
            }

            return createFallback(name, *module_path);
        }
    }

    return {std::move(strategy), std::move(handle)};
}

std::optional< std::filesystem::path > CipherHelper::StrategyLoader::resolveModulePath(const std::string &name) const
{
    std::filesystem::path module_dir;
    if (is_testing_)
    {
        module_dir = base_path_.filename() == "ciphertrader-live" ? base_path_ / "tests" / "strategies" / name
                                                                  : base_path_ / "ciphertrader" / "strategies" / name;
    }
    else
    {
        module_dir = base_path_ / "strategies" / name;
    }

    std::filesystem::path module_path = module_dir / (name + ".so");
    return std::filesystem::exists(module_path) ? std::make_optional(module_path) : std::nullopt;
}

std::pair< std::unique_ptr< CipherHelper::Strategy >, void * > CipherHelper::StrategyLoader::loadFromDynamicLib(
    const std::filesystem::path &path) const
{
    auto handle(dlopen(path.string().c_str(), RTLD_LAZY));
    if (!handle)
    {
        const char *error = dlerror();
        // TODO: Log
        std::cerr << "dlopen error: " << (error ? error : "Unknown error") << std::endl;
        return {nullptr, nullptr};
    }

    using CreateFunc = Strategy *(*)();
    auto *create     = reinterpret_cast< CreateFunc >(dlsym(handle, "createStrategy"));
    if (!create)
    {
        if (handle)
        {
            dlclose(handle);
        }

        const char *error = dlerror();
        // TODO: Log
        std::cerr << "dlsym error: " << (error ? error : "Unable to find createStrategy symbol") << std::endl;
        return {nullptr, nullptr};
    }

    return {std::unique_ptr< Strategy >(create()), std::move(handle)};
}

std::pair< std::unique_ptr< CipherHelper::Strategy >, void * > CipherHelper::StrategyLoader::adjustAndReload(
    const std::string &name, const std::filesystem::path &source_path) const
{
    std::ifstream in_file(source_path);
    if (!in_file)
    {
        return {nullptr, nullptr};
    }

    std::string content((std::istreambuf_iterator< char >(in_file)), std::istreambuf_iterator< char >());
    in_file.close();

    // Match class derived from CipherHelper::Strategy
    std::regex class_pattern(R"(class\s+(\w+)\s*:\s*public\s*CipherHelper::Strategy\s*)");
    // std::regex classPattern(
    //     R"(^(class|struct)\s+(\w+(?:::\w+)*)\s*:\s*public\s*CipherHelper::Strategy(?:\s*,.*)?)");
    std::smatch match;

    if (std::regex_search(content, match, class_pattern) && match.size() > 1)
    {
        std::string old_class_name = match[1];
        if (old_class_name != name)
        {
            // std::string newContent = std::regex_replace(
            //     content, std::regex("class\\s+" + oldClassName), "class " + name);
            std::string new_content = std::regex_replace(
                content,
                std::regex("(?:\\w+::)?class\\s+" + name + "\\s*:\\s*public\\s*CipherHelper::Strategy"),
                "class " + name + " : public CipherHelper::Strategy");

            std::ofstream out_file(source_path);
            if (!out_file)
            {
                return {nullptr, nullptr};
            }
            out_file << new_content;
            out_file.close();

            // Compile with custom library and headers
            std::filesystem::path module_path = source_path.parent_path() / (name + ".so");
            std::string include_flag          = "-I" + include_path_.string();
            std::string lib_flag              = "-L" + library_path_.string();
            // g++ -shared -pthread -ldl -fPIC -std=c++17 -Iinclude
            // -I/opt/homebrew/include   -L/opt/homebrew/lib -o libmy_trading_lib.so
            // src/*
            std::string cmd = "g++ -shared -pthread -ldl -fPIC -std=c++17 "
                              " -I/opt/homebrew/include -I/opt/homebrew/opt/libpq/include "
                              " -I/opt/homebrew/Cellar/yaml-cpp/0.8.0/include " + // TODO:
                              include_flag +
                              " -L/opt/homebrew/lib -lssl -lcrypto -L/opt/homebrew/opt/libpq/lib "
                              " -L/opt/homebrew/Cellar/yaml-cpp/0.8.0/lib " + // TODO:
                              lib_flag +
                              " -o " + module_path.string() + " " + source_path.string();
            if (system(cmd.c_str()) == 0)
            {
                // Verify the .so exists after compilation
                if (std::filesystem::exists(module_path))
                {
                    return loadFromDynamicLib(module_path);
                }
            }
            // Log compilation failure in a real system; here we just return nullptr
            return {nullptr, nullptr};
        }
    }

    return {nullptr, nullptr};
}

std::pair< std::unique_ptr< CipherHelper::Strategy >, void * > CipherHelper::StrategyLoader::createFallback(
    const std::string &, const std::filesystem::path &module_path) const
{
    auto handle(dlopen(module_path.string().c_str(), RTLD_LAZY));
    if (!handle)
    {
        return {nullptr, nullptr};
    }

    // Try common factory names
    static const std::array< const char *, 2 > factory_names = {"createStrategy", "createDefaultStrategy"};

    for (const auto *factory_name : factory_names)
    {
        using CreateFunc = Strategy *(*)();
        auto *create     = reinterpret_cast< CreateFunc >(dlsym(handle, factory_name));
        if (create)
        {
            auto base = std::unique_ptr< Strategy >(create());

            // Wrapper class for fallback
            class NamedStrategy final : public Strategy
            {
               public:
                explicit NamedStrategy(std::unique_ptr< Strategy > &&base) : base_(std::move(base)) {}
                void execute() override
                {
                    if (base_)
                        base_->execute();
                }

               private:
                std::unique_ptr< Strategy > base_;
            };
            return {std::make_unique< NamedStrategy >(std::move(base)), std::move(handle)};
        }
    }

    if (handle)
    {
        dlclose(handle);
    }

    return {nullptr, nullptr};
}

std::string CipherHelper::computeSecureHash(std::string_view msg)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, msg.data(), msg.length());
    SHA256_Final(digest, &ctx);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast< int >(digest[i]);
    }
    return ss.str();
}

template < typename T >
std::vector< T > CipherHelper::insertList(size_t index, const T &item, const std::vector< T > &arr)
{
    std::vector< T > result;
    result.reserve(arr.size() + 1); // Pre-allocate for efficiency

    if (index == static_cast< size_t >(-1))
    {
        result = arr;
        result.push_back(item);
        return result;
    }

    // Throw exception if index is out of range (except for the special case of
    // appending)
    if (index > arr.size())
    {
        throw std::out_of_range("Index out of range in insertList");
    }

    result.insert(result.end(), arr.begin(), arr.begin() + index);
    result.push_back(item);
    result.insert(result.end(), arr.begin() + index, arr.end());
    return result;
}

template < typename T, typename = void >
struct is_map : std::false_type
{
};

template < typename T >
struct is_map< T,
               std::void_t< typename T::key_type,
                            typename T::mapped_type,
                            decltype(std::declval< T >().find(std::declval< const typename T::key_type & >())),
                            decltype(std::declval< T >().end()) > > : std::true_type
{
};

template < typename T >
inline constexpr bool is_map_v = is_map< T >::value;

template < typename MapType >
MapType CipherHelper::mergeMaps(const MapType &d1, const MapType &d2)
{
    using ValueType = typename MapType::mapped_type;

    MapType result = d1;
    for (const auto &[key, value] : d2)
    {
        // If key exists in both maps
        if (result.find(key) != result.end())
        {
            // Check if the value type is a map-like container
            if constexpr (is_map_v< ValueType >)
            {
                // Only recursively merge if both are actually maps
                result[key] = mergeMaps(result[key], value);
            }
            else
            {
                // Overwrite with d2's value for non-map types
                result[key] = value;
            }
        }
        else
        {
            // If key only exists in d2, add it
            result[key] = value;
        }
    }
    return result;
}

using MapType1 = std::map< std::string, int >;
using MapType2 = std::map< std::string, std::string >;
using MapType3 = std::map< std::string, std::map< std::string, int > >;
using MapType4 = std::map< std::string, std::variant< int, std::string > >;
using MapType5 = std::map< std::string, std::map< std::string, std::map< std::string, int > > >;
template MapType1 CipherHelper::mergeMaps(const MapType1 &, const MapType1 &);
template MapType2 CipherHelper::mergeMaps(const MapType2 &, const MapType2 &);
template MapType3 CipherHelper::mergeMaps(const MapType3 &, const MapType3 &);
template MapType4 CipherHelper::mergeMaps(const MapType4 &, const MapType4 &);
template MapType5 CipherHelper::mergeMaps(const MapType5 &, const MapType5 &);

// Explicit template instantiations
template std::vector< int > CipherHelper::insertList(size_t, const int &, const std::vector< int > &);
template std::vector< std::string > CipherHelper::insertList(size_t,
                                                             const std::string &,
                                                             const std::vector< std::string > &);

template std::vector< std::pair< int, std::string > > CipherHelper::insertList(
    size_t, const std::pair< int, std::string > &, const std::vector< std::pair< int, std::string > > &);

// Specialization for nlohmann::json
nlohmann::json mergeMaps(const nlohmann::json &j1, const nlohmann::json &j2)
{
    // If either is not an object, return the second json
    if (!j1.is_object() || !j2.is_object())
    {
        return j2;
    }

    nlohmann::json result = j1;

    for (const auto &[key, value] : j2.items())
    {
        // If key doesn't exist in result, simply add it
        if (result.find(key) == result.end())
        {
            result[key] = value;
        }
        else
        {
            // If both are JSON objects, recursively merge
            if (result[key].is_object() && value.is_object())
            {
                result[key] = mergeMaps(result[key], value);
            }
            else
            {
                // For non-object types, overwrite with j2's value
                result[key] = value;
            }
        }
    }

    return result;
}

bool CipherHelper::isBacktesting()
{
    return CipherConfig::Config::getInstance().getValue< std::string >("app_trading_mode") == "backtest";
}

bool CipherHelper::isDebuggable(const std::string &debug_item)
{
    try
    {
        return isDebugging() && CipherConfig::Config::getInstance().getValue< bool >("env_logging_" + debug_item);
    }
    catch (const std::exception &)
    {
        return false; // Default to true if key not found
    }
}

bool CipherHelper::isDebugging()
{
    return CipherConfig::Config::getInstance().getValue< bool >("app_debug_mode");
}

bool CipherHelper::isImportingCandles()
{
    return CipherConfig::Config::getInstance().getValue< std::string >("app_trading_mode") == "candles";
}

bool CipherHelper::isLive()
{
    return isLiveTrading() || isPaperTrading();
}

bool CipherHelper::isLiveTrading()
{
    return CipherConfig::Config::getInstance().getValue< std::string >("app_trading_mode") == "livetrade";
}

bool CipherHelper::isPaperTrading()
{
    return CipherConfig::Config::getInstance().getValue< std::string >("app_trading_mode") == "papertrade";
}

bool CipherHelper::isOptimizing()
{
    return CipherConfig::Config::getInstance().getValue< std::string >("app_trading_mode") == "optimize";
}

bool CipherHelper::shouldExecuteSilently()
{
    // return isOptimizing() || isUnitTesting(); // TODO:
    return isOptimizing();
}

std::string CipherHelper::generateCompositeKey(const std::string &exchange,
                                               const std::string &symbol,
                                               const std::optional< CipherEnum::Timeframe > &timeframe)
{
    if (!timeframe)
    {
        return exchange + "-" + symbol;
    }
    return exchange + "-" + symbol + "-" + CipherEnum::toString(*timeframe);
}

CipherEnum::Timeframe CipherHelper::maxTimeframe(const std::vector< CipherEnum::Timeframe > &timeframes)
{
    // Define timeframe priority (higher index = higher priority)
    static const std::vector< CipherEnum::Timeframe > timeframe_priority = {CipherEnum::Timeframe::MINUTE_1,
                                                                            CipherEnum::Timeframe::MINUTE_3,
                                                                            CipherEnum::Timeframe::MINUTE_5,
                                                                            CipherEnum::Timeframe::MINUTE_15,
                                                                            CipherEnum::Timeframe::MINUTE_30,
                                                                            CipherEnum::Timeframe::MINUTE_45,
                                                                            CipherEnum::Timeframe::HOUR_1,
                                                                            CipherEnum::Timeframe::HOUR_2,
                                                                            CipherEnum::Timeframe::HOUR_3,
                                                                            CipherEnum::Timeframe::HOUR_4,
                                                                            CipherEnum::Timeframe::HOUR_6,
                                                                            CipherEnum::Timeframe::HOUR_8,
                                                                            CipherEnum::Timeframe::HOUR_12,
                                                                            CipherEnum::Timeframe::DAY_1,
                                                                            CipherEnum::Timeframe::DAY_3,
                                                                            CipherEnum::Timeframe::WEEK_1,
                                                                            CipherEnum::Timeframe::MONTH_1};

    // Find the highest priority timeframe that exists in the input list
    for (auto it = timeframe_priority.rbegin(); it != timeframe_priority.rend(); ++it)
    {
        if (std::find(timeframes.begin(), timeframes.end(), *it) != timeframes.end())
        {
            return *it;
        }
    }

    // If no timeframes found, return the lowest priority (MINUTE_1)
    return CipherEnum::Timeframe::MINUTE_1;
}

int64_t CipherHelper::getTimeframeToOneMinutes(const CipherEnum::Timeframe &timeframe)
{
    // Use static map for better performance and memory usage
    static const std::unordered_map< CipherEnum::Timeframe, int64_t > timeframe_map = {
        {CipherEnum::Timeframe::MINUTE_1, 1},
        {CipherEnum::Timeframe::MINUTE_3, 3},
        {CipherEnum::Timeframe::MINUTE_5, 5},
        {CipherEnum::Timeframe::MINUTE_15, 15},
        {CipherEnum::Timeframe::MINUTE_30, 30},
        {CipherEnum::Timeframe::MINUTE_45, 45},
        {CipherEnum::Timeframe::HOUR_1, 60},
        {CipherEnum::Timeframe::HOUR_2, 60 * 2},
        {CipherEnum::Timeframe::HOUR_3, 60 * 3},
        {CipherEnum::Timeframe::HOUR_4, 60 * 4},
        {CipherEnum::Timeframe::HOUR_6, 60 * 6},
        {CipherEnum::Timeframe::HOUR_8, 60 * 8},
        {CipherEnum::Timeframe::HOUR_12, 60 * 12},
        {CipherEnum::Timeframe::DAY_1, 60 * 24},
        {CipherEnum::Timeframe::DAY_3, 60 * 24 * 3},
        {CipherEnum::Timeframe::WEEK_1, 60 * 24 * 7},
        {CipherEnum::Timeframe::MONTH_1, 60 * 24 * 30}};

    // Use find instead of operator[] to avoid creating new entries
    auto it = timeframe_map.find(timeframe);
    if (it != timeframe_map.end())
    {
        return it->second;
    }

    // If timeframe not found, throw exception with supported timeframes
    std::stringstream ss;
    ss << "Timeframe \"" << toString(timeframe) << "\" is invalid. Supported timeframes are: ";

    bool first = true;
    for (const auto &tf : timeframe_map)
    {
        if (!first)
        {
            ss << ", ";
        }
        ss << toString(tf.first);
        first = false;
    }

    throw CipherException::InvalidTimeframe();
}

template < typename T >
T CipherHelper::scaleToRange(T old_max, T old_min, T new_max, T new_min, T old_value)
{
    static_assert(std::is_arithmetic_v< T >, "Type must be numeric");

    if (old_value > old_max || old_value < old_min)
    {
        throw std::invalid_argument("Value out of range");
    }
    T old_range = old_max - old_min;
    if (old_range == 0)
    {
        throw std::invalid_argument("Old range cannot be zero");
    }
    T new_range = new_max - new_min;
    return (((old_value - old_min) * new_range) / old_range) + new_min;
}

template int CipherHelper::scaleToRange(int old_max, int old_min, int new_max, int new_min, int old_value);

template float CipherHelper::scaleToRange(float old_max, float old_min, float new_max, float new_min, float old_value);

template double CipherHelper::scaleToRange(
    double old_max, double old_min, double new_max, double new_min, double old_value);

template < typename T >
T CipherHelper::normalize(T x, T x_min, T x_max)
{
    static_assert(std::is_arithmetic_v< T >, "Type must be arithmetic");
    if (x_max == x_min)
    {
        return T(0); // Avoid division by zero
    }
    return (x - x_min) / (x_max - x_min);
}

// Explicit template instantiations
template int CipherHelper::normalize(int x, int x_min, int x_max);
template float CipherHelper::normalize(float x, float x_min, float x_max);
template double CipherHelper::normalize(double x, double x_min, double x_max);

CipherEnum::Side CipherHelper::oppositeSide(const CipherEnum::Side &side)
{
    static const std::unordered_map< CipherEnum::Side, CipherEnum::Side > opposites = {
        {CipherEnum::Side::BUY, CipherEnum::Side::SELL}, {CipherEnum::Side::SELL, CipherEnum::Side::BUY}};

    auto it = opposites.find(side);
    if (it == opposites.end())
    {
        throw std::invalid_argument("Invalid side: " + CipherEnum::toString(side));
    }
    return it->second;
}

CipherEnum::TradeType CipherHelper::oppositeTradeType(const CipherEnum::TradeType &trade_type)
{
    static const std::unordered_map< CipherEnum::TradeType, CipherEnum::TradeType > opposites = {
        {CipherEnum::TradeType::LONG, CipherEnum::TradeType::SHORT},
        {CipherEnum::TradeType::SHORT, CipherEnum::TradeType::LONG}};

    auto it = opposites.find(trade_type);
    if (it == opposites.end())
    {
        throw std::invalid_argument("Invalid tradeType: " + CipherEnum::toString(trade_type));
    }
    return it->second;
}

CipherEnum::TradeType CipherHelper::sideToType(const CipherEnum::Side &side)
{
    if (side == CipherEnum::Side::BUY)
    {
        return CipherEnum::TradeType::LONG;
    }
    else if (side == CipherEnum::Side::SELL)
    {
        return CipherEnum::TradeType::SHORT;
    }
    else
    {
        throw std::invalid_argument("Invalid side: " + CipherEnum::toString(side));
    }
}

CipherEnum::Side CipherHelper::typeToSide(const CipherEnum::TradeType &trade_type)
{
    if (trade_type == CipherEnum::TradeType::LONG)
    {
        return CipherEnum::Side::BUY;
    }
    else if (trade_type == CipherEnum::TradeType::SHORT)
    {
        return CipherEnum::Side::SELL;
    }
    else
    {
        throw std::invalid_argument("Invalid tradeType: " + CipherEnum::toString(trade_type));
    }
}

CipherEnum::Side CipherHelper::closingSide(const CipherEnum::Position &position)
{
    if (position == CipherEnum::Position::LONG)
    {
        return CipherEnum::Side::SELL;
    }
    else if (position == CipherEnum::Position::SHORT)
    {
        return CipherEnum::Side::BUY;
    }
    else
    {
        throw std::invalid_argument("Invalid position: " + CipherEnum::toString(position));
    }
}

int64_t CipherHelper::current1mCandleTimestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms  = duration_cast< milliseconds >(now.time_since_epoch()).count();
    // Floor to nearest minute
    return (ms / 60000) * 60000;
}

// TODO: OPTIMIZE?
template < typename T >
blaze::DynamicMatrix< T > CipherHelper::forwardFill(const blaze::DynamicMatrix< T > &matrix, size_t axis)
{
    blaze::DynamicMatrix< T > result(matrix);

    if (axis == 0)
    {
        // Fill along rows
        for (size_t j = 0; j < result.columns(); ++j)
        {
            T last_valid_value   = T();
            bool has_valid_value = false;

            for (size_t i = 0; i < result.rows(); ++i)
            {
                if (!std::isnan(result(i, j)))
                {
                    last_valid_value = result(i, j);
                    has_valid_value  = true;
                }
                else if (has_valid_value)
                {
                    result(i, j) = last_valid_value;
                }
            }
        }
    }
    else
    {
        // Fill along columns
        for (size_t i = 0; i < result.rows(); ++i)
        {
            T last_valid_value   = T();
            bool has_valid_value = false;

            for (size_t j = 0; j < result.columns(); ++j)
            {
                if (!std::isnan(result(j, i)))
                {
                    last_valid_value = result(j, i);
                    has_valid_value  = true;
                }
                else if (has_valid_value)
                {
                    result(j, i) = last_valid_value;
                }
            }
        }
    }

    return std::move(result);
}

template blaze::DynamicMatrix< double > CipherHelper::forwardFill(const blaze::DynamicMatrix< double > &, size_t);

// TODO: OPTIMIZE?
template < typename T >
blaze::DynamicMatrix< T > CipherHelper::shift(const blaze::DynamicMatrix< T > &matrix, int shift, T fill_value)
{
    if (shift == 0)
        return std::move(blaze::DynamicMatrix< T >(matrix));

    blaze::DynamicMatrix< T > result(matrix.rows(), matrix.columns(), fill_value);

    if (shift > 0)
    {
        // Forward shift
        auto src_view  = submatrix(matrix, 0, 0, matrix.rows() - shift, matrix.columns());
        auto dest_view = submatrix(result, shift, 0, matrix.rows() - shift, matrix.columns());
        dest_view      = src_view;
    }
    else
    {
        // Backward shift
        shift          = -shift;
        auto src_view  = submatrix(matrix, shift, 0, matrix.rows() - shift, matrix.columns());
        auto dest_view = submatrix(result, 0, 0, matrix.rows() - shift, matrix.columns());
        dest_view      = src_view;
    }

    return std::move(result);
}

template blaze::DynamicMatrix< double > CipherHelper::shift(const blaze::DynamicMatrix< double > &, int, double);

// TODO: OPTIMIZE?
template < typename T >
blaze::DynamicVector< T > CipherHelper::shift(const blaze::DynamicVector< T > &vector, int shift, T fill_value)
{
    if (shift == 0)
        return blaze::DynamicVector< T >(vector);

    blaze::DynamicVector< T > result(vector.size(), fill_value);

    if (shift > 0)
    {
        // Forward shift (move elements to the right)
        if (vector.size() > static_cast< size_t >(shift))
        {
            auto src_view  = blaze::subvector(vector, 0, vector.size() - shift);
            auto dest_view = blaze::subvector(result, shift, vector.size() - shift);
            dest_view      = src_view;
        }
    }
    else
    {
        // Backward shift (move elements to the left)
        shift = -shift;
        if (vector.size() > static_cast< size_t >(shift))
        {
            auto src_view  = blaze::subvector(vector, shift, vector.size() - shift);
            auto dest_view = blaze::subvector(result, 0, vector.size() - shift);
            dest_view      = src_view;
        }
    }

    return result;
}

template blaze::DynamicVector< double > CipherHelper::shift(const blaze::DynamicVector< double > &, int, double);

template < typename T >
blaze::DynamicMatrix< T > CipherHelper::sameLength(const blaze::DynamicMatrix< T > &bigger,
                                                   const blaze::DynamicMatrix< T > &shorter)
{
    size_t diff = bigger.rows() - shorter.rows();
    blaze::DynamicMatrix< T > result(bigger.rows(), bigger.columns());

    // Fill with NaN
    for (size_t i = 0; i < diff; ++i)
    {
        for (size_t j = 0; j < result.columns(); ++j)
        {
            result(i, j) = std::numeric_limits< T >::quiet_NaN();
        }
    }

    // Copy shorter matrix
    for (size_t i = 0; i < shorter.rows(); ++i)
    {
        for (size_t j = 0; j < shorter.columns(); ++j)
        {
            result(i + diff, j) = shorter(i, j);
        }
    }

    return std::move(result);
}

template blaze::DynamicMatrix< double > CipherHelper::sameLength(const blaze::DynamicMatrix< double > &bigger,
                                                                 const blaze::DynamicMatrix< double > &shorter);

template < typename MT >
bool CipherHelper::matricesEqualWithTolerance(const MT &a, const MT &b, double tolerance)
{
    // Check dimensions
    if (a.rows() != b.rows() || a.columns() != b.columns())
    {
        return false;
    }

    for (size_t i = 0; i < a.rows(); ++i)
    {
        for (size_t j = 0; j < a.columns(); ++j)
        {
            // Special NaN check
            if (std::isnan(a(i, j)) && std::isnan(b(i, j)))
            {
                continue;
            }

            // Use absolute difference for comparison
            if (std::abs(a(i, j) - b(i, j)) > tolerance)
            {
                return false;
            }
        }
    }
    return true;
}

template bool CipherHelper::matricesEqualWithTolerance(const blaze::DynamicMatrix< double > &,
                                                       const blaze::DynamicMatrix< double > &,
                                                       double);

template < typename MT >
std::tuple< bool, size_t > CipherHelper::findOrderbookInsertionIndex(const MT &arr, double target, bool ascending)
{
    size_t lower = 0;
    size_t upper = arr.rows();

    while (lower < upper)
    {
        size_t mid = lower + (upper - lower) / 2;
        double val = arr(mid, 0);

        if (ascending)
        {
            if (std::abs(target - val) < std::numeric_limits< double >::epsilon())
            {
                return {true, mid};
            }
            else if (target > val)
            {
                if (lower == mid)
                {
                    return {false, lower + 1};
                }
                lower = mid;
            }
            else
            {
                if (lower == mid)
                {
                    return {false, lower};
                }
                upper = mid;
            }
        }
        else
        {
            if (std::abs(target - val) < std::numeric_limits< double >::epsilon())
            {
                return {true, mid};
            }
            else if (target < val)
            {
                if (lower == mid)
                {
                    return {false, lower + 1};
                }
                lower = mid;
            }
            else
            {
                if (lower == mid)
                {
                    return {false, lower};
                }
                upper = mid;
            }
        }
    }

    return {false, lower};
}

template std::tuple< bool, size_t > CipherHelper::findOrderbookInsertionIndex(const blaze::DynamicMatrix< double > &,
                                                                              double,
                                                                              bool);

const std::function< double(const std::string &) > CipherHelper::strToDouble =
    std::bind(static_cast< double (*)(const std::string &, size_t *) >(std::stod), std::placeholders::_1, nullptr);

const std::function< float(const std::string &) > CipherHelper::strToFloat =
    std::bind(static_cast< float (*)(const std::string &, std::size_t *) >(std::stof), std::placeholders::_1, nullptr);

template < typename InputType, typename OutputType, typename Converter >
std::vector< std::vector< OutputType > > CipherHelper::cleanOrderbookList(
    const std::vector< std::vector< InputType > > &arr, Converter convert)
{
    std::vector< std::vector< OutputType > > result;
    result.reserve(arr.size());

    for (const auto &inner : arr)
    {
        if (inner.size() < 2)
        {
            throw std::invalid_argument("Each inner vector must have at least 2 elements");
        }

        try
        {
            result.push_back({convert(inner[0]), convert(inner[1])});
        }
        catch (const std::exception &e)
        {
            throw std::invalid_argument("Conversion failed: " + std::string(e.what()));
        }
    }

    return result;
}

// std::string to double
template std::vector< std::vector< double > > CipherHelper::cleanOrderbookList(
    const std::vector< std::vector< std::string > > &arr, decltype(strToDouble));

// std::string to float
template std::vector< std::vector< float > > CipherHelper::cleanOrderbookList(
    const std::vector< std::vector< std::string > > &arr, decltype(strToFloat));

// int to double with static_cast
template std::vector< std::vector< double > > CipherHelper::cleanOrderbookList(
    const std::vector< std::vector< int > > &arr, std::function< double(const int &) > convert);

// int to float with static_cast
template std::vector< std::vector< float > > CipherHelper::cleanOrderbookList(
    const std::vector< std::vector< int > > &arr, std::function< float(const int &) > convert);

double CipherHelper::orderbookTrimPrice(double price, bool ascending, double unit)
{
    if (unit <= 0)
    {
        throw std::invalid_argument("Unit must be positive");
    }

    double trimmed;
    if (ascending)
    {
        trimmed = std::ceil(price / unit) * unit;
        if (std::log10(unit) < 0)
        {
            trimmed = std::round(trimmed * std::pow(10.0, std::abs(std::log10(unit)))) /
                      std::pow(10.0, std::abs(std::log10(unit)));
        }
        return (trimmed == price + unit) ? price : trimmed;
    }
    else
    {
        trimmed = std::ceil(price / unit) * unit - unit;
        if (std::log10(unit) < 0)
        {
            trimmed = std::round(trimmed * std::pow(10.0, std::abs(std::log10(unit)))) /
                      std::pow(10.0, std::abs(std::log10(unit)));
        }
        return (trimmed == price - unit) ? price : trimmed;
    }
}

// TODO: std::move
blaze::DynamicVector< double > CipherHelper::getCandleSource(const blaze::DynamicMatrix< double > &candles,
                                                             CipherCandle::Source source_type)
{
    // Check matrix dimensions (expect at least 6 columns: timestamp, open, close,
    // high, low, volume)
    if (candles.columns() < 6)
    {
        throw std::invalid_argument("Candles matrix must have at least 6 columns");
    }
    if (candles.rows() == 0)
    {
        throw std::invalid_argument("Candles matrix must have at least one row");
    }

    switch (source_type)
    {
        case CipherCandle::Source::Close:
            return blaze::column(candles, 2); // Close prices
        case CipherCandle::Source::High:
            return blaze::column(candles, 3); // High prices
        case CipherCandle::Source::Low:
            return blaze::column(candles, 4); // Low prices
        case CipherCandle::Source::Open:
            return blaze::column(candles, 1); // Open prices
        case CipherCandle::Source::Volume:
            return blaze::column(candles, 5); // Volume
        case CipherCandle::Source::HL2:
            return (blaze::column(candles, 3) + blaze::column(candles, 4)) / 2.0; // (High + Low) / 2
        case CipherCandle::Source::HLC3:
            return (blaze::column(candles, 3) + blaze::column(candles, 4) + blaze::column(candles, 2)) /
                   3.0; // (High + Low + Close) / 3
        case CipherCandle::Source::OHLC4:
            return (blaze::column(candles, 1) + blaze::column(candles, 3) + blaze::column(candles, 4) +
                    blaze::column(candles, 2)) /
                   4.0; // (Open + High + Low + Close) / 4
        default:
            throw std::invalid_argument("Unknown candle source type");
    }
}

template < typename T >
blaze::DynamicMatrix< T > CipherHelper::sliceCandles(const blaze::DynamicMatrix< T > &candles, bool sequential)
{
    auto warmup_candles_num = CipherConfig::Config::getInstance().getValue< int >("env_data_warmup_candles_num", 240);

    if (!sequential && candles.rows() > warmup_candles_num)
    {
        blaze::DynamicMatrix< T > result(warmup_candles_num, candles.columns());
        for (size_t i = 0; i < warmup_candles_num; ++i)
        { // FIXME: Warning.
            for (size_t j = 0; j < candles.columns(); ++j)
            {
                result(i, j) = candles(candles.rows() - warmup_candles_num + i, j);
            }
        }

        return std::move(result);

        // TODO:
        // // Calculate the start row for slicing
        // size_t start_row = candles.rows() - warmup_candles_num;
        // // Create a submatrix view for better performance
        // return blaze::submatrix(candles, start_row, 0, warmup_candles_num, candles.columns());
    }

    return candles;

    // TODO:
    // Return a view of the entire matrix instead of a copy
    // return blaze::submatrix(candles, 0, 0, candles.rows(), candles.columns());
}

template blaze::DynamicMatrix< double > CipherHelper::sliceCandles(const blaze::DynamicMatrix< double > &candles,
                                                                   bool sequential);

template < typename T >
int64_t CipherHelper::getNextCandleTimestamp(const blaze::DynamicVector< T > &candle,
                                             const CipherEnum::Timeframe &timeframe)
{
    if (candle.size() < 1)
    {
        throw std::invalid_argument("Invalid candle data");
    }
    return static_cast< int64_t >(candle[0] + getTimeframeToOneMinutes(timeframe) * 60'000);
}

template int64_t CipherHelper::getNextCandleTimestamp(const blaze::DynamicVector< int64_t > &candle,
                                                      const CipherEnum::Timeframe &timeframe);

template int64_t CipherHelper::getNextCandleTimestamp(const blaze::DynamicVector< double > &candle,
                                                      const CipherEnum::Timeframe &timeframe);

int64_t CipherHelper::getCandleStartTimestampBasedOnTimeframe(const CipherEnum::Timeframe &timeframe,
                                                              int64_t num_candles_to_fetch)
{
    auto one_min_count = getTimeframeToOneMinutes(timeframe);
    auto finish_date   = nowToTimestamp(true);
    return finish_date - (num_candles_to_fetch * one_min_count * 60'000);
}

double CipherHelper::prepareQty(double qty, const std::string &action)
{
    std::string lower_side = action;
    std::transform(lower_side.begin(), lower_side.end(), lower_side.begin(), ::tolower);

    if (lower_side == "sell" || lower_side == "short")
    {
        return -std::abs(qty);
    }
    else if (lower_side == "buy" || lower_side == "long")
    {
        return std::abs(qty);
    }
    else if (lower_side == "close")
    {
        return 0.0;
    }
    else
    {
        throw std::invalid_argument("Invalid side: " + action);
    }
}

// Check if price is near another price within a percentage threshold
bool CipherHelper::isPriceNear(double order_price, double price_to_compare, double percentage_threshold)
{
    // Handle cases where price_to_compare is zero
    if (price_to_compare == 0.0)
    {
        // If both prices are zero, they are considered near
        if (order_price == 0.0)
        {
            return true;
        }
        // If only price_to_compare is zero but order_price isn't, they are not near
        return false;
    }

    // Use absolute difference to handle potential division by zero
    return std::abs(1.0 - (order_price / price_to_compare)) <= percentage_threshold;
}

std::string CipherHelper::getSessionId()
{
    // TODO: Use Store, Write tests.
    return "";
}

void CipherHelper::terminateApp()
{
    // TODO: Close database connection if needed
    // Note: Database connection handling should be implemented separately
    std::exit(1);
}

// Base case for variadic template recursion
void CipherHelper::dump()
{
    // Do nothing (end of recursion)
}

// Template function to print a single item
template < typename T >
void CipherHelper::dump(const T &item)
{
    std::cout << item << std::endl;
}

// Specialization for std::vector to pretty-print
template < typename T >
void CipherHelper::dump(const std::vector< T > &vec)
{
    std::cout << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        std::cout << vec[i];
        if (i < vec.size() - 1)
            std::cout << ", ";
    }
    std::cout << "]" << std::endl;
}

// Variadic template to handle multiple arguments
template < typename T, typename... Args >
void CipherHelper::dump(const T &first, const Args &...rest)
{
    dump(first);
    dump(rest...);
}

// Main dump function
template < typename... Args >
void CipherHelper::dump(const Args &...items)
{
    std::cout << color("\n========= DEBUGGING VALUE ==========", "yellow") << std::endl;

    // If only one argument, print it directly; otherwise, print all
    if constexpr (sizeof...(items) == 1)
    {
        dump(items...);
    }
    else
    {
        dump(items...);
    }

    std::cout << color("====================================\n", "yellow");
}

template void CipherHelper::dump();
template void CipherHelper::dump(const int &);
template void CipherHelper::dump(const std::vector< int > &);
template void CipherHelper::dump(double const &, char const (&)[6]);

void CipherHelper::dumpAndTerminate(const std::string &item)
{
    std::vector< std::string > items{item};
    dump(items);
    terminateApp();
}

bool CipherHelper::isCiphertraderProject()
{
    namespace fs = std::filesystem;
    return fs::exists("strategies") && fs::exists("storage");
}

#if defined(__APPLE__)
#define MY_OS "mac"
#elif defined(__linux__)
#define MY_OS "linux"
#elif defined(_WIN32) || defined(_WIN64)
#define MY_OS "windows"
#else
#define MY_OS "unknown"
#endif

std::string CipherHelper::getOs()
{
    std::string os = MY_OS;
    if (os == "unknown")
    {
        throw std::runtime_error("Unsupported OS: " + os);
    }
    return os;
}

bool CipherHelper::isDocker()
{
    return std::filesystem::exists("/.dockerenv");
}

pid_t CipherHelper::getPid()
{
    return getpid();
}

size_t CipherHelper::getCpuCoresCount()
{
    return std::thread::hardware_concurrency();
}

template < typename T >
std::string CipherHelper::getClassName()
{
    return typeid(T).name();
}

template std::string CipherHelper::getClassName< int >();

std::string CipherHelper::gzipCompress(const std::string &data)
{
    uLong source_len = data.length();
    uLong dest_len   = compressBound(source_len);
    // std::vector< unsigned char > dest(dest_len);
    std::vector< Bytef > dest(dest_len);

    // if (compress(dest.data(), &dest_len, reinterpret_cast< const Bytef * >(data.data()), source_len) != Z_OK)
    // {
    //     throw std::runtime_error("Compression failed");
    // }
    if (compress2(
            dest.data(), &dest_len, reinterpret_cast< const Bytef * >(data.c_str()), source_len, Z_BEST_COMPRESSION) !=
        Z_OK)
    {
        throw std::runtime_error("Compression failed");
    }

    // dest.resize(dest_len);
    // return dest;

    return std::string(reinterpret_cast< char * >(dest.data()), dest_len);
}


/*
 * Helper function to handle compression for HTTP responses.
 * Returns a JSON object with compression info and content.
 * @param content: string content to potentially compress
 * @return: JSON object with is_compressed flag and content
 */
nlohmann::json CipherHelper::compressedResponse(const std::string &content)
{
    // Compress the content using the provided function
    std::string compressed = gzipCompress(content);

    // Encode as base64 string for safe transmission
    std::size_t encoded_len = boost::beast::detail::base64::encoded_size(compressed.size());

    // Allocate a string with enough space (no null terminator needed)
    std::string result(encoded_len, '\0');

    // Encode directly into the string's buffer
    std::size_t written = boost::beast::detail::base64::encode(result.data(), compressed.data(), compressed.size());

    // Ensure the string length matches what was written (trim excess if any)
    result.resize(written);

    // Create and return the JSON response
    nlohmann::json response;
    response["is_compressed"] = true;
    response["data"]          = written;

    return response;
}
