#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <typeindex>
#include <variant>
#include <any>
#include <atomic>

namespace event_adapter {

using EventTimestamp = std::chrono::steady_clock::time_point;
using EventId = std::uint64_t;

class Event {
public:
    Event() : id_(generate_id()), timestamp_(std::chrono::steady_clock::now()) {}
    
    virtual ~Event() = default;
    
    EventId id() const { return id_; }
    EventTimestamp timestamp() const { return timestamp_; }
    
    virtual std::type_index type() const = 0;
    virtual std::string name() const = 0;
    
protected:
    static EventId generate_id() {
        static std::atomic<EventId> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
    
private:
    EventId id_;
    EventTimestamp timestamp_;
};

template<typename T>
class TypedEvent : public Event {
public:
    using EventType = T;
    
    explicit TypedEvent(T data) : data_(std::move(data)) {}
    
    const T& data() const { return data_; }
    T& data() { return data_; }
    
    std::type_index type() const override {
        return std::type_index(typeid(T));
    }
    
    std::string name() const override {
        return typeid(T).name();
    }
    
private:
    T data_;
};

template<typename... Events>
using EventVariant = std::variant<Events...>;

struct DataUpdateEvent {
    std::string source;
    std::string key;
    std::any value;
    std::any previous_value;
};

struct ConnectionEvent {
    enum class Type { Connected, Disconnected, Error };
    Type type;
    std::string source;
    std::string details;
};

struct HeartbeatEvent {
    std::string source;
    std::chrono::milliseconds interval;
};

using EventPtr = std::shared_ptr<Event>;

template<typename T, typename... Args>
EventPtr make_event(Args&&... args) {
    return std::make_shared<TypedEvent<T>>(T{std::forward<Args>(args)...});
}

} // namespace event_adapter