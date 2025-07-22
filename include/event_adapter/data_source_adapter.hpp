#pragma once

#include "event.hpp"
#include "logging.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <algorithm>

namespace event_adapter {

class EventHandler {
public:
    using Callback = std::function<void(EventPtr)>;
    
    virtual ~EventHandler() = default;
    virtual void handle(EventPtr event) = 0;
};

class FunctionalEventHandler : public EventHandler {
public:
    explicit FunctionalEventHandler(Callback callback) : callback_(std::move(callback)) {}
    
    void handle(EventPtr event) override {
        if (callback_) {
            callback_(event);
        }
    }
    
private:
    Callback callback_;
};

class DataSourceAdapter {
public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    
    DataSourceAdapter(std::string name) : name_(std::move(name)), state_(State::Disconnected) {
        EVENT_LOG_DEBUG("DataSourceAdapter '{}' created", name_);
    }
    virtual ~DataSourceAdapter() = default;
    
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    const std::string& name() const { return name_; }
    State state() const { return state_.load(); }
    
    void subscribe(std::shared_ptr<EventHandler> handler) {
        handlers_.push_back(handler);
        EVENT_LOG_DEBUG("Handler subscribed to adapter '{}', total handlers: {}", name_, handlers_.size());
    }
    
    void subscribe(EventHandler::Callback callback) {
        handlers_.push_back(std::make_shared<FunctionalEventHandler>(std::move(callback)));
        EVENT_LOG_DEBUG("Callback subscribed to adapter '{}', total handlers: {}", name_, handlers_.size());
    }
    
    void unsubscribe(std::shared_ptr<EventHandler> handler) {
        auto prev_size = handlers_.size();
        handlers_.erase(
            std::remove(handlers_.begin(), handlers_.end(), handler),
            handlers_.end()
        );
        EVENT_LOG_DEBUG("Handler unsubscribed from adapter '{}', handlers: {} -> {}", name_, prev_size, handlers_.size());
    }
    
protected:
    void emit_event(EventPtr event) {
        EVENT_LOG_TRACE("Adapter '{}' emitting event of type: {}", name_, event->type().name());
        for (const auto& handler : handlers_) {
            if (handler) {
                try {
                    handler->handle(event);
                } catch (const std::exception& e) {
                    EVENT_LOG_ERROR("Handler exception in adapter '{}': {}", name_, e.what());
                }
            }
        }
    }
    
    void set_state(State new_state) {
        auto old_state = state_.load();
        state_.store(new_state);
        EVENT_LOG_INFO("Adapter '{}' state changed: {} -> {}", name_, static_cast<int>(old_state), static_cast<int>(new_state));
    }
    
    template<typename T, typename... Args>
    void emit(Args&&... args) {
        emit_event(make_event<T>(std::forward<Args>(args)...));
    }
    
private:
    std::string name_;
    std::atomic<State> state_;
    std::vector<std::shared_ptr<EventHandler>> handlers_;
};

template<typename SourceType>
class TypedDataSourceAdapter : public DataSourceAdapter {
public:
    using Source = SourceType;
    
    TypedDataSourceAdapter(std::string name) : DataSourceAdapter(std::move(name)) {}
    
protected:
    virtual void on_data_update(const Source& source) = 0;
};

class PollingDataSourceAdapter : public DataSourceAdapter {
public:
    PollingDataSourceAdapter(std::string name, std::chrono::milliseconds interval)
        : DataSourceAdapter(std::move(name))
        , polling_interval_(interval)
        , should_poll_(false) {}
    
    void connect() override {
        EVENT_LOG_INFO("Connecting polling adapter '{}' with interval {}ms", name(), polling_interval_.count());
        should_poll_ = true;
        set_state(State::Connected);
        start_polling();
    }
    
    void disconnect() override {
        EVENT_LOG_INFO("Disconnecting polling adapter '{}'", name());
        should_poll_ = false;
        stop_polling();
        set_state(State::Disconnected);
    }
    
    bool is_connected() const override {
        return should_poll_.load();
    }
    
protected:
    virtual void poll() = 0;
    
    void start_polling() {
        polling_thread_ = std::thread([this]() {
            EVENT_LOG_DEBUG("Polling thread started for adapter '{}'", name());
            while (should_poll_.load()) {
                try {
                    poll();
                } catch (const std::exception& e) {
                    EVENT_LOG_ERROR("Polling error in adapter '{}': {}", name(), e.what());
                }
                std::this_thread::sleep_for(polling_interval_);
            }
            EVENT_LOG_DEBUG("Polling thread stopped for adapter '{}'", name());
        });
    }
    
    void stop_polling() {
        should_poll_ = false;
        if (polling_thread_.joinable()) {
            EVENT_LOG_DEBUG("Waiting for polling thread to finish for adapter '{}'", name());
            polling_thread_.join();
        }
    }
    
private:
    std::chrono::milliseconds polling_interval_;
    std::atomic<bool> should_poll_;
    std::thread polling_thread_;
};

} // namespace event_adapter