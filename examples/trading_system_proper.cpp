#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/event_filter.hpp>
#include <event_adapter/adapters/websocket_adapter.hpp>
#include <event_adapter/logging.hpp>
#include <boost/sml.hpp>
#include <iostream>
#include <unordered_map>

namespace sml = boost::sml;

// Logging macro wrappers with prefixes
#define LOG_EVENT(fmt, ...) EVENT_LOG_INFO("EVENT - " fmt, ##__VA_ARGS__)
#define LOG_ACTION(fmt, ...) EVENT_LOG_INFO("ACTION - " fmt, ##__VA_ARGS__)
#define LOG_GUARD(fmt, ...) EVENT_LOG_DEBUG("GUARD - " fmt, ##__VA_ARGS__)
#define LOG_TRANSITION(fmt, ...) EVENT_LOG_INFO("TRANSITION - " fmt, ##__VA_ARGS__)

// Event type enum for switch statement
enum class EventType {
    MARKET_OPEN,
    MARKET_CLOSE,
    PRICE_UPDATE,
    ORDER_PLACED,
    ORDER_FILLED,
    ORDER_CANCELLED,
    UNKNOWN
};

// Helper function to convert string to enum
EventType stringToEventType(const std::string& type) {
    static const std::unordered_map<std::string, EventType> typeMap = {
        {"market_open", EventType::MARKET_OPEN},
        {"market_close", EventType::MARKET_CLOSE},
        {"price_update", EventType::PRICE_UPDATE},
        {"order_placed", EventType::ORDER_PLACED},
        {"order_filled", EventType::ORDER_FILLED},
        {"order_cancelled", EventType::ORDER_CANCELLED}
    };
    
    auto it = typeMap.find(type);
    return (it != typeMap.end()) ? it->second : EventType::UNKNOWN;
}

// Raw data event from WebSocket
struct MarketDataEvent {
    nlohmann::json data;
    
    MarketDataEvent(const nlohmann::json& d) : data(d) {}
};

// Trading system events
struct MarketOpen {};
struct MarketClose {};
struct PriceUpdate { 
    double price; 
    std::string symbol; 
};
struct OrderPlaced { 
    std::string order_id; 
    double price; 
    int quantity; 
};
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

// State machine definition
struct TradingStateMachine {
    auto operator()() const {
        using namespace sml;
        
        return make_transition_table(
            //+---------------+------------------+------------------+-------------------------------------------------------+----------------+
            //| Source State  | Event            | Guard            | Action                                                | Dest State     |
            //+---------------+------------------+------------------+-------------------------------------------------------+----------------+
            
            // Initial state transitions
           *state<Idle>      + event<MarketOpen>                    / (log_event, 
                                                                        []{LOG_TRANSITION("Idle -> Trading");})              = state<Trading>,
            
            // Trading state transitions
            state<Trading>   + event<PriceUpdate> [is_valid_price]  / (log_event, 
                                                                        place_order, 
                                                                        []{LOG_TRANSITION("Trading -> Processing");})        = state<Processing>,
            
            state<Trading>   + event<MarketClose>                   / (log_event, 
                                                                        []{LOG_TRANSITION("Trading -> Idle");})              = state<Idle>,
            
            // Processing state transitions
            state<Processing> + event<OrderPlaced>                   / (log_event, 
                                                                        []{LOG_TRANSITION("Processing -> Trading");})        = state<Trading>,
            
            state<Processing> + event<OrderFilled>                   / (log_event, 
                                                                        []{LOG_TRANSITION("Processing -> Trading");})        = state<Trading>,
            
            state<Processing> + event<OrderCancelled>                / (log_event, 
                                                                        []{LOG_TRANSITION("Processing -> Trading");})        = state<Trading>,
            
            // Error handling transitions
            state<Trading>   + event<PriceUpdate> [!is_valid_price] / log_event                                             = state<Trading>
            
            //+---------------+------------------+------------------+-------------------------------------------------------+----------------+
        );
    }
};

// PROPER WebSocket adapter - emits raw data events
class MarketDataAdapter : public event_adapter::adapters::WebSocketAdapter {
public:
    MarketDataAdapter(const std::string& uri) 
        : WebSocketAdapter("MarketData", uri) {}
    
protected:
    void on_json_message(const nlohmann::json& message) override {
        // Emit raw JSON data event - let the system handle transformation
        LOG_EVENT("Raw market data received");
        emit<MarketDataEvent>(message);
    }
};

int main() {
    // Initialize logging
    event_adapter::Logger::initialize("trading_system_proper", spdlog::level::debug, true, "trading_system_proper.log");
    EVENT_LOG_INFO("=== Trading System Starting (Proper Event Adapter) ===");
    
    // Create state machine
    sml::sm<TradingStateMachine> trading_sm;
    EVENT_LOG_DEBUG("State machine created");
    
    // Create event adapter system
    event_adapter::EventAdapterSystem<decltype(trading_sm)> system(trading_sm);
    
    // Configure event dispatcher
    auto& dispatcher = system.dispatcher();
    
    // Register event processor for MarketDataEvent that transforms and dispatches appropriate events
    dispatcher.template register_event_processor<MarketDataEvent>(
        [](const MarketDataEvent& event, decltype(trading_sm)& sm) {
            const auto& data = event.data;
            if (!data.contains("type")) {
                EVENT_LOG_WARN("Market data missing 'type' field");
                return;
            }
            
            std::string type = data["type"];
            EVENT_LOG_DEBUG("Processing market data type: {}", type);
            
            // Convert string to enum for switch statement
            switch (stringToEventType(type)) {
                case EventType::MARKET_OPEN:
                    LOG_EVENT("Processing MarketOpen event");
                    sm.process_event(MarketOpen{});
                    break;
                    
                case EventType::MARKET_CLOSE:
                    LOG_EVENT("Processing MarketClose event");
                    sm.process_event(MarketClose{});
                    break;
                    
                case EventType::PRICE_UPDATE: {
                    double price = data["price"].get<double>();
                    std::string symbol = data["symbol"].get<std::string>();
                    
                    // Apply filtering at processing level
                    if (symbol == "AAPL" || symbol == "GOOGL") {
                        LOG_EVENT("Processing PriceUpdate event: {} @ ${:.2f}", symbol, price);
                        sm.process_event(PriceUpdate{price, symbol});
                    } else {
                        LOG_GUARD("Filtering out price update for: {}", symbol);
                    }
                    break;
                }
                
                case EventType::ORDER_PLACED:
                    LOG_EVENT("Processing OrderPlaced event");
                    sm.process_event(OrderPlaced{
                        data["order_id"].get<std::string>(),
                        data["price"].get<double>(),
                        data["quantity"].get<int>()
                    });
                    break;
                    
                case EventType::ORDER_FILLED:
                    LOG_EVENT("Processing OrderFilled event");
                    sm.process_event(OrderFilled{
                        data["order_id"].get<std::string>()
                    });
                    break;
                    
                case EventType::ORDER_CANCELLED:
                    LOG_EVENT("Processing OrderCancelled event");
                    sm.process_event(OrderCancelled{
                        data["order_id"].get<std::string>()
                    });
                    break;
                    
                case EventType::UNKNOWN:
                default:
                    EVENT_LOG_WARN("Unknown market data type: {}", type);
                    break;
            }
        }
    );
    
    // Create and add market data adapter
    EVENT_LOG_INFO("Creating market data adapter");
    auto market_adapter = std::make_shared<MarketDataAdapter>("ws://localhost:8080/market");
    
    // Add adapter to system - the system handles all event routing
    system.add_adapter(market_adapter);
    
    // Start the system
    std::cout << "Starting trading system..." << std::endl;
    EVENT_LOG_INFO("Starting trading system with proper event adapter pattern");
    system.start();
    
    // Run for 5 minutes
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