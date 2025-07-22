#pragma once

#include "event.hpp"
#include "data_source_adapter.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <regex>

namespace event_adapter {

class EventFilter {
public:
    virtual ~EventFilter() = default;
    virtual bool passes(EventPtr event) const = 0;
};

class TypeFilter : public EventFilter {
public:
    template<typename T>
    static std::unique_ptr<TypeFilter> create() {
        return std::make_unique<TypeFilter>(std::type_index(typeid(T)));
    }
    
    explicit TypeFilter(std::type_index type) : type_(type) {}
    
    bool passes(EventPtr event) const override {
        return event->type() == type_;
    }
    
private:
    std::type_index type_;
};

class PredicateFilter : public EventFilter {
public:
    using Predicate = std::function<bool(EventPtr)>;
    
    explicit PredicateFilter(Predicate predicate) : predicate_(std::move(predicate)) {}
    
    bool passes(EventPtr event) const override {
        return predicate_(event);
    }
    
private:
    Predicate predicate_;
};

template<typename T>
class TypedPredicateFilter : public EventFilter {
public:
    using Predicate = std::function<bool(const T&)>;
    
    explicit TypedPredicateFilter(Predicate predicate) : predicate_(std::move(predicate)) {}
    
    bool passes(EventPtr event) const override {
        if (auto typed_event = std::dynamic_pointer_cast<TypedEvent<T>>(event)) {
            return predicate_(typed_event->data());
        }
        return false;
    }
    
private:
    Predicate predicate_;
};

class CompositeFilter : public EventFilter {
public:
    void add_filter(std::unique_ptr<EventFilter> filter) {
        filters_.push_back(std::move(filter));
    }
    
protected:
    std::vector<std::unique_ptr<EventFilter>> filters_;
};

class AndFilter : public CompositeFilter {
public:
    bool passes(EventPtr event) const override {
        return std::all_of(filters_.begin(), filters_.end(),
                          [&event](const auto& filter) { return filter->passes(event); });
    }
};

class OrFilter : public CompositeFilter {
public:
    bool passes(EventPtr event) const override {
        return std::any_of(filters_.begin(), filters_.end(),
                          [&event](const auto& filter) { return filter->passes(event); });
    }
};

class NotFilter : public EventFilter {
public:
    explicit NotFilter(std::unique_ptr<EventFilter> filter) : filter_(std::move(filter)) {}
    
    bool passes(EventPtr event) const override {
        return !filter_->passes(event);
    }
    
private:
    std::unique_ptr<EventFilter> filter_;
};

class EventTransformer {
public:
    virtual ~EventTransformer() = default;
    virtual EventPtr transform(EventPtr event) = 0;
};

template<typename From, typename To>
class TypedEventTransformer : public EventTransformer {
public:
    using TransformFunc = std::function<To(const From&)>;
    
    explicit TypedEventTransformer(TransformFunc func) : transform_func_(std::move(func)) {}
    
    EventPtr transform(EventPtr event) override {
        if (auto typed_event = std::dynamic_pointer_cast<TypedEvent<From>>(event)) {
            return make_event<To>(transform_func_(typed_event->data()));
        }
        return event;
    }
    
private:
    TransformFunc transform_func_;
};

class EventPipeline {
public:
    void add_filter(std::unique_ptr<EventFilter> filter) {
        filters_.push_back(std::move(filter));
    }
    
    void add_transformer(std::unique_ptr<EventTransformer> transformer) {
        transformers_.push_back(std::move(transformer));
    }
    
    EventPtr process(EventPtr event) {
        for (const auto& filter : filters_) {
            if (!filter->passes(event)) {
                return nullptr;
            }
        }
        
        for (const auto& transformer : transformers_) {
            event = transformer->transform(event);
            if (!event) {
                return nullptr;
            }
        }
        
        return event;
    }
    
    template<typename T>
    void filter_by_type() {
        add_filter(TypeFilter::create<T>());
    }
    
    template<typename T>
    void filter_by_predicate(std::function<bool(const T&)> predicate) {
        add_filter(std::make_unique<TypedPredicateFilter<T>>(std::move(predicate)));
    }
    
    template<typename From, typename To>
    void transform(std::function<To(const From&)> transformer) {
        add_transformer(std::make_unique<TypedEventTransformer<From, To>>(std::move(transformer)));
    }
    
private:
    std::vector<std::unique_ptr<EventFilter>> filters_;
    std::vector<std::unique_ptr<EventTransformer>> transformers_;
};

class FilteredEventHandler : public EventHandler {
public:
    FilteredEventHandler(std::unique_ptr<EventPipeline> pipeline, 
                        std::shared_ptr<EventHandler> handler)
        : pipeline_(std::move(pipeline))
        , handler_(std::move(handler)) {}
    
    void handle(EventPtr event) override {
        if (auto processed = pipeline_->process(event)) {
            handler_->handle(processed);
        }
    }
    
private:
    std::unique_ptr<EventPipeline> pipeline_;
    std::shared_ptr<EventHandler> handler_;
};

} // namespace event_adapter