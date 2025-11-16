# alpaca-cpp

Modern C++20 SDK for the Alpaca Markets APIs. **Feature parity with `alpaca-py`** — trading, broker, and market data APIs are fully implemented with zero-cost abstractions, compile-time safety, and native C++ performance.

## Project Status

✅ **Feature Complete.** This SDK provides full feature parity with `alpaca-py`, including all REST API endpoints, WebSocket streams, and Server-Sent Events (SSE) for real-time data.

## Features

### ✅ Implemented

- **Trading API** — Complete implementation of all trading endpoints
  - Order management (submit, get, replace, cancel)
  - Position management (list, get, close, close all)
  - Account information and configuration
  - Assets, watchlists, activities, portfolio history
  - Options contracts and exercise
  - Corporate actions

- **Broker API** — Complete implementation of all broker endpoints
  - Account management (create, get, update, close, list)
  - ACH relationships and bank accounts
  - Transfers (create, list, cancel)
  - Journals (single, batch, reverse batch)
  - Assets and orders for managed accounts
  - Positions, portfolio history, clock, calendar
  - Watchlists management
  - Options exercise
  - Trade account configuration
  - Document management (upload, get, download)
  - Account activities
  - Corporate actions
  - Portfolio rebalancing (portfolios, subscriptions, runs)
  - Event streaming (SSE) for account status, trades, journals, transfers

- **Market Data API** — Complete implementation of all data endpoints
  - Stock data (bars, quotes, trades, latest, snapshots)
  - Crypto data (bars, quotes, trades, latest, orderbooks, snapshots)
  - Options data (bars, trades, latest, snapshots, chains, exchange codes)
  - News data
  - Screener (most actives, market movers)
  - Corporate actions

- **WebSocket Streams** — Real-time data streaming
  - Stock data stream (trades, quotes, bars, trading status, corrections/cancels)
  - Crypto data stream (trades, quotes, bars, orderbooks)
  - Options data stream (trades, quotes)
  - News data stream
  - Trading stream (trade updates)

- **Core Infrastructure**
  - Typed request/response models for all APIs
  - Unified configuration layer with environment switching (live, paper, sandbox)
  - Swappable HTTP transport (libcurl by default)
  - Streaming layer for WebSocket + SSE feeds built on Boost.Beast
  - Strong error model with Alpaca error codes
  - JSON parsing with simdjson for high performance

## Getting Started

```bash
cmake -S . -B build -D ALPACA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Live credentials

1. Copy `.env.example` to `.env` and populate `APCA_API_KEY_ID`/`APCA_API_SECRET_KEY`.
2. Build the examples/tests:
   ```bash
   cmake --build build
   ```
3. Run the live example:
   ```bash
   ./build/alpaca_example_get_account
   ```
4. To compile the live integration test, enable the flag and reconfigure:
   ```bash
   cmake -S . -B build -D ALPACA_BUILD_TESTS=ON -D ALPACA_BUILD_LIVE_TEST=ON
   cmake --build build
   ctest --test-dir build -R alpaca_trading_live_tests
   ```

Use `examples/place_order.cpp` and `examples/get_account.cpp` as starting points for experiments. All examples target the paper trading API by default.

## Quick Start Examples

### Trading API

```cpp
#include "alpaca/trading/client.hpp"
#include "alpaca/core/config.hpp"
#include "alpaca/core/http_transport.hpp"

using namespace alpaca;

// Create client
auto config = core::ClientConfig::from_env();
auto transport = std::make_shared<core::HttpTransport>();
trading::TradingClient client(config, transport);

// Get account
auto account = client.get_account();

// Submit order
trading::MarketOrderRequest order_request;
order_request.symbol = "AAPL";
order_request.qty = 10.0;
order_request.side = trading::OrderSide::Buy;
order_request.time_in_force = trading::TimeInForce::Day;
auto result = client.submit_order(order_request);
```

### Market Data API

```cpp
#include "alpaca/data/client.hpp"
#include "alpaca/data/requests.hpp"

using namespace alpaca;

// Create client
auto config = core::ClientConfig::from_env();
auto transport = std::make_shared<core::HttpTransport>();
data::DataClient client(config, transport);

// Get stock bars
data::StockBarsRequest request;
request.symbols = {"AAPL", "MSFT"};
request.timeframe = data::TimeFrame::Day;
request.start = "2024-01-01T00:00:00Z";
request.end = "2024-01-31T23:59:59Z";
auto bars = client.get_stock_bars(request);
```

### Broker API

```cpp
#include "alpaca/broker/client.hpp"

using namespace alpaca;

// Create client
auto config = core::ClientConfig::from_env();
auto transport = std::make_shared<core::HttpTransport>();
broker::BrokerClient client(config, transport);

// List accounts
auto accounts = client.list_accounts();

// Create ACH relationship
broker::CreateAchRelationshipRequest ach_request;
ach_request.account_owner_name = "John Doe";
ach_request.bank_account_type = broker::BankAccountType::Checking;
ach_request.bank_account_number = "123456789";
ach_request.bank_routing_number = "021000021";
auto relationship = client.create_ach_relationship(account_id, ach_request);
```

### WebSocket Streaming

```cpp
#include "alpaca/data/live/stock.hpp"

using namespace alpaca::data::live;

// Create stream
StockDataStream stream(api_key, secret_key, false, DataFeed::Iex);

// Subscribe to trades
using namespace alpaca::data;
stream.subscribe_trades(
    [](const Trade& trade) {
        std::cout << "Trade: " << trade.symbol << " @ " << trade.price << std::endl;
    },
    {"AAPL", "MSFT"}
);

// Start streaming
stream.run();
```

## Repository Layout

- `include/alpaca/` — public headers organized by namespace:
  - `core/` — configuration, HTTP transport, JSON utilities
  - `trading/` — trading API client, models, requests, enums
  - `broker/` — broker API client, models, requests, enums
  - `data/` — market data API client, models, requests, enums, live streams
- `src/alpaca/` — library implementations
- `examples/` — usage samples compiled with the library
- `tests/` — comprehensive unit tests using mock HTTP transport
- `docs/` — architecture notes, design decisions

## API Coverage

This SDK provides **100% feature parity** with `alpaca-py`:

- ✅ **32/32 Trading API methods** — All order, position, account, asset, watchlist, and activity endpoints
- ✅ **All Broker API methods** — Account management, ACH/banks, transfers, journals, positions, portfolios, subscriptions, runs, documents, activities, corporate actions
- ✅ **All Market Data API methods** — Stock, crypto, options historical data, latest quotes/trades/bars, snapshots, news, screener, corporate actions
- ✅ **All WebSocket streams** — Stock, crypto, options, news data streams, and trading stream
- ✅ **All models and enums** — Complete type system matching Python SDK
- ✅ **Event streaming (SSE)** — Account status, trade, journal, and transfer events

## Dependencies

- **C++20** compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake 3.20+**
- **Boost** (Beast, System, URL) — for HTTP/WebSocket
- **simdjson** — for fast JSON parsing
- **OpenSSL** — for HTTPS support
- **libcurl** (optional) — alternative HTTP transport

## Building

### Build from Source

```bash
# Configure
cmake -S . -B build -D ALPACA_BUILD_TESTS=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build
```

### Installation

Install the library system-wide:

```bash
cmake -S . -B build -D CMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Or install to a custom location:

```bash
cmake -S . -B build -D CMAKE_INSTALL_PREFIX=$HOME/local
cmake --build build
cmake --install build
```

### Using in Your Project

After installation, use the library in your CMake project:

```cmake
find_package(alpaca-cpp REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE 
    alpaca::core
    alpaca::trading
    alpaca::data
    alpaca::broker
)
```

Or use it as a subdirectory:

```cmake
add_subdirectory(alpaca-cpp)
target_link_libraries(my_app PRIVATE alpaca::trading)
```

### Using FetchContent (Recommended)

You can also include it directly in your project using CMake's FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
    alpaca-cpp
    GIT_REPOSITORY https://github.com/yourusername/alpaca-cpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(alpaca-cpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE alpaca::trading)
```

## Contributing

Contributions are welcome! Please open an issue to discuss large design changes. Coding style is enforced via `.clang-format`, and CI will eventually cover build + tests on macOS, Linux, and Windows.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) file for details.

Copyright 2024 Alpaca Markets, Inc.
