cmake_minimum_required(VERSION 3.10)

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

# Include Boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link Boost libraries if needed
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
