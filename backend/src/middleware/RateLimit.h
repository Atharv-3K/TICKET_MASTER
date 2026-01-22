#pragma once
#include "../redis_manager.h"
#include "crow.h"
#include <iostream>

class RateLimitMiddleware {
public:
    struct context {}; // Required by Crow

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        std::string ip = req.remote_ip_address;
        
        // ⚙️ PRODUCTION SETTINGS:
        // Allow 10 requests per 1 second.
        // This stops bots but allows normal users to click fast.
        int limit = 10;
        int window = 1;

        bool allowed = RedisManager::GetInstance()->checkRateLimit(ip, limit, window);

        if (!allowed) {
            std::cout << "🛡️ [RateLimit] Blocking IP: " << ip << std::endl;
            res.code = 429;
            res.body = "{\"error\": \"Too Many Requests - Please wait\"}";
            res.end();
        }
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        // No action needed after request
    }
};