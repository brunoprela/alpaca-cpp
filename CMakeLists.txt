cmake_minimum_required(VERSION 3.20)
project(alpaca-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALPACA_BUILD_TESTS "Build unit tests" ON)
option(ALPACA_BUILD_LIVE_TEST "Build integration tests that hit live APIs" OFF)
option(ALPACA_ENABLE_WARNINGS "Enable recommended warnings" ON)
option(ALPACA_VENDOR_DEPS "Fetch required third-party dependencies" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(ALPACA_ENABLE_WARNINGS)
    include(CheckCXXCompilerFlag)
    set(_warning_flags
        -Wall
        -Wextra
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -pedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion)
    foreach(flag IN LISTS _warning_flags)
        check_cxx_compiler_flag(${flag} HAS_${flag})
        if(HAS_${flag})
            add_compile_options(${flag})
        endif()
    endforeach()
endif()

set(ALPACA_BOOST_INCLUDE_DIR "")
set(ALPACA_SIMDJSON_TARGET "")

if(ALPACA_VENDOR_DEPS)
    include(FetchContent)

    set(BOOST_ENABLE_CMAKE ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        boost
        URL https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2)
    FetchContent_MakeAvailable(boost)
    set(ALPACA_BOOST_INCLUDE_DIR ${boost_SOURCE_DIR})
    set(BOOST_URL_SOURCES
        ${boost_SOURCE_DIR}/libs/url/src/authority_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/decode_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/encoding_opts.cpp
        ${boost_SOURCE_DIR}/libs/url/src/error.cpp
        ${boost_SOURCE_DIR}/libs/url/src/ipv4_address.cpp
        ${boost_SOURCE_DIR}/libs/url/src/ipv6_address.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_encoded_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_encoded_ref.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_encoded_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_ref.cpp
        ${boost_SOURCE_DIR}/libs/url/src/params_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/parse.cpp
        ${boost_SOURCE_DIR}/libs/url/src/parse_path.cpp
        ${boost_SOURCE_DIR}/libs/url/src/parse_query.cpp
        ${boost_SOURCE_DIR}/libs/url/src/pct_string_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/scheme.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_encoded_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_encoded_ref.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_encoded_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_ref.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/static_url.cpp
        ${boost_SOURCE_DIR}/libs/url/src/url.cpp
        ${boost_SOURCE_DIR}/libs/url/src/url_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/url_view.cpp
        ${boost_SOURCE_DIR}/libs/url/src/url_view_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/any_params_iter.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/any_segments_iter.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/decode.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/except.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/format_args.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/normalize.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/pattern.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/pct_format.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/params_iter_impl.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/replacement_field_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/segments_iter_impl.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/url_impl.cpp
        ${boost_SOURCE_DIR}/libs/url/src/detail/vformat.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/ci_string.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/dec_octet_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/delim_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/detail/recycled.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/error.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/literal_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/grammar/string_view_base.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/absolute_uri_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/authority_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/ipv4_address_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/ipv6_address_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/origin_form_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/query_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/relative_ref_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/uri_reference_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/uri_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/h16_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/host_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/hier_part_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/ipv6_addrz_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/ipvfuture_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/ip_literal_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/port_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/relative_part_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/scheme_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/detail/userinfo_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/rfc/uri_rule.cpp
        ${boost_SOURCE_DIR}/libs/url/src/segments_view.cpp)

    FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG v3.9.2)
    FetchContent_MakeAvailable(simdjson)
    set(ALPACA_SIMDJSON_TARGET simdjson)
else()
    find_package(Boost REQUIRED)
    find_package(simdjson CONFIG REQUIRED)
    set(ALPACA_BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
    set(ALPACA_SIMDJSON_TARGET simdjson::simdjson)
    set(BOOST_URL_SOURCES)
endif()

find_package(OpenSSL REQUIRED)

add_library(alpaca_core
    src/alpaca/core/config.cpp
    src/alpaca/core/http_transport.cpp
    src/alpaca/core/http/beast_transport.cpp
    src/alpaca/core/json.cpp
    src/alpaca/core/dotenv.cpp
    ${BOOST_URL_SOURCES})
target_include_directories(alpaca_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${ALPACA_BOOST_INCLUDE_DIR})
target_compile_features(alpaca_core PUBLIC cxx_std_20)
target_link_libraries(alpaca_core PUBLIC ${ALPACA_SIMDJSON_TARGET} OpenSSL::SSL OpenSSL::Crypto)

add_library(alpaca::core ALIAS alpaca_core)

add_library(alpaca_trading
    src/alpaca/trading/client.cpp
    src/alpaca/trading/stream.cpp)
target_link_libraries(alpaca_trading PUBLIC alpaca::core)
if(ALPACA_VENDOR_DEPS)
    target_include_directories(alpaca_trading PUBLIC ${ALPACA_BOOST_INCLUDE_DIR})
else()
    target_link_libraries(alpaca_trading PUBLIC Boost::boost)
endif()
target_include_directories(alpaca_trading
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_features(alpaca_trading PUBLIC cxx_std_20)

add_library(alpaca::trading ALIAS alpaca_trading)

add_library(alpaca_broker
    src/alpaca/broker/client.cpp
    src/alpaca/broker/enums.cpp)
target_link_libraries(alpaca_broker PUBLIC alpaca::core)
target_include_directories(alpaca_broker
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_features(alpaca_broker PUBLIC cxx_std_20)

add_library(alpaca::broker ALIAS alpaca_broker)

add_library(alpaca_data
    src/alpaca/data/client.cpp
    src/alpaca/data/live/websocket.cpp
    src/alpaca/data/live/stock.cpp
    src/alpaca/data/live/crypto.cpp
    src/alpaca/data/live/option.cpp
    src/alpaca/data/live/news.cpp)
target_link_libraries(alpaca_data PUBLIC alpaca::core)
if(ALPACA_VENDOR_DEPS)
    target_include_directories(alpaca_data PUBLIC ${ALPACA_BOOST_INCLUDE_DIR})
else()
    target_link_libraries(alpaca_data PUBLIC Boost::boost)
endif()
target_include_directories(alpaca_data
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_compile_features(alpaca_data PUBLIC cxx_std_20)

add_library(alpaca::data ALIAS alpaca_data)

add_executable(alpaca_example_place_order examples/place_order.cpp)
target_link_libraries(alpaca_example_place_order PRIVATE alpaca::trading)

add_executable(alpaca_example_get_account examples/get_account.cpp)
target_link_libraries(alpaca_example_get_account PRIVATE alpaca::trading)

if(ALPACA_BUILD_TESTS)
    enable_testing()
    add_executable(alpaca_core_tests tests/unit/test_config.cpp)
    target_link_libraries(alpaca_core_tests PRIVATE alpaca::core)
    add_test(NAME alpaca_core_tests COMMAND alpaca_core_tests)

    add_executable(alpaca_trading_tests tests/unit/test_trading_client.cpp)
    target_link_libraries(alpaca_trading_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_tests COMMAND alpaca_trading_tests)

    add_executable(alpaca_trading_account_tests tests/unit/test_trading_account.cpp)
    target_link_libraries(alpaca_trading_account_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_account_tests COMMAND alpaca_trading_account_tests)

    add_executable(alpaca_trading_position_tests tests/unit/test_trading_positions.cpp)
    target_link_libraries(alpaca_trading_position_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_position_tests COMMAND alpaca_trading_position_tests)

    add_executable(alpaca_trading_asset_tests tests/unit/test_trading_assets.cpp)
    target_link_libraries(alpaca_trading_asset_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_asset_tests COMMAND alpaca_trading_asset_tests)

    add_executable(alpaca_trading_calendar_tests tests/unit/test_trading_calendar.cpp)
    target_link_libraries(alpaca_trading_calendar_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_calendar_tests COMMAND alpaca_trading_calendar_tests)

    add_executable(alpaca_trading_activity_tests tests/unit/test_trading_activities.cpp)
    target_link_libraries(alpaca_trading_activity_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_activity_tests COMMAND alpaca_trading_activity_tests)

    add_executable(alpaca_trading_watchlist_tests tests/unit/test_trading_watchlists.cpp)
    target_link_libraries(alpaca_trading_watchlist_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_watchlist_tests COMMAND alpaca_trading_watchlist_tests)

    add_executable(alpaca_broker_transfer_tests tests/unit/test_broker_transfers.cpp)
    target_link_libraries(alpaca_broker_transfer_tests PRIVATE alpaca::broker)
    add_test(NAME alpaca_broker_transfer_tests COMMAND alpaca_broker_transfer_tests)

    add_executable(alpaca_broker_journal_tests tests/unit/test_broker_journals.cpp)
    target_link_libraries(alpaca_broker_journal_tests PRIVATE alpaca::broker)
    add_test(NAME alpaca_broker_journal_tests COMMAND alpaca_broker_journal_tests)

    add_executable(alpaca_broker_assets_orders_tests tests/unit/test_broker_assets_orders.cpp)
    target_link_libraries(alpaca_broker_assets_orders_tests PRIVATE alpaca::broker)
    add_test(NAME alpaca_broker_assets_orders_tests COMMAND alpaca_broker_assets_orders_tests)

    add_executable(alpaca_broker_corporate_actions_tests tests/unit/test_broker_corporate_actions.cpp)
    target_link_libraries(alpaca_broker_corporate_actions_tests PRIVATE alpaca::broker)
    add_test(NAME alpaca_broker_corporate_actions_tests COMMAND alpaca_broker_corporate_actions_tests)

    add_executable(alpaca_broker_events_tests tests/unit/test_broker_events.cpp)
    target_link_libraries(alpaca_broker_events_tests PRIVATE alpaca::broker)
    add_test(NAME alpaca_broker_events_tests COMMAND alpaca_broker_events_tests)

    add_executable(alpaca_trading_transfer_tests tests/unit/test_trading_transfers.cpp)
    target_link_libraries(alpaca_trading_transfer_tests PRIVATE alpaca::trading)
    add_test(NAME alpaca_trading_transfer_tests COMMAND alpaca_trading_transfer_tests)

    add_executable(alpaca_data_stock_bars_tests tests/unit/test_data_stock_bars.cpp)
    target_link_libraries(alpaca_data_stock_bars_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_stock_bars_tests COMMAND alpaca_data_stock_bars_tests)
    add_executable(alpaca_data_stock_quotes_tests tests/unit/test_data_stock_quotes.cpp)
    target_link_libraries(alpaca_data_stock_quotes_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_stock_quotes_tests COMMAND alpaca_data_stock_quotes_tests)
    add_executable(alpaca_data_latest_quotes_tests tests/unit/test_data_latest_quotes.cpp)
    target_link_libraries(alpaca_data_latest_quotes_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_latest_quotes_tests COMMAND alpaca_data_latest_quotes_tests)
    add_executable(alpaca_data_stock_trades_tests tests/unit/test_data_stock_trades.cpp)
    target_link_libraries(alpaca_data_stock_trades_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_stock_trades_tests COMMAND alpaca_data_stock_trades_tests)
    add_executable(alpaca_data_latest_trades_tests tests/unit/test_data_latest_trades.cpp)
    target_link_libraries(alpaca_data_latest_trades_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_latest_trades_tests COMMAND alpaca_data_latest_trades_tests)
    add_executable(alpaca_data_latest_trades_reverse_tests
                   tests/unit/test_data_latest_trades_reverse.cpp)
    target_link_libraries(alpaca_data_latest_trades_reverse_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_latest_trades_reverse_tests
             COMMAND alpaca_data_latest_trades_reverse_tests)
    add_executable(alpaca_data_latest_bars_tests tests/unit/test_data_latest_bars.cpp)
    target_link_libraries(alpaca_data_latest_bars_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_latest_bars_tests COMMAND alpaca_data_latest_bars_tests)
    add_executable(alpaca_data_snapshots_tests tests/unit/test_data_snapshots.cpp)
    target_link_libraries(alpaca_data_snapshots_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_snapshots_tests COMMAND alpaca_data_snapshots_tests)
    add_executable(alpaca_data_crypto_bars_tests tests/unit/test_data_crypto_bars.cpp)
    target_link_libraries(alpaca_data_crypto_bars_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_bars_tests COMMAND alpaca_data_crypto_bars_tests)
    add_executable(alpaca_data_crypto_latest_orderbooks_tests
                   tests/unit/test_data_crypto_latest_orderbooks.cpp)
    target_link_libraries(alpaca_data_crypto_latest_orderbooks_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_latest_orderbooks_tests
             COMMAND alpaca_data_crypto_latest_orderbooks_tests)
    add_executable(alpaca_data_crypto_quotes_tests tests/unit/test_data_crypto_quotes.cpp)
    target_link_libraries(alpaca_data_crypto_quotes_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_quotes_tests COMMAND alpaca_data_crypto_quotes_tests)
    add_executable(alpaca_data_crypto_trades_tests tests/unit/test_data_crypto_trades.cpp)
    target_link_libraries(alpaca_data_crypto_trades_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_trades_tests COMMAND alpaca_data_crypto_trades_tests)
    add_executable(alpaca_data_crypto_latest_tests tests/unit/test_data_crypto_latest.cpp)
    target_link_libraries(alpaca_data_crypto_latest_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_latest_tests COMMAND alpaca_data_crypto_latest_tests)
    add_executable(alpaca_data_crypto_snapshots_tests tests/unit/test_data_crypto_snapshots.cpp)
    target_link_libraries(alpaca_data_crypto_snapshots_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_crypto_snapshots_tests COMMAND alpaca_data_crypto_snapshots_tests)
    add_executable(alpaca_data_options_tests tests/unit/test_data_options.cpp)
    target_link_libraries(alpaca_data_options_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_options_tests COMMAND alpaca_data_options_tests)
    add_executable(alpaca_data_screener_tests tests/unit/test_data_screener.cpp)
    target_link_libraries(alpaca_data_screener_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_screener_tests COMMAND alpaca_data_screener_tests)
    add_executable(alpaca_data_news_tests tests/unit/test_data_news.cpp)
    target_link_libraries(alpaca_data_news_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_news_tests COMMAND alpaca_data_news_tests)
    add_executable(alpaca_data_corporate_actions_tests tests/unit/test_data_corporate_actions.cpp)
    target_link_libraries(alpaca_data_corporate_actions_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_corporate_actions_tests COMMAND alpaca_data_corporate_actions_tests)
    add_executable(alpaca_data_option_meta_exchanges_tests tests/unit/test_data_option_meta_exchanges.cpp)
    target_link_libraries(alpaca_data_option_meta_exchanges_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_option_meta_exchanges_tests COMMAND alpaca_data_option_meta_exchanges_tests)
    add_executable(alpaca_data_raw_endpoints_tests tests/unit/test_data_raw_endpoints.cpp)
    target_link_libraries(alpaca_data_raw_endpoints_tests PRIVATE alpaca::data)
    add_test(NAME alpaca_data_raw_endpoints_tests COMMAND alpaca_data_raw_endpoints_tests)

    if(ALPACA_BUILD_LIVE_TEST)
        add_executable(alpaca_trading_live_tests tests/integration/test_trading_live.cpp)
        target_link_libraries(alpaca_trading_live_tests PRIVATE alpaca::trading)
        add_test(NAME alpaca_trading_live_tests COMMAND alpaca_trading_live_tests)
        set_tests_properties(alpaca_trading_live_tests PROPERTIES
            PASS_REGULAR_EXPRESSION "Live account cash")
    endif()
endif()
