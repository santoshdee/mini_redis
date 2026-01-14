# MINI_REDIS

## Phase 1

## Phase 2

### Expiry support:
* When setting a key, user can optionally specify a TTL(in seconds).
* After TTL expires, key should be considered invalid.

### Auto-cleanup:
* A background thread should periodically remove expired keys.
* Prevent memory from growing indefinitely.

### Extending the KV store to support multiple data types.
* Right now it is limited to `std::string`, but we can leverage C++17's `std::variant` to store different types in a single map.
* Now, using `std::variant` we could support:
    * std::string -> textual values
    * int -> integer numbers
    * double -> floating point numbers
    * bool -> true/false
* Later can be extended to support more complex data types like arrays or maps.

### Before going to phase 3, Add a method to update TTL like redis.
* In phase 2, we already support TTL when you call `set(key, value, ttl)`.
* Background cleaner deletes expired keys automatically.
* But there's a gap:
    * Once a key is created without TTL, you can't later attach an expiry to it.
    * Or if you want to update the TTL of an existing key, you can't do that either.
* Redis solves this with the `EXPIRE` command:
```bash
SET foo bar
EXPIRE foo 10   # foo will expire in 10 seconds
```
#### Why add `expire()` method now (before phase 3)
* **Command Completeness**: When we build the command parser in the next phase, we'll want to support a Redis-like command set (`SET`, `GET`, `DEL`, `EXPIRE`).
* **Cleaner API**: Instead of forcing sers to delete & re-set with TTL, we provide a direct way to attach/update expiry.
* **Consistency with Redis**: anyone familiar with Redis will expect `EXPIRE` support.
* **Simple to add now**:
    * It's just updating the `ValueEntry` metadata for a key.
    * The background cleaner thread already handles cleanup.

#### Method
`bool expire(const std::string &key, int ttl_secs);`
* returns true if key exists and ttl was set
* returns false if key doesn't exist.

### Testing upto phase 2, using <cassert> and CTest
* With <cassert>, everything is already in the C++ standard library. It is simple, lightweight and no framework
* We can use **GoogleTest(gtest)** - the JUnit eqv. for C++. But skipping that for now.


## Phase 3: Command parsing (REPL/Server commands)
* Parsing is trickier than it looks - spaces in keys. values, numbers vs strings, quotes, etc.
* Read a line of user input (like Redis CLI)
* Splits into command + arguments
* Handles quoted keys/values correctly (so "hello world" becomes one token, not two)
* Dispatches to storage methods
* Run a REPL loop until the user exits

### Command syntax we need to support
```bash
SET "ns" "n s"
SET "a b" "c"
SET x 42
SET pi 3.14
SET flag true
GET "ns"
DEL "a b"
EXISTS "ns"
EXPIRE x 10
```
* Tokens may be quoted -> "a b" means a single token
* Numbers should parse correctly (int, double)
* Boolean literals should be parsed correctly (true, false)
* Everything else defaults to string

### Implementation 
* Tokenizer
    * split the input into tokens, respecting quotes
    ```bash
    input: SET "a b" "c d"
    tokens: [SET] [a b] [c d]
    ```
* Command dispatcher
    * first token = command
    * remaining tokens = arguments
    * call the right storage api
* Printer
    * show user friendly output (OK, 1, 0, (nil) etc.)


## Phase 4: Networking - POSIX sockets and Persistence

* Each client connected spawns a new thread - multi-threading in action
* The command parser built in the last phase is used to parse the commands and do the correct operations over the network

* Right now in our project:
    * All clients share one global store
    * No persistence
    * No DB isolation

### Now the plan is to take it further

### 1. Client Isolation

* Each connected client will have its own `Storage` instance (DB)
* When a client connects -> `Server` creates a new `Storage` object and assigns it to that connection's handler thread
* Thus clients can't access or modify each other's data

* **Design changes**
    * `Server` no longer holds a single `Storage` or `CommandParser`
    * For each client connection (`handle_client`):
        * `Storage client_store;`
        * `CommandParser client_parser(client_store);`
    * No change in client command syntax - everything works as before

### 2. Persistence

* Add persistence for each client's DB:
    * Option 1: snapshot-style persistence (`SAVE` command -> writes the entire store to the disk)
    * Option 2: Append-only file persistence (like Redis AOF)
* Each client could have its own persistence file (ex: `client_<id>.rdb`)
* Server reloads it on reconnect (optional)

* Each client's database (their `Storage`) is dumped into a single JSON file(ex: `client_<id>.json`)

* Using `nlohmann/json` for header-only, super easy

### 3. Later Extension (optional)

* Let clients create multiple DBs and switch using commands like `CREATE_DB, USE_DB <name>`
* That will require a session-level DB manager — we can design that cleanly once the base isolation and persistence are done