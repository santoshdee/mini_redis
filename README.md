# MINI REDIS (In-Memory Key-Value Store in C++)

## Overview
Mini Redis is a Redis-inspired, in-memory key-value store implemented from scratch in C++ using Linux sockets and multi-threading.

The project focuses on understanding how real-world systems like Redis handle networking, concurrency, data storage,TTL expiration, and persistance at a low level.

Each connected client is handled independently with:
  * Isolated in-memory storage
  * Background TTL cleanup
  * Per-client persistance using JSON snapshots

## Key Features
* **Multi-client support using Linux sockets**
  * Each client connection runs in its own thread
  * Supports multiple concurrent clients
* **Per-client isolated storage**
  * Each client has its own in-memory database
  * No data leakage between clients
* **Type-safe key-value storage**
  * Supports *int*, *double*, *string* and *bool*
  * Implemented using *std::variant*
* **TTL and key expiration**
  * Supports *EXPIRE* command
  * Background cleaner thread removes expired keys safely
* **Pesistence using JSON**
  * Automatic load on client connect
  * Automatic save on client disconnect
  * Manual *SAVE* and *LOAD* commands supoorted
  * Stored per client under `data\client_<socket>/`
* **Redis-like command interface**
  * Simple text-based protocol usable via `telnet` or `nc`
 
## Supported Commands
| Command | Syntax | Description |
|--------|--------|-------------|
| SET | `SET <key> <value> [ttl]` | Sets a key with the given value and an optional TTL (in seconds) |
| GET | `GET <key>` | Retrieves the value associated with a key |
| DEL | `DEL <key>` | Deletes a key from the store |
| EXISTS | `EXISTS <key>` | Checks whether a key exists |
| EXPIRE | `EXPIRE <key> <ttl>` | Sets an expiration time (TTL in seconds) on a key |
| SHOW / DISPLAY | `SHOW` / `DISPLAY` | Displays all key-value pairs in the client’s store |
| SAVE | `SAVE <filename>` | Saves the client’s data to a JSON file (per-client persistence) |
| LOAD | `LOAD <filename>` | Loads the client’s data from a JSON file |
| EXIT / QUIT | `EXIT` / `QUIT` | Disconnects the client from the server |

## How to Build and Run (Linux/WSL)
**1. Prerequisites**
   * Linux
   * `g++` (C++17 or later)
   * `make` (optional)
   * `nlohmann/json` (header-only)
Install JSON:
```bash
sudo apt update
sudo apt install nlohmann-json3-dev
```
**2. Compile the project**
```bash
g++ -std=c++17 \
    -Iinclude \
    src/main.cpp \
    src/server.cppg \
    src/storage.cpp \
    src/command_parser.cpp \
    -pthread \
    -o mini_redis
```
**3. Run the server**
```bash
./mini_redis
```
* you should see: Server running on port 6379.

**4. Connect a client**
  * Using telnet: `telnet localhost 6379`
  * or using netcat: `nc localhost 6379`

**5. Persistence behaviour**
  * On client connect -> previous data is automaticall loaded (if exists)
  * On client diconnect -> data is automatically save to: `data/client_<socket>/autosave.json`
