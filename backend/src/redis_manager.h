#pragma once
#include <sw/redis++/redis++.h>
#include <iostream>
#include <mutex>
#include <vector>
#include <optional>

using namespace sw::redis;

class RedisManager {
private:
    static RedisManager* instance;
    static std::mutex mutex_;
    std::unique_ptr<Redis> redis;

    RedisManager() {
        try {
            ConnectionOptions connection_options;
            connection_options.host = "127.0.0.1"; 
            connection_options.port = 6379; 
            connection_options.socket_timeout = std::chrono::milliseconds(500); 
            redis = std::make_unique<Redis>(connection_options);
        } catch (const Error &e) {
            std::cerr << "❌ Redis Init Error: " << e.what() << std::endl;
        }
    }

public:
    static RedisManager* GetInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance == nullptr) instance = new RedisManager();
        return instance;
    }

    // 🧠 ATOMIC BULK LOCK
    bool acquireLockBulk(const std::vector<std::string>& keys, const std::string& user_id, int ttl_seconds) {
        std::string script = R"(
            for i, key in ipairs(KEYS) do
                if redis.call("EXISTS", key) == 1 then return 0 end
            end
            for i, key in ipairs(KEYS) do
                redis.call("SET", key, ARGV[1], "EX", ARGV[2])
            end
            return 1
        )";
        try {
            std::vector<std::string> args = {user_id, std::to_string(ttl_seconds)};
            auto res = redis->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
            return res == 1;
        } catch (...) { return false; }
    }

    // 🛡️ RATE LIMITER (Fixed Lua Syntax)
    bool checkRateLimit(const std::string& ip_address, int limit, int window_seconds) {
        std::string key = "ratelimit:" + ip_address;
        
        // 🛠️ FIX: Changed 'var' to 'local'
        std::string script = R"(
            local current = redis.call("INCR", KEYS[1])
            if current == 1 then
                redis.call("EXPIRE", KEYS[1], ARGV[1])
            end
            return current
        )";

        try {
            std::vector<std::string> keys = {key};
            std::vector<std::string> args = {std::to_string(window_seconds)};
            
            // Execute Script
            long long current_count = redis->eval<long long>(script, keys.begin(), keys.end(), args.begin(), args.end());
            
            // Debug Log
            std::cout << "   [Redis] IP: " << ip_address << " | Count: " << current_count << "/" << limit << std::endl;

            if (current_count > limit) return false;
            return true;
        } catch (const Error &e) { 
            std::cerr << "❌ REDIS LUA ERROR: " << e.what() << std::endl;
            return true; 
        }
    }

    void setSession(const std::string& key, const std::string& val, int ttl) {
        try { redis->set(key, val, std::chrono::seconds(ttl)); } catch (...) {}
    }
    std::optional<std::string> getSession(const std::string& key) {
        try { 
            auto val = redis->get(key);
            if (val) return *val;
        } catch (...) {}
        return std::nullopt;
    }
};

RedisManager* RedisManager::instance = nullptr;
std::mutex RedisManager::mutex_;