#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/data_source_adapter.hpp>
#include <boost/sml.hpp>
#include <iostream>
#include <random>

namespace sml = boost::sml;

// Simple events
struct StartEvent {};
struct StopEvent {};
struct TickEvent { int count; };

// Simple state machine
struct SimpleStateMachine {
    auto operator()() const {
        using namespace sml;
        
        return make_transition_table(
            *"idle"_s + event<StartEvent> = "running"_s,
            "running"_s + event<TickEvent> = "running"_s,
            "running"_s + event<StopEvent> = "idle"_s
        );
    }
};

// Simple adapter that generates tick events
class TickGenerator : public event_adapter::PollingDataSourceAdapter {
public:
    TickGenerator() 
        : PollingDataSourceAdapter("TickGenerator", std::chrono::milliseconds(1000))
        , counter_(0) {}
    
protected:
    void poll() override {
        emit<TickEvent>(++counter_);
        std::cout << "Tick #" << counter_ << std::endl;
        
        // Stop after 5 ticks
        if (counter_ >= 5) {
            emit<StopEvent>();
        }
    }
    
private:
    int counter_;
};

int main() {
    std::cout << "Simple Event Adapter Example\n" << std::endl;
    
    // Create state machine
    sml::sm<SimpleStateMachine> sm;
    
    // Create event system
    event_adapter::EventAdapterSystem<decltype(sm)> system(sm);
    
    // Configure dispatcher
    auto& dispatcher = system.dispatcher();
    dispatcher.register_direct_mapping<StartEvent>();
    dispatcher.register_direct_mapping<StopEvent>();
    dispatcher.register_direct_mapping<TickEvent>();
    
    // Add tick generator
    auto ticker = std::make_shared<TickGenerator>();
    system.add_adapter(ticker);
    
    // Start the system
    system.start();
    
    // Create a separate adapter to emit the start event
    class StartEventAdapter : public event_adapter::DataSourceAdapter {
    public:
        StartEventAdapter() : DataSourceAdapter("StartEvent") {}
        void connect() override { 
            set_state(State::Connected);
            emit<StartEvent>();
        }
        void disconnect() override { set_state(State::Disconnected); }
        bool is_connected() const override { return state() == State::Connected; }
    };
    
    auto start_adapter = std::make_shared<StartEventAdapter>();
    system.add_adapter(start_adapter);
    
    // Run for a while
    std::this_thread::sleep_for(std::chrono::seconds(7));
    
    // Stop the system
    system.stop();
    
    std::cout << "\nExample completed!" << std::endl;
    
    return 0;
}