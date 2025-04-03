cmake_minimum_required(VERSION 3.20)

# Project name and version
project(CipherTrader VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # TODO

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

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)
find_package(Blaze REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GTest REQUIRED)
find_package(sqlpp11 REQUIRED)
find_package(PostgreSQL REQUIRED)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OpenSSL_INCLUDE_DIR}
		${ZLIB_INCLUDE_DIRS}
		${sqlpp11_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
)

# Source files
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/*.cpp")

# Create main library
add_library(${PROJECT_NAME}_lib STATIC ${SOURCES})

# Set library properties
set_target_properties(${PROJECT_NAME}_lib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Link dependencies to main library
target_link_libraries(${PROJECT_NAME}_lib
    PUBLIC
        ${Boost_LIBRARIES}
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
				${ZLIB_LIBRARIES}
				sqlpp11::sqlpp11
        ${PostgreSQL_LIBRARIES}
)

# Include directories for main library
target_include_directories(${PROJECT_NAME}_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        ${OpenSSL_INCLUDE_DIR}
				${ZLIB_INCLUDE_DIRS}
        ${sqlpp11_INCLUDE_DIRS}
        ${PostgreSQL_INCLUDE_DIRS}
)

# Create main executable
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Create test executable
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_tests
    PRIVATE
        ${PROJECT_NAME}_lib
        GTest::GTest
        GTest::Main
        ${Boost_LIBRARIES}
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
				${ZLIB_LIBRARIES}
        sqlpp11::sqlpp11
        ${PostgreSQL_LIBRARIES}
)

# Enable testing
enable_testing()
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

# Installation rules
install(TARGETS ${PROJECT_NAME}_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)









# add_library(my_trading_lib SHARED <your_lib_sources>)
# set_target_properties(my_trading_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# add_library(strategy_loader strategy_loader.cpp)
# target_include_directories(strategy_loader PUBLIC ${CMAKE_SOURCE_DIR}/include)
# target_link_libraries(strategy_loader PRIVATE my_trading_lib dl) # Link against dl for dlopen

# add_library(MyStrategy SHARED strategies/MyStrategy/main.cpp)
# target_include_directories(MyStrategy PUBLIC ${CMAKE_SOURCE_DIR}/include)
# target_link_libraries(MyStrategy PRIVATE my_trading_lib)
# set_target_properties(MyStrategy PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/strategies/MyStrategy)

