#pragma once

#include "../data_source_adapter.hpp"
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

namespace event_adapter::adapters {

using json = nlohmann::json;

class HttpAdapter : public PollingDataSourceAdapter {
public:
    HttpAdapter(std::string name, std::string url, std::chrono::milliseconds interval)
        : PollingDataSourceAdapter(std::move(name), interval)
        , url_(std::move(url))
        , curl_(nullptr) {
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~HttpAdapter() {
        disconnect();
        curl_global_cleanup();
    }
    
protected:
    void poll() override {
        std::string response = fetch_data();
        if (!response.empty()) {
            process_response(response);
        }
    }
    
    virtual void process_response(const std::string& response) {
        emit<DataUpdateEvent>(name(), "http_response", response, last_response_);
        last_response_ = response;
    }
    
private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    std::string fetch_data() {
        std::string response;
        
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res != CURLE_OK) {
                emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), 
                                    std::string("HTTP request failed: ") + curl_easy_strerror(res));
            }
            
            curl_easy_cleanup(curl);
        }
        
        return response;
    }
    
    std::string url_;
    std::string last_response_;
    CURL* curl_;
};

class JsonHttpAdapter : public HttpAdapter {
public:
    using HttpAdapter::HttpAdapter;
    
protected:
    void process_response(const std::string& response) override {
        try {
            json j = json::parse(response);
            emit<DataUpdateEvent>(name(), "json_data", j, last_json_);
            last_json_ = j;
        } catch (const json::parse_error& e) {
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), 
                                std::string("JSON parse error: ") + e.what());
        }
    }
    
private:
    json last_json_;
};

} // namespace event_adapter::adapters