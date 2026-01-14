/*
This test file contains all the tests for storage class and covers the following:

set and get
overwrite behaviour
delete
exists
size
TTL auto-expiry
manual expire() method
*/

#include "../include/storage.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <variant>
#include <vector>

// helper to get string from variant
std::string asString(const Storage::Value &val) {
    if(std::holds_alternative<std::string>(val)) return std::get<std::string>(val);
    if(std::holds_alternative<int>(val)) return std::to_string(std::get<int>(val));
    if (std::holds_alternative<double>(val)) return std::to_string(std::get<double>(val));
    if (std::holds_alternative<bool>(val)) return std::get<bool>(val) ? "true" : "false";
    return "<unknown>";
}

void test_set_and_get() {
    Storage store;
    store.set("a", "ashlee deanna");
    auto val = store.get("a");
    assert(val.has_value());
    assert(std::get<std::string>(*val) == "ashlee deanna");
}

void test_all_types() {
    Storage store;
    store.set("int_key", 18);
    store.set("double_key", 3.1415);
    store.set("string_key", std::string("mini redis"));
    store.set("bool_key", true);

    auto int_val = store.get("int_key");
    auto dbl_val = store.get("double_key");
    auto str_val = store.get("string_key");
    auto bool_val = store.get("bool_key");

    assert(int_val && std::get<int>(*int_val) == 18);
    assert(dbl_val && std::get<double>(*dbl_val) == 3.1415);
    assert(str_val && std::get<std::string>(*str_val) == "mini redis");
    assert(bool_val && std::get<bool>(*bool_val) == true);
}

void test_overwrite() {
    Storage store;
    store.set("key", std::string("first"));
    store.set("key", std::string("second"));
    auto val = store.get("key");
    assert(val.has_value());
    assert(std::get<std::string>(*val) == "second");
}

void test_delete() {
    Storage store;
    store.set("k", true);
    assert(store.del("k") == true);
    assert(!store.exists("k"));
}

void test_exists() {
    Storage store;
    store.set("x", 3.14);
    assert(store.exists("x"));
    assert(!store.exists("missing"));
}

void test_size() {
    Storage store;
    store.set("a", 1);
    store.set("b", 2);
    assert(store.size() == 2);
}

void test_ttl_expiry() {
    Storage store;
    store.set("temp", "hello", 1); // TTL = 1sec
    assert(store.exists("temp"));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    assert(!store.exists("temp"));
    auto val = store.get("temp");
    assert(!val.has_value());
    assert(store.size() == 0); // check internal map size decreased
}

void test_expire_method() {
    Storage store;
    store.set("key", "value");
    bool ok = store.expire("key", 1); // add TTL later
    assert(ok == true);
    assert(store.exists("key"));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    assert(!store.exists("key"));
}

void test_keys_with_spaces() {
    Storage store;
    store.set("ns", "n s");
    store.set("a b", "c");
    store.set("space key", "space value");

    assert(std::get<std::string>(*store.get("ns")) == "n s");
    assert(std::get<std::string>(*store.get("a b")) == "c");
    assert(std::get<std::string>(*store.get("space key")) == "space value");
}

void test_edge_cases() {
    Storage store;
    // expire non-existent key
    assert(!store.expire("missing", 5));
    // delete non-existent key
    assert(!store.del("missing"));
    // zero or negative TTL
    store.set("zero", "val", 0);
    store.set("negative", "val", -1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    assert(!store.exists("zero") && !store.exists("negative"));
}

void test_dump() {
    Storage store;
    store.set("int_key", 42);
    store.set("double_key", 3.14);
    store.set("string_key", "hello");
    store.set("bool_key", true);

    auto snapshot = store.dump();
    for (auto &kv : snapshot) {
        std::cout << kv.first << " => " << asString(kv.second) << "\n";
    }
    assert(snapshot.size() == 4);
}

void test_concurrency() {
    Storage store;
    const int N = 1000;
    std::vector<std::thread> threads;

    for(int i=0; i<5; i++) {
        threads.emplace_back([&store, N, i]() {
            for(int j=0; j<N; j++) {
                store.set("k" + std::to_string(i*N + j), i*N + j);
            }
        });
    }

    for(int i=0; i<5; i++) {
        threads.emplace_back([&store, N, i]() {
            for(int j=0; j<N; j++) {
                store.get("k" + std::to_string(i*N + j));
            }
        });
    }

    for(auto& thread: threads) thread.join();
    assert(store.size() == 5*N);
}

int main() {
    test_set_and_get();
    test_all_types();
    test_overwrite();
    test_delete();
    test_exists();
    test_size();
    test_ttl_expiry();
    test_expire_method();
    test_keys_with_spaces();
    test_edge_cases();
    test_dump();
    test_concurrency();

    return 0;
}