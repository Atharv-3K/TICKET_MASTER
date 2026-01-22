#pragma once
#include "../redis_manager.h"
#include "crow.h"

// This helper checks if a request has already been processed
class IdempotencyManager {
public:
    static bool check(const crow::request& req, crow::response& existing_res) {
        std::string key = req.get_header_value("Idempotency-Key");
        if (key.empty()) return false; // No key, process normally

        auto* redis = RedisManager::GetInstance();
        auto cached = redis->getSession("idempotency:" + key);
        
        if (cached) {
            // Found a duplicate request! Return the cached response.
            existing_res.code = 200; // Simplified
            existing_res.body = *cached;
            existing_res.add_header("X-Idempotency-Hit", "true");
            return true; // Stop processing
        }
        return false;
    }

    static void save(const crow::request& req, const std::string& response_body) {
        std::string key = req.get_header_value("Idempotency-Key");
        if (!key.empty()) {
            // Save result for 24 hours
            RedisManager::GetInstance()->setSession("idempotency:" + key, response_body, 86400);
        }
    }
};