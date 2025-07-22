#pragma once

#include "../data_source_adapter.hpp"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <thread>
#include <iostream>
#include <set>

namespace event_adapter::adapters {

// Keyboard event types
struct KeyPressEvent {
    char key;
    bool ctrl;
    bool alt;
    bool shift;
};

struct KeyReleaseEvent {
    char key;
};

struct SpecialKeyEvent {
    enum Key {
        Arrow_Up,
        Arrow_Down,
        Arrow_Left,
        Arrow_Right,
        Home,
        End,
        Page_Up,
        Page_Down,
        Insert,
        Delete,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Escape,
        Tab,
        Backspace,
        Enter
    };
    Key key;
};

class KeyboardAdapter : public DataSourceAdapter {
public:
    enum class Mode {
        Raw,      // Raw input mode - all keys
        Line,     // Line buffered mode
        Filtered  // Only specific keys
    };
    
    KeyboardAdapter(std::string name = "Keyboard", Mode mode = Mode::Raw) 
        : DataSourceAdapter(std::move(name))
        , mode_(mode)
        , should_run_(false)
        , echo_enabled_(false) {}
    
    ~KeyboardAdapter() {
        disconnect();
    }
    
    void connect() override {
        set_state(State::Connecting);
        
        // Save current terminal settings
        if (tcgetattr(STDIN_FILENO, &old_termios_) != 0) {
            set_state(State::Error);
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), "Failed to get terminal attributes");
            return;
        }
        
        // Configure terminal for raw input
        termios new_termios = old_termios_;
        
        if (mode_ == Mode::Raw) {
            // Raw mode - no processing
            new_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            new_termios.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR);
            new_termios.c_oflag &= ~(OPOST);
            new_termios.c_cc[VMIN] = 0;
            new_termios.c_cc[VTIME] = 0;
        } else if (mode_ == Mode::Line) {
            // Line mode - canonical but no echo
            new_termios.c_lflag &= ~(ECHO | ECHOE);
        } else {
            // Filtered mode - similar to raw but with some processing
            new_termios.c_lflag &= ~(ICANON | ECHO | ECHOE);
            new_termios.c_cc[VMIN] = 0;
            new_termios.c_cc[VTIME] = 0;
        }
        
        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_termios) != 0) {
            set_state(State::Error);
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), "Failed to set terminal attributes");
            return;
        }
        
        // Make stdin non-blocking
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        
        should_run_ = true;
        set_state(State::Connected);
        
        // Start input processing thread
        input_thread_ = std::thread([this]() {
            process_input();
        });
        
        emit<ConnectionEvent>(ConnectionEvent::Type::Connected, name(), "Keyboard input active");
    }
    
    void disconnect() override {
        if (state() != State::Connected) {
            return;
        }
        
        set_state(State::Disconnecting);
        should_run_ = false;
        
        if (input_thread_.joinable()) {
            input_thread_.join();
        }
        
        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &old_termios_);
        
        // Restore blocking mode
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
        
        set_state(State::Disconnected);
        emit<ConnectionEvent>(ConnectionEvent::Type::Disconnected, name(), "Keyboard input inactive");
    }
    
    bool is_connected() const override {
        return should_run_.load();
    }
    
    void set_echo(bool enabled) {
        echo_enabled_ = enabled;
    }
    
    void add_key_filter(char key) {
        filtered_keys_.insert(key);
    }
    
    void clear_key_filters() {
        filtered_keys_.clear();
    }
    
private:
    void process_input() {
        char buffer[16];
        
        while (should_run_) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms
            
            if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout) > 0) {
                ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
                
                if (bytes_read > 0) {
                    process_buffer(buffer, bytes_read);
                }
            }
        }
    }
    
    void process_buffer(const char* buffer, ssize_t length) {
        for (ssize_t i = 0; i < length; ++i) {
            char ch = buffer[i];
            
            // Check for escape sequences
            if (ch == 27 && i + 2 < length && buffer[i + 1] == '[') {
                // ANSI escape sequence
                char seq = buffer[i + 2];
                handle_escape_sequence(seq);
                i += 2; // Skip the escape sequence
                continue;
            }
            
            // Handle special characters
            if (ch == 27) {
                emit<SpecialKeyEvent>(SpecialKeyEvent::Escape);
            } else if (ch == '\t') {
                emit<SpecialKeyEvent>(SpecialKeyEvent::Tab);
            } else if (ch == '\n' || ch == '\r') {
                emit<SpecialKeyEvent>(SpecialKeyEvent::Enter);
                if (echo_enabled_) std::cout << std::endl;
            } else if (ch == 127 || ch == 8) {
                emit<SpecialKeyEvent>(SpecialKeyEvent::Backspace);
                if (echo_enabled_) std::cout << "\b \b" << std::flush;
            } else if (ch >= 1 && ch <= 26) {
                // Ctrl+A through Ctrl+Z
                emit<KeyPressEvent>(static_cast<char>('a' + ch - 1), true, false, false);
            } else {
                // Regular character
                if (mode_ == Mode::Filtered && !filtered_keys_.empty()) {
                    if (filtered_keys_.find(ch) == filtered_keys_.end()) {
                        continue; // Skip non-filtered keys
                    }
                }
                
                emit<KeyPressEvent>(ch, false, false, false);
                if (echo_enabled_) std::cout << ch << std::flush;
            }
        }
    }
    
    void handle_escape_sequence(char seq) {
        switch (seq) {
            case 'A':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Arrow_Up);
                break;
            case 'B':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Arrow_Down);
                break;
            case 'C':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Arrow_Right);
                break;
            case 'D':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Arrow_Left);
                break;
            case 'H':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Home);
                break;
            case 'F':
                emit<SpecialKeyEvent>(SpecialKeyEvent::End);
                break;
            case '2':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Insert);
                break;
            case '3':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Delete);
                break;
            case '5':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Page_Up);
                break;
            case '6':
                emit<SpecialKeyEvent>(SpecialKeyEvent::Page_Down);
                break;
        }
    }
    
    Mode mode_;
    std::atomic<bool> should_run_;
    std::thread input_thread_;
    termios old_termios_;
    bool echo_enabled_;
    std::set<char> filtered_keys_;
};

// Convenience factory functions
inline std::shared_ptr<KeyboardAdapter> make_keyboard_adapter(
    const std::string& name = "Keyboard",
    KeyboardAdapter::Mode mode = KeyboardAdapter::Mode::Raw) {
    return std::make_shared<KeyboardAdapter>(name, mode);
}

} // namespace event_adapter::adapters