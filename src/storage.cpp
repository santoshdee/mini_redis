#include "storage.h"
#include <iostream>
#include <fstream>  // std::ofstream, std::ifstream

// Thread safety: Every method locks mtx_ to ensure safe access

Storage::Storage()
{
    // launch background cleaner thread
    cleaner_thread_ = std::thread([this]()
                                  { cleaner(); });
}

Storage::~Storage()
{
    // signal cleaner thread to loop
    stop_ = true;
    if (cleaner_thread_.joinable())
    {
        cleaner_thread_.join();
    }
}

// Store a key-value pair
void Storage::set(const std::string &key, const Value &value)
{
    std::lock_guard<std::mutex> lock(mtx_);
    map_[key] = ValueEntry{value, {}, false};
}

void Storage::set(const std::string &key, const Value &value, int ttl_secs)
{
    std::lock_guard<std::mutex> lock(mtx_);
    ValueEntry entry;
    entry.value = value;
    entry.hasExpiry = true;
    entry.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_secs);
    map_[key] = entry;
}

// Retrieve the value for a key
std::optional<Storage::Value> Storage::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end())
    {
        return std::nullopt;
    }

    if (it->second.hasExpiry && std::chrono::steady_clock::now() >= it->second.expiry)
    {
        // key expired, erase it
        map_.erase(it);
        return std::nullopt;
    }

    return it->second.value;
}

// Delete a key
// Returns true if a key was removed, false if it wasn't found
bool Storage::del(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx_);
    return map_.erase(key) > 0;
}

// Check if a key exists
bool Storage::exists(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end())
        return false;

    if (it->second.hasExpiry && std::chrono::steady_clock::now() >= it->second.expiry)
    {
        map_.erase(it);
        return false;
    }
    return true;
}

// Return the number of stored key-value pairs
// Use std::mutex, so it can lock even in a const method
size_t Storage::size() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return map_.size();
}

// If key exists → attaches/updates expiry
// If key doesn’t exist → returns false (like Redis)
// Background cleaner thread will remove it when expired
bool Storage::expire(const std::string &key, int ttl_secs)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end())
    {
        return false; // key does not exist
    }

    it->second.hasExpiry = true;
    it->second.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_secs);
    return true;
}

// returns the entire map
std::unordered_map<std::string, Storage::Value> Storage::dump() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::unordered_map<std::string, Value> snapshot;

    auto now = std::chrono::steady_clock::now();
    for(const auto& [key, val]: map_) {
        if(val.hasExpiry && now >= val.expiry) continue; // skip expired
        snapshot[key] = val.value;
    }
    return snapshot;
}

void Storage::cleaner()
{
    while (!stop_)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto now = std::chrono::steady_clock::now();
            for (auto it = map_.begin(); it != map_.end();)
            {
                if (it->second.hasExpiry && now >= it->second.expiry)
                {
                    it = map_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); // runs every second
    }
}

/*
 * JSON Persistence
 * saveToFile()
 * loadFromFile()
*/

bool Storage::saveToFile(const std::string &filename) const {
    std::lock_guard<std::mutex> lock(mtx_);

    json js;
    auto now = std::chrono::steady_clock::now();

    for(const auto& [key, entry]: map_) {
        // skip expired keys
        if(entry.hasExpiry && now >= entry.expiry) continue;

        json valueJson;
        std::visit([&](auto &&arg) {
            valueJson["value"] = arg;
        }, entry.value);

        valueJson["hasExpiry"] = entry.hasExpiry;
        if(entry.hasExpiry) {
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(entry.expiry - now).count();
            valueJson["ttl_remaining"] = remaining;
        } else {
            valueJson["ttl_remaining"] = nullptr;
        }

        js[key] = valueJson;
    }

    std::ofstream file(filename);
    if(!file.is_open()) return false;
    file << js.dump(4);
    return true;
}

bool Storage::loadFromFile(const std::string &filename) {
    std::lock_guard<std::mutex> lock(mtx_);

    std::ifstream file(filename);
    if(!file.is_open()) return false;

    json js;
    file >> js;
    file.close();

    map_.clear();
    auto now = std::chrono::steady_clock::now();

    for(auto it = js.begin(); it != js.end(); it++) {
        const std::string &key = it.key();
        const json &entryJson = it.value();

        ValueEntry entry;
        const auto &v = entryJson["value"];

        if(v.is_boolean()) entry.value = v.get<bool>();
        else if(v.is_number_integer()) entry.value = v.get<int>();
        else if(v.is_number_float()) entry.value = v.get<double>();
        else if(v.is_string()) entry.value = v.get<std::string>();

        entry.hasExpiry = entryJson.value("hasExpiry", false);
        if(entry.hasExpiry && !entryJson["ttl_remaining"].is_null()) {
            int remaining = entryJson["ttl_remaining"];
            entry.expiry = now + std::chrono::seconds(remaining);
        }

        map_[key] = entry;
    }

    return true;
}
















