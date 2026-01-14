#pragma once

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include "storage.h"
#include "command_parser.h"

class Server {
private:
    int port_;
    int server_sock_;
    std::atomic<bool> running_;

    std::vector<std::thread> client_threads_;

    void accept_clients();                  // Main loop for accepting clients
    void handle_client(int client_sock);    // Handle a single client

public:
    Server(int port);
    ~Server();

    void start();       // Start server
    void stop();        // Stop server gracefully
};
