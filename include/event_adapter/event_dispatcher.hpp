#pragma once

#include "event.hpp"
#include "data_source_adapter.hpp"
#include "logging.hpp"
#include <boost/sml.hpp>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <typeindex>
#include <thread>
#include <atomic>

namespace event_adapter {

template<typename StateMachine>
class EventDispatcher {
public:
    using EventProcessor = std::function<void(EventPtr, StateMachine&)>;
    
    explicit EventDispatcher(StateMachine& sm) : state_machine_(sm), running_(false) {
        EVENT_LOG_DEBUG("EventDispatcher created");
    }
    
    ~EventDispatcher() {
        EVENT_LOG_DEBUG("EventDispatcher destructor called");
        stop();
    }
    
    template<typename EventType>
    void register_event_processor(std::function<void(const EventType&, StateMachine&)> processor) {
        EVENT_LOG_DEBUG("Registering event processor for type: {}", typeid(EventType).name());
        processors_[std::type_index(typeid(EventType))] = [processor](EventPtr event, StateMachine& sm) {
            if (auto typed_event = std::dynamic_pointer_cast<TypedEvent<EventType>>(event)) {
                processor(typed_event->data(), sm);
            }
        };
    }
    
    template<typename EventType, typename SMEvent>
    void register_event_mapping(std::function<SMEvent(const EventType&)> converter) {
        register_event_processor<EventType>([converter](const EventType& event, StateMachine& sm) {
            sm.process_event(converter(event));
        });
    }
    
    template<typename EventType>
    void register_direct_mapping() {
        register_event_processor<EventType>([](const EventType& event, StateMachine& sm) {
            sm.process_event(event);
        });
    }
    
    void dispatch(EventPtr event) {
        EVENT_LOG_TRACE("Dispatching event of type: {}", event->type().name());
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            event_queue_.push(event);
            EVENT_LOG_TRACE("Event queued, queue size: {}", event_queue_.size());
        }
        queue_cv_.notify_one();
    }
    
    void start() {
        EVENT_LOG_INFO("Starting EventDispatcher");
        running_ = true;
        processor_thread_ = std::thread([this]() {
            process_events();
        });
        EVENT_LOG_INFO("EventDispatcher started");
    }
    
    void stop() {
        EVENT_LOG_INFO("Stopping EventDispatcher");
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            running_ = false;
        }
        queue_cv_.notify_all();
        
        if (processor_thread_.joinable()) {
            EVENT_LOG_DEBUG("Waiting for processor thread to finish");
            processor_thread_.join();
        }
        EVENT_LOG_INFO("EventDispatcher stopped");
    }
    
    size_t queue_size() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return event_queue_.size();
    }
    
private:
    void process_events() {
        EVENT_LOG_DEBUG("Event processing thread started");
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() { return !event_queue_.empty() || !running_; });
            
            while (!event_queue_.empty()) {
                auto event = event_queue_.front();
                event_queue_.pop();
                EVENT_LOG_TRACE("Processing event from queue, remaining: {}", event_queue_.size());
                lock.unlock();
                
                process_event(event);
                
                lock.lock();
            }
        }
        EVENT_LOG_DEBUG("Event processing thread exiting");
    }
    
    void process_event(EventPtr event) {
        auto it = processors_.find(event->type());
        if (it != processors_.end()) {
            EVENT_LOG_TRACE("Processing event with registered handler: {}", event->type().name());
            try {
                it->second(event, state_machine_);
            } catch (const std::exception& e) {
                EVENT_LOG_ERROR("Exception processing event {}: {}", event->type().name(), e.what());
            }
        } else {
            EVENT_LOG_WARN("No processor registered for event type: {}", event->type().name());
        }
    }
    
    StateMachine& state_machine_;
    std::unordered_map<std::type_index, EventProcessor> processors_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<EventPtr> event_queue_;
    std::atomic<bool> running_;
    std::thread processor_thread_;
};

template<typename SM>
class SmlEventDispatcher : public EventDispatcher<SM> {
public:
    using StateMachine = SM;
    
    explicit SmlEventDispatcher(SM& sm) : EventDispatcher<SM>(sm) {}
    
    template<typename Event>
    void auto_register() {
        this->template register_direct_mapping<Event>();
    }
    
    template<typename... Events>
    void auto_register_all() {
        (auto_register<Events>(), ...);
    }
};

template<typename StateMachine>
class EventAdapterSystem {
public:
    using Dispatcher = EventDispatcher<StateMachine>;
    
    EventAdapterSystem(StateMachine& sm) : dispatcher_(sm) {
        EVENT_LOG_INFO("EventAdapterSystem created");
    }
    
    void add_adapter(std::shared_ptr<DataSourceAdapter> adapter) {
        EVENT_LOG_INFO("Adding adapter: {}", adapter->name());
        adapter->subscribe([this](EventPtr event) {
            dispatcher_.dispatch(event);
        });
        adapters_.push_back(adapter);
    }
    
    void start() {
        EVENT_LOG_INFO("Starting EventAdapterSystem");
        dispatcher_.start();
        for (auto& adapter : adapters_) {
            EVENT_LOG_INFO("Connecting adapter: {}", adapter->name());
            adapter->connect();
        }
        EVENT_LOG_INFO("EventAdapterSystem started with {} adapters", adapters_.size());
    }
    
    void stop() {
        EVENT_LOG_INFO("Stopping EventAdapterSystem");
        for (auto& adapter : adapters_) {
            EVENT_LOG_INFO("Disconnecting adapter: {}", adapter->name());
            adapter->disconnect();
        }
        dispatcher_.stop();
        EVENT_LOG_INFO("EventAdapterSystem stopped");
    }
    
    Dispatcher& dispatcher() { return dispatcher_; }
    
private:
    Dispatcher dispatcher_;
    std::vector<std::shared_ptr<DataSourceAdapter>> adapters_;
};

} // namespace event_adapter