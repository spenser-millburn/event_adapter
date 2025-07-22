# Event Adapter System

A modern C++ event adapter system for converting data updates from various sources into events for boost::sml state machines.

## Features

- **Type-safe event system** with compile-time event type checking
- **Multiple data source adapters**: WebSocket, HTTP, File system
- **Thread-safe event dispatching** with configurable event queue
- **Event filtering and transformation pipeline**
- **Seamless boost::sml integration**
- **Header-only library** for easy integration

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Data Sources   │     │  Event Adapter  │     │ State Machine   │
├─────────────────┤     ├─────────────────┤     ├─────────────────┤
│ WebSocket       │────▶│ Event Creation  │────▶│ boost::sml      │
│ HTTP API        │     │ Filtering       │     │ State Logic     │
│ File System     │     │ Transformation  │     │ Actions         │
│ Custom Sources  │     │ Dispatching     │     │ Guards          │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Quick Start

```cpp
#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <boost/sml.hpp>

// Define your events
struct StartEvent {};
struct DataEvent { int value; };

// Define your state machine
struct MyStateMachine {
    auto operator()() const {
        using namespace boost::sml;
        return make_transition_table(
            *"idle"_s + event<StartEvent> = "active"_s,
            "active"_s + event<DataEvent> = "processing"_s
        );
    }
};

// Use the event adapter system
int main() {
    boost::sml::sm<MyStateMachine> sm;
    event_adapter::EventAdapterSystem<decltype(sm)> system(sm);
    
    // Register event mappings
    system.dispatcher().register_direct_mapping<StartEvent>();
    system.dispatcher().register_direct_mapping<DataEvent>();
    
    // Add data source adapters
    auto adapter = std::make_shared<MyCustomAdapter>("MySource");
    system.add_adapter(adapter);
    
    // Start the system
    system.start();
    
    return 0;
}
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Requirements

- C++17 or later
- Boost (for boost::sml)
- Optional: websocketpp, libcurl, nlohmann/json

## Running Examples

### Trading System Example

The trading system example demonstrates a WebSocket-based market data adapter with event filtering and a boost::sml state machine for order processing.

To run the trading example, you'll need a WebSocket server running on `ws://localhost:8080/market` that sends JSON messages in this format:

```json
{
  "type": "market_open" | "market_close" | "price_update" | "order_placed",
  "symbol": "AAPL",      // for price_update
  "price": 150.25,       // for price_update and order_placed
  "order_id": "12345",   // for order_placed
  "quantity": 100        // for order_placed
}
```

You can simulate a WebSocket server using tools like:
- `websocat` - `websocat -s 8080`
- Node.js with `ws` library
- Python with `websockets` library

Example Python WebSocket server:
```python
import asyncio
import websockets
import json
import random

async def market_simulator(websocket, path):
    await websocket.send(json.dumps({"type": "market_open"}))
    
    symbols = ["AAPL", "GOOGL", "MSFT"]
    while True:
        # Send price updates
        await websocket.send(json.dumps({
            "type": "price_update",
            "symbol": random.choice(symbols),
            "price": round(random.uniform(100, 200), 2)
        }))
        await asyncio.sleep(1)

start_server = websockets.serve(market_simulator, "localhost", 8080)
asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
```

Run the trading example:
```bash
./build/examples/trading_system_example
```

### Simple Example

Demonstrates basic event generation with a tick generator:
```bash
./build/examples/simple_example
```

### Keyboard Examples

1. **Text Editor Example** - A simple vi-like editor with command and insert modes:
```bash
./build/examples/keyboard_example
```
- Press 'i' to enter insert mode
- Press ESC to return to command mode
- Press 'q' in command mode to quit

2. **Simple Keyboard Monitor** - Shows all keyboard events:
```bash
./build/examples/keyboard_simple
```
- Press any key to see the event details
- Press Ctrl+C to exit

## License

MIT License