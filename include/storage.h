#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <atomic>
#include <thread>
#include <variant>
#include <nlohmann/json.hpp> // for json 

using json = nlohmann::json;

class Storage {
private:
    using InternalValue = std::variant<int, double, std::string, bool>;

    struct ValueEntry {
        InternalValue value;
        std::chrono::steady_clock::time_point expiry;
        bool hasExpiry = false;
    };

    mutable std::mutex mtx_;
    std::unordered_map<std::string, ValueEntry> map_;
    
    std::atomic<bool> stop_{false};
    std::thread cleaner_thread_;

    void cleaner(); // background cleanup loop

public:
    Storage();
    ~Storage();

    using Value = InternalValue; // public alias

    // Store a key-value pair
    void set(const std::string &key, const Value &value);
    void set(const std::string &key, const Value &value, int ttl_secs);

    // Retrieve the value for a key
    // Returns std::nullopt if key does not exist
    std::optional<Value> get(const std::string &key);

    // Delete a key
    // Returns true if deleted, false if key did not exist
    bool del(const std::string &key);

    // Check if a key exists
    bool exists(const std::string &key);

    // Get the number of stored key-value pairs
    size_t size() const;

    // Set a TTL on an existing key (in seconds)
    // Return true if TTL was set/updated, false if key not found
    bool expire(const std::string &key, int ttl_secs);

    // Get a snapshot of all keys and values
    std::unordered_map<std::string, Value> dump() const;

    // JSON persistence
    bool saveToFile(const std::string &filename) const;
    bool loadFromFile(const std::string &filename);
};