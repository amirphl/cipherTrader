cmake_minimum_required(VERSION 3.20)
project(NobitexArbitrageBot)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Debug)

# Compiler configuration
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using Clang")
endif()

# Compiler flags
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -ggdb")
# add_compile_definitions(DEBUG_BUILD)

# Completely disable optimizations and enable full debug symbols
set(CMAKE_CXX_FLAGS "-g -O0")
set(CMAKE_C_FLAGS "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()


# Add debug build definition
# add_compile_definitions(DEBUG_BUILD)

# Find required packages
find_package(cpprestsdk REQUIRED)
find_package(Boost REQUIRED COMPONENTS system json url random)
find_package(OpenSSL REQUIRED)

# Add executable
add_executable(arbitrage_bot main.cpp)

# Link libraries
target_link_libraries(arbitrage_bot 
    PRIVATE 
    Boost::system
    Boost::json
    Boost::url
		Boost::random
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Add compiler warnings
if(MSVC)
  target_compile_options(arbitrage_bot PRIVATE /W4)
else()
  # target_compile_options(arbitrage_bot PRIVATE -Wall -Wextra -Wpedantic -stdlib=libc++ -g -O0)
  target_compile_options(arbitrage_bot 
    PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic 
    # -stdlib=libc++ 
    -g
    # -g3
    -O0 
    # -ggdb
    # -fno-optimize-sibling-calls
  )
endif()

# Output binary to bin directory
set_target_properties(arbitrage_bot
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install target
install(TARGETS arbitrage_bot DESTINATION bin)
