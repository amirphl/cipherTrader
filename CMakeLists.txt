cmake_minimum_required(VERSION 3.20)

# Project name and version
project(CipherTrader VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_UNITY_BUILD ON)

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Parallel compilation
include(ProcessorCount)
ProcessorCount(N)
if(NOT N EQUAL 0)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
    message(STATUS "Building with ${N} parallel jobs")
endif()

# Compiler configuration
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using Clang")
endif()

# Use Clang
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "It is recommended to use Clang for this project.")
endif()
# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Common flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")  # Optimize for your CPU
    endif()

    # Link-time optimization for release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

# Use pkg-config for better package detection on Linux
find_package(PkgConfig REQUIRED)

find_package(Boost REQUIRED COMPONENTS system program_options)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

# Use pkg-config for libraries that might not have proper CMake configs
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Manual detection for redis++ since it doesn't have pkg-config
find_path(REDIS_PLUS_PLUS_INCLUDE_DIR
    NAMES sw/redis++/redis++.h redis++.h
    PATHS /usr/local/include /usr/include
    PATH_SUFFIXES sw/redis++
)

find_library(REDIS_PLUS_PLUS_LIBRARY
    NAMES redis++ libredis++
    PATHS /usr/local/lib /usr/lib
)

if(REDIS_PLUS_PLUS_INCLUDE_DIR AND REDIS_PLUS_PLUS_LIBRARY)
    set(REDIS_PLUS_PLUS_FOUND TRUE)
    message(STATUS "Found redis++: ${REDIS_PLUS_PLUS_LIBRARY}")
    message(STATUS "Redis++ include dir: ${REDIS_PLUS_PLUS_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "redis++ not found")
endif()

# PostgreSQL - try different approaches
find_package(PostgreSQL QUIET)
if(NOT PostgreSQL_FOUND)
    pkg_check_modules(PostgreSQL REQUIRED libpq)
endif()

# Blaze - try to find it properly
find_package(blaze QUIET)
if(NOT blaze_FOUND)
    find_path(BLAZE_INCLUDE_DIR blaze/Blaze.h
        PATHS /usr/include /usr/local/include
        PATH_SUFFIXES blaze
    )
    if(BLAZE_INCLUDE_DIR)
        add_library(blaze::blaze INTERFACE IMPORTED)
        target_include_directories(blaze::blaze INTERFACE ${BLAZE_INCLUDE_DIR})
        message(STATUS "Found Blaze at: ${BLAZE_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Blaze not found")
    endif()
endif()

# SQLPP11 - try different approaches
find_package(Sqlpp11 QUIET)
if(NOT Sqlpp11_FOUND)
    pkg_check_modules(SQLPP11 QUIET sqlpp11)
    if(SQLPP11_FOUND)
        add_library(sqlpp11::sqlpp11 INTERFACE IMPORTED)
        target_include_directories(sqlpp11::sqlpp11 INTERFACE ${SQLPP11_INCLUDE_DIRS})
        target_link_libraries(sqlpp11::sqlpp11 INTERFACE ${SQLPP11_LIBRARIES})
    else()
        message(FATAL_ERROR "SQLPP11 not found")
    endif()
endif()

find_path(CEREAL_INCLUDE_DIR
    NAMES cereal/cereal.hpp
    # PATHS /usr/include /usr/local/include /opt/local/include
    PATHS ${CMAKE_SOURCE_DIR}/external/cereal/include
    PATH_SUFFIXES cereal
)

if(CEREAL_INCLUDE_DIR)
    message(STATUS "Found Cereal: ${CEREAL_INCLUDE_DIR}")
    add_library(cereal INTERFACE)
    target_include_directories(cereal INTERFACE ${CEREAL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Cereal not found. Please ensure Cereal is installed.")
endif()

# Precompiled headers for frequently used headers
set(PCH_HEADERS
  <algorithm>
  <any>
  <atomic>
  <chrono>
  <cmath>
  <csignal>
  <cstdint>
  <cstdlib>
  <cstring>
  <ctime>
  <dlfcn.h>
  <exception>
  <filesystem>
  <fstream>
  <functional>
  <future>
  <iomanip>
  <iostream>
  <limits>
  <map>
  <memory>
  <mutex>
  <optional>
  <queue>
  <random>
  <regex>
  <set>
  <sstream>
  <stdexcept>
  <string>
  <thread>
  <tuple>
  <typeinfo>
  <unistd.h>
  <unordered_map>
  <utility>
  <variant>
  <vector>
  <zlib.h>
  <date/date.h>
  <openssl/md5.h>
  <openssl/sha.h>
  <openssl/evp.h>
  <spdlog/sinks/rotating_file_sink.h>
  <spdlog/sinks/stdout_color_sinks.h>
  <spdlog/spdlog.h>
  <blaze/Math.h>
  <nlohmann/json.hpp>
  <yaml-cpp/yaml.h>
  <sw/redis++/redis++.h>
  <boost/algorithm/string.hpp>
  <boost/beast/core/detail/base64.hpp>
  <boost/format.hpp>
  <boost/multiprecision/cpp_dec_float.hpp>
  <boost/uuid/random_generator.hpp>
  <boost/uuid/string_generator.hpp>
  <boost/uuid/uuid.hpp>
  <boost/uuid/uuid_generators.hpp>
  <boost/uuid/uuid_io.hpp>
  <sqlpp11/postgresql/connection.h>
  <sqlpp11/postgresql/connection_config.h>
  <sqlpp11/postgresql/postgresql.h>
  <sqlpp11/parameter.h>
  <sqlpp11/parameter_list.h>
  <sqlpp11/data_types/blob/data_type.h>
  <sqlpp11/data_types/floating_point.h>
  <sqlpp11/data_types/integral.h>
  <sqlpp11/aggregate_functions.h>
  <sqlpp11/boolean_expression.h>
  <sqlpp11/char_sequence.h>
  <sqlpp11/functions.h>
  <sqlpp11/insert.h>
  <sqlpp11/null.h>
  <sqlpp11/select.h>
  <sqlpp11/table.h>
  <sqlpp11/transaction.h>
  <sqlpp11/update.h>
  <sqlpp11/where.h>
  <sqlpp11/prepared_insert.h>
  <sqlpp11/prepared_select.h>
  <sqlpp11/prepared_update.h>
  <cereal/cereal.hpp>
  <cereal/archives/json.hpp>
  <cereal/archives/binary.hpp>
  <cereal/types/chrono.hpp>
  <cereal/types/memory.hpp>
  <cereal/types/optional.hpp>
  <cereal/types/map.hpp>
  <cereal/types/unordered_map.hpp>
  <cereal/types/string.hpp>
  <cereal/types/vector.hpp>
)

# Source files - consider using explicit file lists for better incremental builds
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/include/*.hpp")

# Create main library
add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})

# Set precompiled headers
target_precompile_headers(${PROJECT_NAME}_lib PRIVATE ${PCH_HEADERS})

# Set library properties
set_target_properties(${PROJECT_NAME}_lib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# Target-specific include directories
target_include_directories(${PROJECT_NAME}_lib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

# Link dependencies to main library
target_link_libraries(${PROJECT_NAME}_lib
    PUBLIC
        Boost::system
        Boost::program_options
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        yaml-cpp::yaml-cpp
        spdlog::spdlog
        blaze::blaze
        cereal
)

# Link pkg-config dependencies
if(HIREDIS_FOUND)
    target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${HIREDIS_LIBRARIES})
    target_include_directories(${PROJECT_NAME}_lib PUBLIC ${HIREDIS_INCLUDE_DIRS})
endif()

# if(REDIS_PLUS_PLUS_FOUND)
#     target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${REDIS_PLUS_PLUS_LIBRARIES})
#     target_include_directories(${PROJECT_NAME}_lib PUBLIC ${REDIS_PLUS_PLUS_INCLUDE_DIRS})
# endif()
if(REDIS_PLUS_PLUS_FOUND)
    target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${REDIS_PLUS_PLUS_LIBRARY})
    target_include_directories(${PROJECT_NAME}_lib PUBLIC ${REDIS_PLUS_PLUS_INCLUDE_DIR})
endif()

if(PostgreSQL_FOUND)
    if(TARGET PostgreSQL::PostgreSQL)
        target_link_libraries(${PROJECT_NAME}_lib PUBLIC PostgreSQL::PostgreSQL)
    else()
        target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${PostgreSQL_LIBRARIES})
        target_include_directories(${PROJECT_NAME}_lib PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    endif()
endif()

if(Sqlpp11_FOUND OR SQLPP11_FOUND)
    if(TARGET sqlpp11::sqlpp11)
        target_link_libraries(${PROJECT_NAME}_lib PUBLIC sqlpp11::sqlpp11)
    else()
        target_link_libraries(${PROJECT_NAME}_lib PUBLIC ${SQLPP11_LIBRARIES})
        target_include_directories(${PROJECT_NAME}_lib PUBLIC ${SQLPP11_INCLUDE_DIRS})
    endif()
endif()

# Create main executable
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Test configuration improvements
find_package(GTest REQUIRED)
file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/*.cpp")

add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# Ensure test target has proper include directories
target_include_directories(${PROJECT_NAME}_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests  # Add this if you have test headers
)

# Link all the same dependencies as the main library
target_link_libraries(${PROJECT_NAME}_tests
    PRIVATE
        ${PROJECT_NAME}_lib
        GTest::GTest
        GTest::Main
)

# Ensure tests have the same PIC setting as the library
set_target_properties(${PROJECT_NAME}_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
    POSITION_INDEPENDENT_CODE ON  # Match the library setting
)

# Apply the same precompiled headers to tests
# target_precompile_headers(${PROJECT_NAME}_tests REUSE_FROM ${PROJECT_NAME}_lib)
target_precompile_headers(${PROJECT_NAME}_tests PRIVATE ${PCH_HEADERS})

# Ensure tests use the same C++ standard
set_target_properties(${PROJECT_NAME}_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)

enable_testing()
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

# # Installation rules
# install(TARGETS ${PROJECT_NAME}_lib ${PROJECT_NAME}
#     EXPORT ${PROJECT_NAME}Targets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
#     INCLUDES DESTINATION include
# )
#
# install(DIRECTORY include/
#     DESTINATION include
#     FILES_MATCHING PATTERN "*.hpp"
# )
#
# install(EXPORT ${PROJECT_NAME}Targets
#     FILE ${PROJECT_NAME}Targets.cmake
#     NAMESPACE ${PROJECT_NAME}::
#     DESTINATION lib/cmake/${PROJECT_NAME}
# )
