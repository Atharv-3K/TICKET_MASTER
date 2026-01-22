#pragma once
#include "../db.h"
#include "../redis_manager.h"
#include <vector>
#include <crow/json.h>

struct Show {
    int id;
    std::string movie_name;
    std::string start_time;
    double price;
};

class CatalogDAO {
public:
    static std::vector<Show> getShows(int theater_id) {
        auto* redis = RedisManager::GetInstance();
        std::string cache_key = "shows:theater:" + std::to_string(theater_id);

        // 1. FAST PATH: Check Redis
        auto cached = redis->getSession(cache_key);
        if (cached) {
            std::cout << "🚀 Cache Hit! Serving from Redis." << std::endl;
            // In real world, we'd parse this JSON string back to objects.
            // For now, we return empty vector to signal "Use the string directly" 
            // (Simplified for this C++ example)
            return {}; 
        }

        // 2. SLOW PATH: Check DB
        std::cout << "🐌 Cache Miss. Hitting Postgres..." << std::endl;
        std::vector<Show> shows;
        try {
            DBConnection conn;
            pqxx::work txn(*conn);
            // Complex Join: Show -> Movie
            pqxx::result res = txn.exec_params(
                "SELECT s.id, m.title, s.start_time, s.price_standard "
                "FROM shows s JOIN movies m ON s.movie_id = m.id "
                "WHERE s.screen_id IN (SELECT id FROM screens WHERE theater_id = $1)",
                theater_id
            );

            crow::json::wvalue json_arr;
            int i = 0;
            for (auto row : res) {
                shows.push_back({
                    row[0].as<int>(), row[1].as<std::string>(), 
                    row[2].as<std::string>(), row[3].as<double>()
                });
                // Build JSON for cache
                json_arr[i]["id"] = row[0].as<int>();
                json_arr[i]["movie"] = row[1].as<std::string>();
                json_arr[i]["time"] = row[2].as<std::string>();
                json_arr[i]["price"] = row[3].as<double>();
                i++;
            }

            // 3. WRITE BACK TO CACHE (TTL 5 mins)
            redis->setSession(cache_key, json_arr.dump(), 300);

        } catch (const std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
        }
        return shows;
    }
};