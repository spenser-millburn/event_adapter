j#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/event_filter.hpp>
#include <event_adapter/adapters/websocket_adapter.hpp>
#include <event_adapter/logging.hpp>
#include <boost/sml.hpp>
#include <iostream>

namespace sml = boost::sml;

// Logging macro wrappers with prefixes
#define LOG_EVENT(fmt, ...) EVENT_LOG_INFO("EVENT - " fmt, ##__VA_ARGS__)
#define LOG_ACTION(fmt, ...) EVENT_LOG_INFO("ACTION - " fmt, ##__VA_ARGS__)
#define LOG_GUARD(fmt, ...) EVENT_LOG_DEBUG("GUARD - " fmt, ##__VA_ARGS__)
#define LOG_TRANSITION(fmt, ...) EVENT_LOG_INFO("TRANSITION - " fmt, ##__VA_ARGS__)

// Trading system events
struct MarketOpen {};
struct MarketClose {};
struct PriceUpdate { double price; std::string symbol; };
struct OrderPlaced { std::string order_id; double price; int quantity; };
struct OrderFilled { std::string order_id; };
struct OrderCancelled { std::string order_id; };

// Trading system states
struct Idle {};
struct Trading {};
struct Processing {};

// Guards
auto is_valid_price = [](const PriceUpdate& e) { 
    bool valid = e.price > 0;
    LOG_GUARD("Price validation for {}: {} (price: {})", e.symbol, valid ? "valid" : "invalid", e.price);
    return valid;
};
auto is_market_hours = []() { 
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time);
    bool in_hours = tm.tm_hour >= 9 && tm.tm_hour < 16;
    LOG_GUARD("Market hours check: {} (current hour: {})", in_hours ? "open" : "closed", tm.tm_hour);
    return in_hours;
};

// Actions
auto log_event = [](const auto& event) {
    LOG_EVENT("Received: {}", typeid(event).name());
    std::cout << "Event: " << typeid(event).name() << std::endl;
};

auto place_order = [](const PriceUpdate& update) {
    LOG_ACTION("Placing order for {} at ${:.2f}", update.symbol, update.price);
    std::cout << "Placing order for " << update.symbol << " at $" << update.price << std::endl;
};

// Transition logging action
auto log_transition = [](auto src_state, auto dst_state) {
    LOG_TRANSITION("{} -> {}", 
        typeid(src_state).name(), 
        typeid(dst_state).name());
};

// State machine definition
struct TradingStateMachine {
    auto operator()() const {
        using namespace sml;
        
        return make_transition_table(
            // Initial state
            *state<Idle> + event<MarketOpen> / (log_event, []{LOG_TRANSITION("Idle -> Trading");}) = state<Trading>,
            
            // Trading state
            state<Trading> + event<PriceUpdate>[is_valid_price] / (log_event, place_order, []{LOG_TRANSITION("Trading -> Processing");}) = state<Processing>,
            state<Trading> + event<MarketClose> / (log_event, []{LOG_TRANSITION("Trading -> Idle");}) = state<Idle>,
            
            // Processing state
            state<Processing> + event<OrderPlaced> / (log_event, []{LOG_TRANSITION("Processing -> Trading");}) = state<Trading>,
            state<Processing> + event<OrderFilled> / (log_event, []{LOG_TRANSITION("Processing -> Trading");}) = state<Trading>,
            state<Processing> + event<OrderCancelled> / (log_event, []{LOG_TRANSITION("Processing -> Trading");}) = state<Trading>,
            
            // Error handling
            state<Trading> + event<PriceUpdate>[!is_valid_price] / log_event = state<Trading>
        );
    }
};

// Custom WebSocket adapter for market data
class MarketDataAdapter : public event_adapter::adapters::WebSocketAdapter {
public:
    MarketDataAdapter(const std::string& uri) 
        : WebSocketAdapter("MarketData", uri) {}
    
protected:
    void on_json_message(const nlohmann::json& message) override {
        if (message.contains("type")) {
            std::string type = message["type"];
            EVENT_LOG_DEBUG("MarketDataAdapter received message type: {}", type);
            
            if (type == "market_open") {
                LOG_EVENT("Market opened");
                emit<MarketOpen>();
            } else if (type == "market_close") {
                LOG_EVENT("Market closed");
                emit<MarketClose>();
            } else if (type == "price_update") {
                double price = message["price"].get<double>();
                std::string symbol = message["symbol"].get<std::string>();
                LOG_EVENT("Price update: {} = ${:.2f}", symbol, price);
                emit<PriceUpdate>(price, symbol);
            } else if (type == "order_placed") {
                std::string order_id = message["order_id"].get<std::string>();
                LOG_EVENT("Order placed: {}", order_id);
                emit<OrderPlaced>(
                    order_id,
                    message["price"].get<double>(),
                    message["quantity"].get<int>()
                );
            } else {
                EVENT_LOG_WARN("Unknown message type: {}", type);
            }
        } else {
            EVENT_LOG_WARN("Message missing 'type' field");
        }
    }
};

int main() {
    // Initialize logging
    event_adapter::Logger::initialize("trading_system", spdlog::level::debug, true, "trading_system.log");
    EVENT_LOG_INFO("=== Trading System Starting ===");
    
    // Create state machine
    sml::sm<TradingStateMachine> trading_sm;
    EVENT_LOG_DEBUG("State machine created");
    
    // Create event adapter system
    event_adapter::EventAdapterSystem<decltype(trading_sm)> system(trading_sm);
    
    // Configure event dispatcher
    auto& dispatcher = system.dispatcher();
    
    // Register event mappings
    dispatcher.template register_direct_mapping<MarketOpen>();
    dispatcher.template register_direct_mapping<MarketClose>();
    dispatcher.template register_direct_mapping<PriceUpdate>();
    dispatcher.template register_direct_mapping<OrderPlaced>();
    dispatcher.template register_direct_mapping<OrderFilled>();
    dispatcher.template register_direct_mapping<OrderCancelled>();
    
    // Create and configure market data adapter
    EVENT_LOG_INFO("Creating market data adapter");
    auto market_adapter = std::make_shared<MarketDataAdapter>("ws://localhost:8080/market");
    
    // Create handler that processes all events but filters prices
    auto event_handler = std::make_shared<event_adapter::FunctionalEventHandler>([&system](event_adapter::EventPtr event) {
        // Check if it's a PriceUpdate event
        if (auto price_update = std::dynamic_pointer_cast<PriceUpdate>(event)) {
            // Apply symbol filter to price updates only
            if (price_update->symbol == "AAPL" || price_update->symbol == "GOOGL") {
                LOG_GUARD("Price filter: {} accepted", price_update->symbol);
                system.dispatcher().dispatch(event);
            } else {
                LOG_GUARD("Price filter: {} rejected", price_update->symbol);
            }
        } else {
            // Pass through all other events (MarketOpen, MarketClose, OrderPlaced, etc.)
            system.dispatcher().dispatch(event);
        }
    });
    
    // Subscribe handler to market adapter
    market_adapter->subscribe(event_handler);
    
    // Add adapter to system
    system.add_adapter(market_adapter);
    
    // Start the system
    std::cout << "Starting trading system..." << std::endl;
    EVENT_LOG_INFO("Starting trading system with {} adapters", 1);
    system.start();
    
    // Simulate running for some time
    EVENT_LOG_INFO("Trading system running for 5 minutes");
    std::this_thread::sleep_for(std::chrono::minutes(5));
    
    // Stop the system
    std::cout << "Stopping trading system..." << std::endl;
    EVENT_LOG_INFO("Initiating shutdown");
    system.stop();
    
    EVENT_LOG_INFO("=== Trading System Stopped ===");
    event_adapter::Logger::shutdown();
    
    return 0;
}