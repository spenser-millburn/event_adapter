#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/adapters/keyboard_adapter.hpp>
#include <boost/sml.hpp>
#include <iostream>
#include <iomanip>

namespace sml = boost::sml;
using namespace event_adapter::adapters;

// Text editor states
struct EditMode {};
struct CommandMode {};
struct ExitState {};

// Text editor data
struct EditorData {
    std::string buffer;
    size_t cursor_pos = 0;
    
    void insert_char(char ch) {
        buffer.insert(cursor_pos, 1, ch);
        cursor_pos++;
    }
    
    void delete_char() {
        if (cursor_pos > 0 && cursor_pos <= buffer.size()) {
            buffer.erase(cursor_pos - 1, 1);
            cursor_pos--;
        }
    }
    
    void move_left() {
        if (cursor_pos > 0) cursor_pos--;
    }
    
    void move_right() {
        if (cursor_pos < buffer.size()) cursor_pos++;
    }
    
    void display() const {
        std::cout << "\r[" << (cursor_pos == 0 ? "|" : "") 
                  << buffer.substr(0, cursor_pos)
                  << "|" 
                  << buffer.substr(cursor_pos) 
                  << "]" << std::string(20, ' ') << std::flush;
    }
};

// Guards
auto is_escape_key = [](const SpecialKeyEvent& e) { 
    return e.key == SpecialKeyEvent::Escape; 
};

auto is_enter_key = [](const SpecialKeyEvent& e) { 
    return e.key == SpecialKeyEvent::Enter; 
};

auto is_backspace_key = [](const SpecialKeyEvent& e) { 
    return e.key == SpecialKeyEvent::Backspace; 
};

auto is_arrow_left_key = [](const SpecialKeyEvent& e) { 
    return e.key == SpecialKeyEvent::Arrow_Left; 
};

auto is_arrow_right_key = [](const SpecialKeyEvent& e) { 
    return e.key == SpecialKeyEvent::Arrow_Right; 
};

auto is_quit_command = [](const KeyPressEvent& e) { 
    return e.key == 'q' || e.key == 'Q'; 
};

auto is_insert_command = [](const KeyPressEvent& e) { 
    return e.key == 'i' || e.key == 'I'; 
};

auto is_printable = [](const KeyPressEvent& e) {
    return e.key >= 32 && e.key <= 126 && !e.ctrl;
};

// Actions
auto insert_character = [](const KeyPressEvent& e, EditorData& data) {
    data.insert_char(e.key);
    data.display();
};

auto handle_backspace = [](EditorData& data) {
    data.delete_char();
    data.display();
};

auto handle_arrow_left = [](EditorData& data) {
    data.move_left();
    data.display();
};

auto handle_arrow_right = [](EditorData& data) {
    data.move_right();
    data.display();
};

auto show_command_mode = []() {
    std::cout << "\n[COMMAND MODE] Press 'i' to insert, 'q' to quit" << std::endl;
};

auto show_edit_mode = []() {
    std::cout << "\n[EDIT MODE] Press ESC to return to command mode" << std::endl;
};

auto save_and_exit = [](EditorData& data) {
    std::cout << "\nFinal text: \"" << data.buffer << "\"" << std::endl;
    std::cout << "Exiting..." << std::endl;
};

// Simple text editor state machine
struct TextEditorStateMachine {
    auto operator()() const {
        using namespace sml;
        
        return make_transition_table(
            // Initial state
            *state<CommandMode> / show_command_mode,
            
            // Command mode transitions
            state<CommandMode> + event<KeyPressEvent>[is_insert_command] = state<EditMode>,
            state<CommandMode> + event<KeyPressEvent>[is_quit_command] / save_and_exit = state<ExitState>,
            
            // Edit mode transitions
            state<EditMode> / show_edit_mode,
            state<EditMode> + event<KeyPressEvent>[is_printable] / insert_character = state<EditMode>,
            state<EditMode> + event<SpecialKeyEvent>[is_escape_key] = state<CommandMode>,
            state<EditMode> + event<SpecialKeyEvent>[is_enter_key] / insert_character = state<EditMode>,
            state<EditMode> + event<SpecialKeyEvent>[is_backspace_key] / handle_backspace = state<EditMode>,
            state<EditMode> + event<SpecialKeyEvent>[is_arrow_left_key] / handle_arrow_left = state<EditMode>,
            state<EditMode> + event<SpecialKeyEvent>[is_arrow_right_key] / handle_arrow_right = state<EditMode>
        );
    }
};

int main() {
    std::cout << "Simple Text Editor Example\n" << std::endl;
    std::cout << "This is a vi-like editor with command and insert modes.\n" << std::endl;
    
    // Create editor data
    EditorData editor_data;
    
    // Create state machine with data
    sml::sm<TextEditorStateMachine> sm{editor_data};
    
    // Create event system
    event_adapter::EventAdapterSystem<decltype(sm)> system(sm);
    
    // Configure dispatcher
    auto& dispatcher = system.dispatcher();
    dispatcher.register_direct_mapping<KeyPressEvent>();
    dispatcher.register_direct_mapping<SpecialKeyEvent>();
    
    // Create keyboard adapter
    auto keyboard = make_keyboard_adapter("Keyboard", KeyboardAdapter::Mode::Raw);
    keyboard->set_echo(false); // We'll handle display ourselves
    
    // Add adapter to system
    system.add_adapter(keyboard);
    
    // Start the system
    system.start();
    
    // Run until exit state
    while (!sm.is(sml::state<ExitState>)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop the system
    system.stop();
    
    return 0;
}