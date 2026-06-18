cmake_minimum_required(VERSION 3.20)

project(spot_lead_lag_validator
    VERSION 0.1.0
    DESCRIPTION "C++20 paper validation framework for spot lead-lag latency research"
    LANGUAGES CXX)

option(LATENCY_ARB_BUILD_TESTS "Build unit tests" ON)
option(LATENCY_ARB_FETCH_DEPS "Fetch fmt/spdlog when not installed" OFF)
option(LATENCY_ARB_BUILD_LIVE "Build public websocket live paper runner" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fmt CONFIG QUIET)
find_package(spdlog CONFIG QUIET)
find_package(OpenSSL QUIET)
find_package(Boost QUIET COMPONENTS system)

if((NOT fmt_FOUND OR NOT spdlog_FOUND) AND LATENCY_ARB_FETCH_DEPS)
    include(FetchContent)
    if(NOT fmt_FOUND)
        FetchContent_Declare(fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 10.2.1)
        FetchContent_MakeAvailable(fmt)
    endif()
    if(NOT spdlog_FOUND)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.14.1)
        FetchContent_MakeAvailable(spdlog)
    endif()
endif()

if(NOT fmt_FOUND AND NOT TARGET fmt::fmt)
    message(FATAL_ERROR "fmt is required. Install fmt or configure with -DLATENCY_ARB_FETCH_DEPS=ON.")
endif()

if(NOT spdlog_FOUND AND NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog is required. Install spdlog or configure with -DLATENCY_ARB_FETCH_DEPS=ON.")
endif()

add_library(latency_arb
    src/Backtester.cpp
    src/BookManager.cpp
    src/Config.cpp
    src/CsvLogger.cpp
    src/MarketObserver.cpp
    src/PaperTrader.cpp
    src/RiskAnalyzer.cpp
    src/SignalDetector.cpp
    src/StatisticsEngine.cpp
    src/TradeValidator.cpp
    $<$<BOOL:${LATENCY_ARB_BUILD_LIVE}>:src/LiveMarketData.cpp>)

target_include_directories(latency_arb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(latency_arb PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(latency_arb PRIVATE /W4 /permissive-)
else()
    target_compile_options(latency_arb PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(latency_arb
    PUBLIC
        fmt::fmt
        spdlog::spdlog)

if(LATENCY_ARB_BUILD_LIVE)
    if(NOT OpenSSL_FOUND OR NOT Boost_FOUND)
        message(FATAL_ERROR "OpenSSL and Boost::system are required for LATENCY_ARB_BUILD_LIVE=ON")
    endif()
    target_compile_definitions(latency_arb PUBLIC LATENCY_ARB_BUILD_LIVE=1)
    target_link_libraries(latency_arb PUBLIC OpenSSL::SSL OpenSSL::Crypto Boost::system)
endif()

add_executable(latency_arb_backtest app/main.cpp)
target_link_libraries(latency_arb_backtest PRIVATE latency_arb)

if(LATENCY_ARB_BUILD_LIVE)
    add_executable(latency_arb_live app/live_main.cpp)
    target_link_libraries(latency_arb_live PRIVATE latency_arb)
endif()

if(LATENCY_ARB_BUILD_TESTS)
    enable_testing()
    add_executable(latency_arb_tests tests/engine_tests.cpp)
    target_link_libraries(latency_arb_tests PRIVATE latency_arb)
    add_test(NAME latency_arb_tests COMMAND latency_arb_tests)
endif()
