#pragma once

#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <typeinfo>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


#include <zlib.h>

#include <date/date.h>

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <blaze/Math.h>

#include <nlohmann/json.hpp>

#include <yaml-cpp/yaml.h>

#include <sw/redis++/redis++.h>

#include <boost/algorithm/string.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// Basic sqlpp11 headers
#include <sqlpp11/postgresql/connection.h>
#include <sqlpp11/postgresql/connection_config.h>
#include <sqlpp11/postgresql/postgresql.h> // If using PostgreSQL

// For using parameters
#include <sqlpp11/parameter.h>
#include <sqlpp11/parameter_list.h>

// For data types
#include <sqlpp11/data_types/blob/data_type.h>
#include <sqlpp11/data_types/floating_point.h>
#include <sqlpp11/data_types/integral.h>

// For common operations
#include <sqlpp11/aggregate_functions.h>
#include <sqlpp11/boolean_expression.h>
#include <sqlpp11/char_sequence.h>
#include <sqlpp11/functions.h>
#include <sqlpp11/insert.h>
#include <sqlpp11/null.h>
#include <sqlpp11/select.h>
#include <sqlpp11/table.h>
#include <sqlpp11/transaction.h>
#include <sqlpp11/update.h>
#include <sqlpp11/where.h>

// For prepared statements (if using that approach)
#include <sqlpp11/prepared_insert.h>
#include <sqlpp11/prepared_select.h>
#include <sqlpp11/prepared_update.h>
