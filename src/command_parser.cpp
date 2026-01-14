#include "command_parser.h"
#include "../include/constants.h"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <iomanip>    // for setw, left
#include <variant>
#include <filesystem> // C++17 for folder management

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_MAGENTA "\033[35m"


CommandParser::CommandParser(Storage &s, int sock) : store(s), clientSock(sock) {}

std::vector<std::string> CommandParser::tokenize(const std::string &line) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuotes = false;

    for(size_t i=0; i<line.size(); i++) {
        char ch = line[i];

        if(ch == '"') {
            inQuotes = !inQuotes;
            if(!inQuotes) {
                tokens.push_back(token);
                token.clear();
            }
        } else if(std::isspace(ch) && !inQuotes) {
            if(!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(ch);
        }
    }

    if(!token.empty()) tokens.push_back(token);
    return tokens;
}

Storage::Value CommandParser::parseValue(const std::string &token) {
    // try int
    try {
        size_t pos;
        int i = std::stoi(token, &pos);
        if(pos == token.size()) return i;
    } catch (...) {}

    // try double 
    try {
        size_t pos;
        double d = std::stod(token, &pos);
        if(pos == token.size()) return d;
    } catch (...) {}

    // try bool
    if(token == "true" || token == "TRUE") return true;
    if(token == "false" || token == "FALSE") return false;

    // fallback string
    return token;
}

// helper to stringify variant value
static std::string valueToString(const Storage::Value &v) {
    if(std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if(std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if(std::holds_alternative<double>(v)) return std::to_string(std::get<double>(v));
    if(std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
    return "(unknown)";
}

std::string CommandParser::execute(const std::string &line) {
    // ensure base data directory exists
    if(!std::filesystem::exists(DATA_DIR)) {
        std::filesystem::create_directory(DATA_DIR);
    }

    // ensure client-specific directory exists
    std::string clientDir = DATA_DIR + "/client_" + std::to_string(clientSock);
    if(!std::filesystem::exists(clientDir)) {
        std::filesystem::create_directories(clientDir);
    }

    auto tokens = tokenize(line);
    if(tokens.empty()) return "";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if(cmd == "SET") {
        if(tokens.size() < 3) return std::string(COLOR_RED) + "(error) wrong number of arguments" + COLOR_RESET;
        std::string key = tokens[1];
        Storage::Value val = parseValue(tokens[2]);
        if(tokens.size() == 4) {
            int ttl = std::stoi(tokens[3]);
            store.set(key, val, ttl);
        } else {
            store.set(key, val);
        }
        return std::string(COLOR_GREEN) + "OK" + COLOR_RESET;
    }

    if(cmd == "GET") {
        if(tokens.size() != 2) return std::string(COLOR_RED) + "(error) wrong number of arguments" + COLOR_RESET;
        std::string key = tokens[1];

        if(!store.exists(key)) {
            return std::string(COLOR_YELLOW) + "(nil) no such key" + COLOR_RESET;
        }

        auto val = store.get(key);
        if(!val) return std::string(COLOR_YELLOW) + "(nil)" + COLOR_RESET;
        return std::string(COLOR_CYAN) + valueToString(*val) + COLOR_RESET;
    }

    if(cmd == "DEL") {
        if(tokens.size() != 2) return std::string(COLOR_RED) + "(error) wrong number of arguments" + COLOR_RESET;
        
        std::string key = tokens[1];
        if(!store.exists(key)) {
            return std::string(COLOR_YELLOW) + "(nil) no such key" + COLOR_RESET;
        }
        
        bool deleted = store.del(key);
        return deleted 
            ? std::string(COLOR_MAGENTA) + "(integer) 1" + COLOR_RESET
            : std::string(COLOR_YELLOW) + "(nil) deletion failed" + COLOR_RESET;
    }

    if(cmd == "EXISTS") {
        if(tokens.size() != 2) return std::string(COLOR_RED) + "(error) wrong number of arguments" + COLOR_RESET;
        return store.exists(tokens[1]) 
            ? std::string(COLOR_MAGENTA) + "(integer) 1" + COLOR_RESET
            : std::string(COLOR_YELLOW) + "(integer) 0" + COLOR_RESET;
    }

    if(cmd == "EXPIRE") {
        if(tokens.size() != 3) return std::string(COLOR_RED) + "(error) wrong number of arguments" + COLOR_RESET;
        
        std::string key = tokens[1];
        if(!store.exists(key)) {
            return std::string(COLOR_YELLOW) + "(nil) no such key to expire" + COLOR_RESET;
        }

        try {
            int ttl = std::stoi(tokens[2]);
            if(ttl <= 0) return std::string(COLOR_RED) + "(error) TTL must be positive" + COLOR_RESET;

            bool success = store.expire(key, ttl);
            return success
                ? std::string(COLOR_MAGENTA) + "(integer) 1" + COLOR_RESET
                : std::string(COLOR_YELLOW) + "(nil) failed to set expiry" + COLOR_RESET;
        } catch(...) {
            return std::string(COLOR_RED) + "(error) invalid TTL value" + COLOR_RESET;
        }
    }

    if(cmd == "SHOW" || cmd == "DISPLAY") {
        auto snapshot = store.dump();
        if(snapshot.empty()) return std::string(COLOR_YELLOW) + "(empty) store" + COLOR_RESET;

        // Determine max key/value widths dynamically
        size_t maxKeyLen = 3; // for header "KEY"
        size_t maxValLen = 5; // for header "VALUE"

        for(const auto& [key, value]: snapshot) {
            maxKeyLen = std::max(maxKeyLen, key.size());
            std::string valStr = valueToString(value);
            maxValLen = std::max(maxValLen, valStr.size());
        }

        // Add some padding
        maxKeyLen += 2;
        maxValLen += 2;

        std::ostringstream out;
        out << COLOR_CYAN 
            << std::string(maxKeyLen + maxValLen + 5, '-') << "\n"
            << std::left << std::setw(maxKeyLen) << "KEY" 
            << std::setw(maxValLen) << "VALUE" << "\n"
            << std::string(maxKeyLen + maxValLen + 5, '-') 
            << COLOR_RESET << "\n";

        for(const auto& [key, value]: snapshot) {
            out << std::left << std::setw(maxKeyLen) << key
                << std::setw(maxValLen) << valueToString(value) << "\n";
        }

        out << COLOR_CYAN << std::string(maxKeyLen + maxValLen + 5, '-') << COLOR_RESET;
        return out.str();
    }

    // SAVE (per-client isolation)
    if(cmd == "SAVE") {
        if(tokens.size() != 2) return std::string(COLOR_RED) + "(error) SAVE requires filename" + COLOR_RESET;

        std::string filename = clientDir + "/" + tokens[1];
        return store.saveToFile(filename) 
            ? std::string(COLOR_GREEN) + "OK: Saved to " + filename + COLOR_RESET
            : std::string(COLOR_RED) + "(error) could not save file" + COLOR_RESET;
    }

    // LOAD
    if(cmd == "LOAD") {
        if(tokens.size() != 2) return std::string(COLOR_RED) + "(error) LOAD requires filename" + COLOR_RESET;

        std::string filename = clientDir + "/" + tokens[1];
        return store.loadFromFile(filename) 
            ? std::string(COLOR_GREEN) + "OK: Loaded from " + filename + COLOR_RESET
            : std::string(COLOR_RED) + "(error) could not load file" + COLOR_RESET;
    }

    return std::string(COLOR_RED) + "(error) unknown command" + COLOR_RESET;
}
