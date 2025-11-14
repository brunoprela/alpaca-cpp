# Architecture Overview

## Goals
- Feature parity with `alpaca-py` clients (Trading, Market Data, Broker).
- Modern C++20 API with strong typing and minimal overhead.
- Pluggable transports and codecs so users can swap networking/JSON stacks.

## Layers
1. **Core** — environment/config management, HTTP abstractions, retry helpers, JSON adapter interfaces.
2. **Domain Clients** — `alpaca::trading`, `alpaca::broker`, `alpaca::data` namespaces that expose request/response types modeled after the REST APIs.
3. **Streaming** — WebSocket/SSE dispatcher with automatic reconnect/backoff.
4. **Integration** — Examples, CLI helpers, bindings (future).

## Dependencies
- C++20 STL by default.
- Optional Boost.Asio/Beast for async networking (under evaluation).
- JSON backend to be selected (likely `nlohmann::json` behind the `JsonCodec` interface).

## Next Steps
- Finalize decision on default HTTP+JSON stack.
- Flesh out `alpaca::trading` request models and wire them to a concrete transport.
- Bring in Catch2 for expressive tests once API surface stabilizes.

