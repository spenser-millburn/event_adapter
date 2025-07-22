#pragma once

#include "../data_source_adapter.hpp"
#include "../logging.hpp"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <nlohmann/json.hpp>

namespace event_adapter::adapters {

using json = nlohmann::json;

class WebSocketAdapter : public DataSourceAdapter {
public:
    using client = websocketpp::client<websocketpp::config::asio_client>;
    using message_ptr = websocketpp::config::asio_client::message_type::ptr;
    
    WebSocketAdapter(std::string name, std::string uri) 
        : DataSourceAdapter(std::move(name))
        , uri_(std::move(uri)) {
        
        EVENT_LOG_INFO("WebSocketAdapter '{}' created with URI: {}", this->name(), uri_);
        
        client_.set_access_channels(websocketpp::log::alevel::none);
        client_.set_error_channels(websocketpp::log::elevel::none);
        
        client_.init_asio();
        
        client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            on_open(hdl);
        });
        
        client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            on_close(hdl);
        });
        
        client_.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
            on_message(hdl, msg);
        });
        
        client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            on_fail(hdl);
        });
    }
    
    ~WebSocketAdapter() {
        EVENT_LOG_DEBUG("WebSocketAdapter '{}' destructor called", name());
        disconnect();
    }
    
    void connect() override {
        EVENT_LOG_INFO("WebSocketAdapter '{}' connecting to: {}", name(), uri_);
        set_state(State::Connecting);
        
        websocketpp::lib::error_code ec;
        client::connection_ptr con = client_.get_connection(uri_, ec);
        
        if (ec) {
            EVENT_LOG_ERROR("WebSocketAdapter '{}' connection error: {}", name(), ec.message());
            set_state(State::Error);
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), ec.message());
            return;
        }
        
        connection_ = con;
        client_.connect(con);
        
        client_thread_ = std::thread([this]() {
            EVENT_LOG_DEBUG("WebSocketAdapter '{}' client thread started", name());
            client_.run();
            EVENT_LOG_DEBUG("WebSocketAdapter '{}' client thread finished", name());
        });
    }
    
    void disconnect() override {
        EVENT_LOG_INFO("WebSocketAdapter '{}' disconnecting", name());
        set_state(State::Disconnecting);
        
        if (connection_) {
            websocketpp::lib::error_code ec;
            client_.close(connection_->get_handle(), websocketpp::close::status::normal, "Closing", ec);
            if (ec) {
                EVENT_LOG_WARN("WebSocketAdapter '{}' close error: {}", name(), ec.message());
            }
        }
        
        client_.stop();
        
        if (client_thread_.joinable()) {
            client_thread_.join();
        }
        
        set_state(State::Disconnected);
    }
    
    bool is_connected() const override {
        return state() == State::Connected;
    }
    
    void send_message(const std::string& message) {
        if (connection_ && is_connected()) {
            EVENT_LOG_TRACE("WebSocketAdapter '{}' sending message: {} bytes", name(), message.size());
            websocketpp::lib::error_code ec;
            client_.send(connection_->get_handle(), message, websocketpp::frame::opcode::text, ec);
            
            if (ec) {
                EVENT_LOG_ERROR("WebSocketAdapter '{}' send error: {}", name(), ec.message());
                emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), ec.message());
            }
        } else {
            EVENT_LOG_WARN("WebSocketAdapter '{}' cannot send message - not connected", name());
        }
    }
    
    void send_json(const json& data) {
        EVENT_LOG_TRACE("WebSocketAdapter '{}' sending JSON message", name());
        send_message(data.dump());
    }
    
protected:
    virtual void on_json_message(const json& message) {
        emit<DataUpdateEvent>("websocket", "message", message, json{});
    }
    
    virtual void on_text_message(const std::string& message) {
        EVENT_LOG_TRACE("WebSocketAdapter '{}' received message: {} bytes", name(), message.size());
        try {
            json j = json::parse(message);
            on_json_message(j);
        } catch (const json::parse_error& e) {
            EVENT_LOG_WARN("WebSocketAdapter '{}' JSON parse error: {}", name(), e.what());
            emit<DataUpdateEvent>("websocket", "raw_message", message, std::string{});
        }
    }
    
private:
    void on_open(websocketpp::connection_hdl hdl) {
        EVENT_LOG_INFO("WebSocketAdapter '{}' connected successfully", name());
        set_state(State::Connected);
        emit<ConnectionEvent>(ConnectionEvent::Type::Connected, name(), uri_);
    }
    
    void on_close(websocketpp::connection_hdl hdl) {
        EVENT_LOG_INFO("WebSocketAdapter '{}' connection closed", name());
        set_state(State::Disconnected);
        emit<ConnectionEvent>(ConnectionEvent::Type::Disconnected, name(), uri_);
    }
    
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
        on_text_message(msg->get_payload());
    }
    
    void on_fail(websocketpp::connection_hdl hdl) {
        EVENT_LOG_ERROR("WebSocketAdapter '{}' connection failed", name());
        set_state(State::Error);
        emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), "Connection failed");
    }
    
    std::string uri_;
    client client_;
    client::connection_ptr connection_;
    std::thread client_thread_;
};

} // namespace event_adapter::adapters