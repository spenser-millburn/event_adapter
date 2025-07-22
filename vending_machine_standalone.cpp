#include <iostream>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <map>
#include <chrono>
#include <memory>
#include <functional>

// Simple event system for standalone demo
namespace event_system {
    
struct Event {
    virtual ~Event() = default;
    virtual std::string name() const = 0;
};

template<typename T>
struct TypedEvent : Event {
    T data;
    explicit TypedEvent(T d) : data(std::move(d)) {}
    std::string name() const override { return typeid(T).name(); }
};

using EventPtr = std::shared_ptr<Event>;
using EventHandler = std::function<void(EventPtr)>;

class EventDispatcher {
    std::vector<EventHandler> handlers;
public:
    void subscribe(EventHandler handler) {
        handlers.push_back(handler);
    }
    
    void dispatch(EventPtr event) {
        for (auto& handler : handlers) {
            handler(event);
        }
    }
};

} // namespace event_system

// Vending machine implementation
namespace vending_machine {

using namespace event_system;

// Events
struct CoinInserted { int cents; };
struct ProductSelected { char button; };
struct CancelPressed {};
struct MaintenanceMode {};

// States
enum class State {
    Idle,
    AcceptingCoins,
    Dispensing,
    Maintenance
};

const char* state_name(State s) {
    switch(s) {
        case State::Idle: return "IDLE";
        case State::AcceptingCoins: return "ACCEPTING COINS";
        case State::Dispensing: return "DISPENSING";
        case State::Maintenance: return "MAINTENANCE";
    }
    return "UNKNOWN";
}

// Vending machine logic
class VendingMachine {
    State current_state = State::Idle;
    int balance = 0;
    std::map<char, std::pair<std::string, int>> products = {
        {'1', {"Cola", 150}},      // $1.50
        {'2', {"Chips", 100}},     // $1.00
        {'3', {"Candy", 75}},      // $0.75
        {'4', {"Water", 125}},     // $1.25
        {'5', {"Coffee", 200}}     // $2.00
    };
    
    void log_state() {
        std::cout << "\n[STATE] " << state_name(current_state) 
                  << " | Balance: $" << std::fixed << std::setprecision(2) 
                  << balance / 100.0 << std::endl;
    }
    
    void log_event(const std::string& event) {
        std::cout << "[EVENT] " << event << std::endl;
    }
    
    void transition_to(State new_state) {
        if (new_state != current_state) {
            std::cout << "[TRANSITION] " << state_name(current_state) 
                      << " -> " << state_name(new_state) << std::endl;
            current_state = new_state;
            log_state();
        }
    }
    
public:
    void handle_event(EventPtr event) {
        if (auto coin_event = std::dynamic_pointer_cast<TypedEvent<CoinInserted>>(event)) {
            handle_coin_inserted(coin_event->data);
        }
        else if (auto product_event = std::dynamic_pointer_cast<TypedEvent<ProductSelected>>(event)) {
            handle_product_selected(product_event->data);
        }
        else if (std::dynamic_pointer_cast<TypedEvent<CancelPressed>>(event)) {
            handle_cancel_pressed();
        }
        else if (std::dynamic_pointer_cast<TypedEvent<MaintenanceMode>>(event)) {
            handle_maintenance_mode();
        }
    }
    
private:
    void handle_coin_inserted(const CoinInserted& e) {
        if (current_state == State::Idle || current_state == State::AcceptingCoins) {
            balance += e.cents;
            log_event("Coin inserted: " + std::to_string(e.cents) + " cents");
            transition_to(State::AcceptingCoins);
        }
    }
    
    void handle_product_selected(const ProductSelected& e) {
        if (current_state == State::AcceptingCoins) {
            auto it = products.find(e.button);
            if (it != products.end()) {
                auto& [name, price] = it->second;
                log_event("Product selected: " + name + " ($" + 
                         std::to_string(price / 100.0) + ")");
                
                if (balance >= price) {
                    transition_to(State::Dispensing);
                    std::cout << "[ACTION] Dispensing: " << name << std::endl;
                    balance -= price;
                    
                    // Return change
                    if (balance > 0) {
                        std::cout << "[ACTION] Returning change: $" 
                                  << std::to_string(balance / 100.0) << std::endl;
                        balance = 0;
                    }
                    
                    // Simulate dispensing delay
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    transition_to(State::Idle);
                } else {
                    log_event("Insufficient funds for " + name + 
                             ". Need $" + std::to_string((price - balance) / 100.0) + " more");
                }
            } else {
                log_event("Invalid product selection: " + std::string(1, e.button));
            }
        }
    }
    
    void handle_cancel_pressed() {
        if (current_state == State::AcceptingCoins) {
            if (balance > 0) {
                log_event("Refunding all coins: $" + std::to_string(balance / 100.0));
                balance = 0;
            }
            transition_to(State::Idle);
        } else if (current_state == State::Maintenance) {
            transition_to(State::Idle);
        }
    }
    
    void handle_maintenance_mode() {
        if (current_state == State::Idle) {
            transition_to(State::Maintenance);
        }
    }
    
public:
    void start() {
        log_state();
    }
};

// Keyboard input handler
class KeyboardInput {
    std::atomic<bool> should_run{false};
    std::thread input_thread;
    termios old_term{};
    EventDispatcher& dispatcher;
    
    void show_menu() {
        std::cout << "\n=== VENDING MACHINE ===" << std::endl;
        std::cout << "Products:" << std::endl;
        std::cout << "  1 - Cola   ($1.50)" << std::endl;
        std::cout << "  2 - Chips  ($1.00)" << std::endl;
        std::cout << "  3 - Candy  ($0.75)" << std::endl;
        std::cout << "  4 - Water  ($1.25)" << std::endl;
        std::cout << "  5 - Coffee ($2.00)" << std::endl;
        std::cout << "\nCoins:" << std::endl;
        std::cout << "  q - Quarter (25¢)" << std::endl;
        std::cout << "  d - Dime (10¢)" << std::endl;
        std::cout << "  n - Nickel (5¢)" << std::endl;
        std::cout << "  o - Dollar ($1.00)" << std::endl;
        std::cout << "\nOther:" << std::endl;
        std::cout << "  c - Cancel/Refund" << std::endl;
        std::cout << "  m - Maintenance Mode" << std::endl;
        std::cout << "  h - Show this menu" << std::endl;
        std::cout << "  x - Exit" << std::endl;
        std::cout << "\nPress a key..." << std::endl;
    }
    
    void process_input() {
        while (should_run) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            
            timeval timeout = {0, 100000}; // 100ms timeout
            if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout) > 0) {
                char ch;
                if (read(STDIN_FILENO, &ch, 1) == 1) {
                    handle_key(ch);
                }
            }
        }
    }
    
    void handle_key(char ch) {
        switch (ch) {
            // Coins
            case 'q':
            case 'Q':
                dispatcher.dispatch(std::make_shared<TypedEvent<CoinInserted>>(CoinInserted{25}));
                break;
            case 'd':
            case 'D':
                dispatcher.dispatch(std::make_shared<TypedEvent<CoinInserted>>(CoinInserted{10}));
                break;
            case 'n':
            case 'N':
                dispatcher.dispatch(std::make_shared<TypedEvent<CoinInserted>>(CoinInserted{5}));
                break;
            case 'o':
            case 'O':
                dispatcher.dispatch(std::make_shared<TypedEvent<CoinInserted>>(CoinInserted{100}));
                break;
                
            // Products
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
                dispatcher.dispatch(std::make_shared<TypedEvent<ProductSelected>>(ProductSelected{ch}));
                break;
                
            // Control
            case 'c':
            case 'C':
                dispatcher.dispatch(std::make_shared<TypedEvent<CancelPressed>>(CancelPressed{}));
                break;
            case 'm':
            case 'M':
                dispatcher.dispatch(std::make_shared<TypedEvent<MaintenanceMode>>(MaintenanceMode{}));
                break;
            case 'h':
            case 'H':
                show_menu();
                break;
            case 'x':
            case 'X':
                should_run = false;
                break;
        }
    }
    
public:
    explicit KeyboardInput(EventDispatcher& d) : dispatcher(d) {}
    
    ~KeyboardInput() {
        stop();
    }
    
    void start() {
        should_run = true;
        
        // Set terminal to raw mode
        tcgetattr(STDIN_FILENO, &old_term);
        termios new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
        
        // Start input thread
        input_thread = std::thread([this]() {
            process_input();
        });
        
        show_menu();
    }
    
    void stop() {
        should_run = false;
        
        if (input_thread.joinable()) {
            input_thread.join();
        }
        
        // Restore terminal
        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    }
    
    bool is_running() const {
        return should_run.load();
    }
};

} // namespace vending_machine

int main() {
    std::cout << "Vending Machine Demo - Keyboard Input with State Logging\n" << std::endl;
    
    // Create components
    event_system::EventDispatcher dispatcher;
    vending_machine::VendingMachine vm;
    vending_machine::KeyboardInput keyboard(dispatcher);
    
    // Connect vending machine to dispatcher
    dispatcher.subscribe([&vm](event_system::EventPtr event) {
        vm.handle_event(event);
    });
    
    // Start vending machine
    vm.start();
    
    // Start keyboard input
    keyboard.start();
    
    // Run until exit
    while (keyboard.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n\nVending machine shut down." << std::endl;
    
    return 0;
}