#pragma once

#include "../data_source_adapter.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <set>

namespace event_adapter::adapters {

namespace fs = std::filesystem;

class FileWatcherAdapter : public PollingDataSourceAdapter {
public:
    FileWatcherAdapter(std::string name, std::string path, std::chrono::milliseconds interval)
        : PollingDataSourceAdapter(std::move(name), interval)
        , path_(std::move(path))
        , last_write_time_() {}
    
protected:
    void poll() override {
        try {
            if (fs::exists(path_)) {
                auto current_write_time = fs::last_write_time(path_);
                
                if (current_write_time != last_write_time_) {
                    last_write_time_ = current_write_time;
                    on_file_changed();
                }
            } else if (last_write_time_ != fs::file_time_type{}) {
                last_write_time_ = fs::file_time_type{};
                emit<DataUpdateEvent>(name(), "file_deleted", path_, std::string{});
            }
        } catch (const fs::filesystem_error& e) {
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), e.what());
        }
    }
    
    virtual void on_file_changed() {
        emit<DataUpdateEvent>(name(), "file_modified", path_, std::string{});
    }
    
protected:
    std::string path_;
    
private:
    fs::file_time_type last_write_time_;
};

class FileContentAdapter : public FileWatcherAdapter {
public:
    using FileWatcherAdapter::FileWatcherAdapter;
    
protected:
    void on_file_changed() override {
        std::string content = read_file_content();
        emit<DataUpdateEvent>(name(), "content", content, last_content_);
        last_content_ = std::move(content);
    }
    
private:
    std::string read_file_content() {
        std::ifstream file(path_);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::string last_content_;
};

class DirectoryWatcherAdapter : public PollingDataSourceAdapter {
public:
    DirectoryWatcherAdapter(std::string name, std::string path, std::chrono::milliseconds interval)
        : PollingDataSourceAdapter(std::move(name), interval)
        , path_(std::move(path)) {}
    
protected:
    void poll() override {
        try {
            if (!fs::exists(path_) || !fs::is_directory(path_)) {
                return;
            }
            
            std::set<std::string> current_files;
            for (const auto& entry : fs::directory_iterator(path_)) {
                current_files.insert(entry.path().filename().string());
            }
            
            std::vector<std::string> added;
            std::vector<std::string> removed;
            
            std::set_difference(current_files.begin(), current_files.end(),
                              last_files_.begin(), last_files_.end(),
                              std::back_inserter(added));
            
            std::set_difference(last_files_.begin(), last_files_.end(),
                              current_files.begin(), current_files.end(),
                              std::back_inserter(removed));
            
            for (const auto& file : added) {
                emit<DataUpdateEvent>(name(), "file_added", file, std::string{});
            }
            
            for (const auto& file : removed) {
                emit<DataUpdateEvent>(name(), "file_removed", file, std::string{});
            }
            
            last_files_ = std::move(current_files);
            
        } catch (const fs::filesystem_error& e) {
            emit<ConnectionEvent>(ConnectionEvent::Type::Error, name(), e.what());
        }
    }
    
private:
    std::string path_;
    std::set<std::string> last_files_;
};

} // namespace event_adapter::adapters