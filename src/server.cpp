#include "server.h"
#include "../include/constants.h"
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <filesystem>

constexpr int BUFFER_SIZE = 1024;

Server::Server(int port)
    : port_(port), running_(false), server_sock_(-1) {}

Server::~Server() {
    stop();
}

void Server::start() {
    server_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_ < 0) {
        throw std::runtime_error("Error creating socket");
    }

    int opt = 1;
    setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_sock_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Bind failed");
    }

    if (listen(server_sock_, 5) < 0) {
        throw std::runtime_error("Listen failed");
    }

    running_ = true;
    std::cout << "Server running on port " << port_ << "...\n";

    accept_clients();

    for (auto &th : client_threads_) {
        if (th.joinable()) th.join();
    }

    std::cout << "Server stopped\n";
}

void Server::accept_clients() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock_, (struct sockaddr*)&client_addr, &client_len);

        if (!running_) break;

        if (client_sock < 0) {
            if (running_) std::cerr << "Failed to accept client.\n";
            continue;
        }

        std::cout << "Client connected.\n";
        client_threads_.emplace_back(&Server::handle_client, this, client_sock);
    }
}

void Server::handle_client(int client_sock) {
    // create isolated store + parser for this client
    Storage client_store;
    CommandParser client_parser(client_store, client_sock);

    // prepare client-specific directory: data/client_<sock>/
    const std::string clientDir = DATA_DIR + "/client_" + std::to_string(client_sock);
    std::error_code ec;
    std::filesystem::create_directories(clientDir, ec);
    if(ec) {
        std::cerr << "Warning: could not create directory '" << clientDir << "': " << ec.message() << "\n";
    }

    // auto-load previous session data (autosave.json) if it exists
    std::string autosavePath = clientDir + "/autosave.json";
    client_store.loadFromFile(autosavePath); // loadFromFile returns false if file missing

    const char* welcomeMsg =
        "\nWelcome to Mini Redis Server!\n"
        "--------------------------------------------\n"
        "Available Commands:\n"
        "SET <key> <value> <ttl>     -> Set key to value (optionally with TTL in seconds)\n"
        "GET <key>                   -> Get value of key\n"
        "DEL <key>                   -> Delete a key\n"
        "EXISTS <key>                -> Check if a key exists\n"
        "EXPIRE <key> <ttl>          -> Set expiry for a key\n"
        "SHOW / DISPLAY              -> Show all key-value pairs\n"
        "EXIT / QUIT                 -> Disconnect from server\n"
        "SAVE <filename>             -> Saves the data to a json file\n"
        "LOAD <filename>             -> loads the data from the json file\n"
        "--------------------------------------------\n\n";
    send(client_sock, welcomeMsg, strlen(welcomeMsg), 0);

    std::string command;
    char ch;

    while (true) {
        ssize_t n = recv(client_sock, &ch, 1, 0);
        if (n <= 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        if (ch == '\n') {
            if (!command.empty() && command.back() == '\r') command.pop_back();

            std::string upperCmd = command;
            std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::toupper);

            if (upperCmd == "EXIT" || upperCmd == "QUIT") {
                const char* goodbye = "Goodbye!\r\n";
                send(client_sock, goodbye, strlen(goodbye), 0);
                std::cout << "Client disconnected!\n";
                break;
            }

            if (!command.empty()) {
                std::string response = client_parser.execute(command);
                response += "\r\n";
                ssize_t total_sent = 0;
                while (total_sent < response.size()) {
                    ssize_t sent = send(client_sock, response.c_str() + total_sent, response.size() - total_sent, 0);
                    if (sent <= 0) break;
                    total_sent += sent;
                }
            }

            command.clear();
        } else {
            command.push_back(ch);
        }
    }

    // auto-save client db on disconnect to clientDir/autosave.json
    if(!std::filesystem::exists(clientDir)) {
        std::filesystem::create_directories(clientDir, ec);
    }

    if (!client_store.saveToFile(autosavePath)) {
        std::cerr << "Warning: failed to autosave client data to " << autosavePath << "\n";
    } else {
        std::cout << "Autosaved client data to " << autosavePath << "\n";
    }

    close(client_sock);
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    if (server_sock_ >= 0) close(server_sock_);
}
