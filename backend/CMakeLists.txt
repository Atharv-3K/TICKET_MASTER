cmake_minimum_required(VERSION 3.10)
project(server)

# 1. Force C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Windows VCPKG Setup
set(VCPKG_ROOT "C:/Users/athar/vcpkg/installed/x64-windows")
include_directories(${VCPKG_ROOT}/include)
link_directories(${VCPKG_ROOT}/lib)

message(STATUS "🖥️ Windows Build Configured. Using: ${VCPKG_ROOT}")

# 3. Find Packages
find_package(Crow CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)

# Manual Find for libraries without Config files
find_library(REDISPP_LIB NAMES redis++ libredis++ PATHS "${VCPKG_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(RABBITMQ_LIB NAMES SimpleAmqpClient.7 SimpleAmqpClient PATHS "${VCPKG_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)

# 4. Build & Link
add_executable(server src/main.cpp)

target_link_libraries(server PRIVATE 
    Crow::Crow 
    libpqxx::pqxx 
    hiredis::hiredis
    ${REDISPP_LIB}
    ${RABBITMQ_LIB}
    ws2_32 # Essential for Windows Networking
)

target_include_directories(server PRIVATE "${VCPKG_ROOT}/include")