cmake_minimum_required(VERSION 3.10)

project(mini_redis VERSION 0.1 LANGUAGES CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project dirs
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")

# Collect sources conditionally so CMake won't fail if some files don't exist yet.
set(SOURCES)
list(APPEND SOURCES "${SRC_DIR}/main.cpp")

if(EXISTS "${SRC_DIR}/storage.cpp")
  list(APPEND SOURCES "${SRC_DIR}/storage.cpp")
endif()

if(EXISTS "${SRC_DIR}/server.cpp")
  list(APPEND SOURCES "${SRC_DIR}/server.cpp")
endif()

if(EXISTS "${SRC_DIR}/command_parser.cpp")
  list(APPEND SOURCES "${SRC_DIR}/command_parser.cpp")
endif()

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include headers from include/
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIR})

# Compiler warning flags (cross-platform)
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------
# Testing Section
# ---------------------------
enable_testing()

set(TEST_DIR "${PROJECT_SOURCE_DIR}/tests")

if(EXISTS "${TEST_DIR}/storage_tests.cpp")
    add_executable(storage_tests
        ${TEST_DIR}/storage_tests.cpp
        ${SRC_DIR}/storage.cpp
    )
    target_include_directories(storage_tests PRIVATE ${INCLUDE_DIR})

    add_test(NAME StorageSetGet    COMMAND storage_tests set_get)
    add_test(NAME StorageAllTypes  COMMAND storage_tests all_types)
    add_test(NAME StorageOverwrite COMMAND storage_tests overwrite)
    add_test(NAME StorageDelete    COMMAND storage_tests delete)
    add_test(NAME StorageExists    COMMAND storage_tests exists)
    add_test(NAME StorageSize      COMMAND storage_tests size)
    add_test(NAME StorageTTL       COMMAND storage_tests ttl)
    add_test(NAME StorageExpire    COMMAND storage_tests expire)
    add_test(NAME StorageKeysWithSpaces    COMMAND storage_tests keys_with_spaces)
    add_test(NAME StorageEdgecases   COMMAND storage_tests edge_cases)
    add_test(NAME StorageDump        COMMAND storage_tests dump)
    add_test(NAME StorageConcurrency COMMAND storage_tests concurrency)
endif()
