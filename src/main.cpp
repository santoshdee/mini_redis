#include "../include/storage.h"
#include "../include/command_parser.h"
#include "../include/server.h"
#include <iostream>
#include <algorithm>

int main() {
    
    try {
        Server server(6379);
        server.start();
    } catch (const std::exception &e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }

    return 0;
}