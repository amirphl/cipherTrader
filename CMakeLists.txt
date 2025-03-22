cmake_minimum_required(VERSION 3.20)

# Project name
project(CipherTrader)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Use Clang as the compiler (Optional: Only if you specifically want Clang)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using Clang")
endif()

# Use Clang
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "It is recommended to use Clang for this project.")
endif()

# Set compiler flags
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -g -O0")

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include Boos"Rout"
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Blaze REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create a library for the main project code
add_library(${PROJECT_NAME}_lib ${SOURCES})

# Link Boost and nlohmann_json to the library
target_link_libraries(${PROJECT_NAME}_lib ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

# For fixing Candle.hpp not found in Candle.cpp.
# Add include directories to the library target
target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
		${Boost_INCLUDE_DIRS}
)

# Create the main executable and link it to the library
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/main.cpp)  # Assuming main.cpp exists
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib)

# Add Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Test source files
file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/*.cpp")

# Create a test executable
add_executable(t ${TEST_SOURCES})

# Link test executable with Google Test, the project library, and other dependencies
target_link_libraries(t ${PROJECT_NAME}_lib GTest::GTest GTest::Main ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

# Add a test target
enable_testing()
add_test(NAME t COMMAND ${PROJECT_NAME}_tests)
