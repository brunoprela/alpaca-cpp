# alpaca-cpp

Modern C++20 SDK for the Alpaca Markets APIs. The goal is to offer the same breadth of functionality as `alpaca-py`—trading, broker, and market data—while providing zero-cost abstractions, compile-time safety, and first-class async support for native applications.

## Project Status

🚧 **Work in progress.** This repository currently contains the initial scaffolding (core config/transport primitives, build system, docs). Interfaces are intentionally simple while we design the higher-level clients.

## Features (planned)

- Typed request/response models for Trading, Broker, Market Data REST APIs.
- Unified configuration layer with environment switching (live, paper, sandbox).
- Swappable HTTP transport (libcurl by default, ASIO-compatible option later).
- Streaming layer for WebSocket + SSE feeds built on Boost.Asio/Beast.
- Coroutine-friendly async APIs plus blocking facades.
- Strong error model with Alpaca error codes and retry helpers.

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

Use `examples/place_order.cpp` and `examples/get_account.cpp` as starting points for experiments. The examples currently target the paper trading API and will evolve as real endpoints are implemented.

## Repository Layout

- `include/` — public headers (`alpaca/core` ready today; more namespaces coming soon).
- `src/` — library implementations (`core`, `trading` currently available).
- `examples/` — usage samples compiled with the library.
- `tests/` — lightweight unit tests (Catch2 or GoogleTest can be hooked up later).
- `docs/` — architecture notes, design decisions.

## Contributing

Contributions are welcome! Please open an issue to discuss large design changes. Coding style is enforced via `.clang-format`, and CI will eventually cover build + tests on macOS, Linux, and Windows.
