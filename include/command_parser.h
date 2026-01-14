#pragma once
#include "storage.h"
#include <string>
#include <vector>

class CommandParser {
private:
    Storage &store;

    int clientSock; // unique per client

    // Helper: tokenize with quotes
    std::vector<std::string> tokenize(const std::string &line);

    // Helper: convert string to variant value
    Storage::Value parseValue(const std::string &token);

public:
    CommandParser(Storage &store, int clientSock);

    // Parse a line of input and execute the command
    std::string execute(const std::string &line);
};