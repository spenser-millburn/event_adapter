#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/adapters/keyboard_adapter.hpp>
#include <iostream>
#include <atomic>

using namespace event_adapter::adapters;

std::atomic<bool> should_exit{false};

void handle_key_press(event_adapter::EventPtr event) {
    if (auto key_event = std::dynamic_pointer_cast<event_adapter::TypedEvent<KeyPressEvent>>(event)) {
        const auto& data = key_event->data();
        std::cout << "Key pressed: '" << data.key << "'";
        if (data.ctrl) std::cout << " (Ctrl)";
        if (data.alt) std::cout << " (Alt)";
        if (data.shift) std::cout << " (Shift)";
        std::cout << std::endl;
        
        // Exit on Ctrl+C
        if (data.ctrl && (data.key == 'c' || data.key == 'C')) {
            std::cout << "Exiting..." << std::endl;
            should_exit = true;
        }
    }
}

void handle_special_key(event_adapter::EventPtr event) {
    if (auto special_event = std::dynamic_pointer_cast<event_adapter::TypedEvent<SpecialKeyEvent>>(event)) {
        const auto& data = special_event->data();
        std::cout << "Special key: ";
        switch (data.key) {
            case SpecialKeyEvent::Escape: std::cout << "Escape"; break;
            case SpecialKeyEvent::Tab: std::cout << "Tab"; break;
            case SpecialKeyEvent::Enter: std::cout << "Enter"; break;
            case SpecialKeyEvent::Backspace: std::cout << "Backspace"; break;
            case SpecialKeyEvent::Arrow_Up: std::cout << "Arrow Up"; break;
            case SpecialKeyEvent::Arrow_Down: std::cout << "Arrow Down"; break;
            case SpecialKeyEvent::Arrow_Left: std::cout << "Arrow Left"; break;
            case SpecialKeyEvent::Arrow_Right: std::cout << "Arrow Right"; break;
            default: std::cout << "Other"; break;
        }
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "Simple Keyboard Event Adapter Demo\n" << std::endl;
    std::cout << "Press keys to see events. Press Ctrl+C to exit.\n" << std::endl;
    
    // Create keyboard adapter
    auto keyboard = make_keyboard_adapter("Keyboard", KeyboardAdapter::Mode::Raw);
    keyboard->set_echo(false);
    
    // Subscribe to events
    keyboard->subscribe(handle_key_press);
    keyboard->subscribe(handle_special_key);
    
    // Connect (start listening)
    keyboard->connect();
    
    // Run until exit
    while (!should_exit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Disconnect
    keyboard->disconnect();
    
    return 0;
}