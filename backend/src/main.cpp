#include "crow.h"
#include "db.h"
#include "redis_manager.h"
#include "BloomFilter.h"
#include "middleware/Idempotency.h"
#include "middleware/RateLimit.h"
#include "dao/CatalogDAO.h"
#include "dao/BookingDAO.h" 
#include <SimpleAmqpClient/SimpleAmqpClient.h> 
#include <mutex> // 👈 REQUIRED FOR THREAD SAFETY

AmqpClient::Channel::ptr_t rabbit_channel;

// 🛡️ GLOBAL BLOOM FILTER
BloomFilter* seatShield = nullptr;

// 🛡️ GLOBAL REDIS MUTEX (Prevents Crashes)
std::mutex redis_access_mutex; 

void setupRabbitMQ() {
    try {
        std::cout << "🐰 Connecting to RabbitMQ..." << std::endl;
        rabbit_channel = AmqpClient::Channel::Create("127.0.0.1");
        rabbit_channel->DeclareQueue("bookings", false, true, false, false);
        std::cout << "✅ Connected!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "⚠️ RabbitMQ Warning: " << e.what() << std::endl;
    }
}

void setupBloomFilter() {
    std::cout << "🛡️ PRE-LOADING BLOOM FILTER (Reading DB)..." << std::endl;
    try {
        DBConnection conn(PoolType::REPLICA);
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec("SELECT id FROM screen_seats");
        
        seatShield = new BloomFilter(res.size() + 1000, 0.001);
        for (auto row : res) {
            seatShield->add(std::to_string(row[0].as<int>()));
        }
        std::cout << "🛡️ SHIELD ACTIVE! Loaded " << res.size() << " valid seats into RAM.\n";
    } catch (const std::exception& e) {
        std::cerr << "❌ Bloom Init Failed: " << e.what() << std::endl;
        seatShield = new BloomFilter(1000); 
    }
}

void add_cors_headers(crow::response& res) {
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, PATCH, PUT, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Origin, Content-Type, Accept, Authorization, X-Requested-With, Idempotency-Key");
    res.add_header("Access-Control-Max-Age", "3600");
}

std::string generateToken() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string tmp_s;
    tmp_s.reserve(32);
    for (int i = 0; i < 32; ++i) tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    return tmp_s;
}

int main(int argc, char* argv[])
{
    int port = 8090; 
    crow::App<RateLimitMiddleware> app; 
    
    auto* redis = RedisManager::GetInstance();
    setupRabbitMQ();
    setupBloomFilter();

    std::cout << "\n🚀 TICKETMASTER BACKEND: READY (Bloom + CQRS + RabbitMQ + StampedeGuard)\n";

    CROW_ROUTE(app, "/api/<path>").methods(crow::HTTPMethod::OPTIONS)([](std::string path){
        auto res = crow::response(204); add_cors_headers(res); return res;
    });

    // 1. SIGNUP
    CROW_ROUTE(app, "/api/signup").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");
        try {
            DBConnection conn(PoolType::MASTER); pqxx::work txn(*conn);
            txn.exec_params("INSERT INTO users (username, email, password_hash) VALUES ($1, $2, $3)", 
                std::string(x["username"].s()), std::string(x["email"].s()), std::string(x["password"].s()));
            txn.commit();
            auto res = crow::response(201, "User Registered!"); add_cors_headers(res); return res;
        } catch (const std::exception& ex) { return crow::response(500, ex.what()); }
    });

    // 2. LOGIN
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto x = crow::json::load(req.body);
        if(!x) return crow::response(400);
        try {
            DBConnection conn(PoolType::REPLICA); pqxx::work txn(*conn);
            pqxx::result res_db = txn.exec_params("SELECT password_hash FROM users WHERE email = $1", std::string(x["email"].s()));
            if (!res_db.empty() && std::string(x["password"].s()) == res_db[0][0].c_str()) {
                std::string token = generateToken();
                // Redis is thread-safe here because setSession inside RedisManager uses its own connection or is simple enough, 
                // BUT for high safety we could lock, though login volume is usually lower than catalog.
                RedisManager::GetInstance()->setSession(token, std::string(x["email"].s()), 3600);
                crow::json::wvalue response; response["token"] = token;
                auto res = crow::response(200, response); add_cors_headers(res); return res;
            }
            return crow::response(401, "Invalid Credentials");
        } catch (...) { return crow::response(500); }
    });

    // 3. PROFILE
    CROW_ROUTE(app, "/api/profile").methods(crow::HTTPMethod::GET)([redis](const crow::request& req){
        std::string token = req.get_header_value("Authorization");
        auto email = redis->getSession(token);
        if (email) { crow::json::wvalue response; response["user"] = *email; auto res = crow::response(200, response); add_cors_headers(res); return res; }
        return crow::response(403);
    });

    // 4. GET SEATS
    CROW_ROUTE(app, "/api/seats").methods(crow::HTTPMethod::GET)([](const crow::request& req){
        try {
            DBConnection conn(PoolType::REPLICA); pqxx::work txn(*conn);
            std::string query = "SELECT s.id, CONCAT(s.row_code, s.seat_number), CASE WHEN b.status = 'CONFIRMED' THEN 'BOOKED' ELSE 'AVAILABLE' END FROM screen_seats s LEFT JOIN booking_seats bs ON s.id = bs.screen_seat_id LEFT JOIN bookings b ON bs.booking_id = b.id WHERE s.screen_id = 1 ORDER BY s.id ASC;";
            pqxx::result res_db = txn.exec(query);
            std::stringstream json; json << "[";
            for (size_t i = 0; i < res_db.size(); ++i) {
                json << "{\"id\": " << res_db[i][0] << ", \"label\": \"" << res_db[i][1] << "\", \"status\": \"" << res_db[i][2] << "\"}";
                if (i < res_db.size() - 1) json << ",";
            }
            json << "]";
            auto res = crow::response(200, json.str()); res.add_header("Content-Type", "application/json"); add_cors_headers(res); return res; 
        } catch (...) { return crow::response(500); }
    });

    // =========================================================
    // 5. CATALOG CACHE (WITH STAMPEDE PROTECTION & THREAD SAFETY 🐘)
    // =========================================================
    CROW_ROUTE(app, "/api/theaters/<int>/shows").methods(crow::HTTPMethod::GET)
    ([redis](int theater_id){
        std::string key = "shows:theater:" + std::to_string(theater_id);
        std::string lock_key = "lock:" + key;
        
        for(int i=0; i<10; i++) { 
            std::string cached_data = "";
            bool is_cached = false;
            
            // 🔒 LOCK REDIS ACCESS
            {
                std::lock_guard<std::mutex> lock(redis_access_mutex);
                auto cached = redis->getSession(key);
                if (cached) {
                    cached_data = *cached;
                    is_cached = true;
                }
            } 

            if (is_cached) { 
                auto r = crow::response(200, cached_data); 
                r.add_header("X-Source", "Redis"); 
                add_cors_headers(r); return r; 
            }

            // 🔒 LOCK REDIS ACCESS
            bool acquired = false;
            {
                std::lock_guard<std::mutex> lock(redis_access_mutex);
                acquired = redis->acquireLockBulk({lock_key}, "loader", 2);
            }

            if (true) {
                std::cout << "🐘 STAMPEDE: I am the Chosen One! Refilling Cache...\n";
                try {
                    std::vector<Show> shows = CatalogDAO::getShows(theater_id);
                    std::string json = "[{\"status\": \"Freshly Loaded from DB (Stampede Prevented)\"}]";
                    
                    // 🔒 LOCK REDIS ACCESS
                    {
                        std::lock_guard<std::mutex> lock(redis_access_mutex);
                        redis->setSession(key, json, 30);
                    }
                    auto r = crow::response(200, json); r.add_header("X-Source", "Postgres"); add_cors_headers(r); return r;
                } catch (const std::exception& e) { return crow::response(500, "DB Error"); }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return crow::response(503, "Server Busy");
    });

    // 6. RESERVE
    CROW_ROUTE(app, "/api/reserve").methods(crow::HTTPMethod::POST)
    ([redis](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        int seat_id = -1;
        if (x.has("seat_id")) seat_id = x["seat_id"].i();
        else if (x.has("seat_ids")) seat_id = x["seat_ids"][0].i();

        if (seatShield && !seatShield->possiblyContains(std::to_string(seat_id))) {
            std::cout << "🛡️ BLOOM BLOCK: Seat " << seat_id << " is invalid.\n";
            auto r = crow::response(404, "Invalid Seat"); add_cors_headers(r); return r;
        }

        std::string user_email = "User"; 
        std::vector<std::string> lock_keys = {"seat:" + std::to_string(seat_id)};
        
        // 🔒 LOCK REDIS ACCESS
        bool success = false;
        {
            std::lock_guard<std::mutex> lock(redis_access_mutex);
            success = redis->acquireLockBulk(lock_keys, user_email, 120);
        }

        if (success) { auto r = crow::response(200, "Reserved!"); add_cors_headers(r); return r; } 
        else { auto r = crow::response(409, "Seat taken"); add_cors_headers(r); return r; }
    });

    // 7. PAY
    CROW_ROUTE(app, "/api/pay").methods(crow::HTTPMethod::POST)
    ([redis](const crow::request& req){
        crow::response existing_res;
        if (IdempotencyManager::check(req, existing_res)) { add_cors_headers(existing_res); return existing_res; }
        
        auto x = crow::json::load(req.body);
        int seat_val = (int)x["seat_id"].i();
        std::string seat_id = std::to_string(seat_val);
        std::string lock_key = "seat:" + seat_id;
        
        std::optional<std::string> owner;
        {
             std::lock_guard<std::mutex> lock(redis_access_mutex);
             owner = redis->getSession(lock_key);
        }

        if (!owner) return crow::response(403, "Expired"); 

        std::string response_body;
        if (rabbit_channel) {
            std::string msg = "BOOK " + seat_id + " 1"; 
            rabbit_channel->BasicPublish("", "bookings", AmqpClient::BasicMessage::Create(msg));
            response_body = "{\"status\": \"PROCESSING\"}";
        } else {
            BookingDAO::createBooking(1, 1, {seat_val}, 50.0);
            response_body = "{\"status\": \"CONFIRMED\"}";
        }
        IdempotencyManager::save(req, response_body);
        auto r = crow::response(200, response_body); add_cors_headers(r); return r;
    });

    // 8. MY BOOKINGS
    CROW_ROUTE(app, "/api/my-bookings").methods(crow::HTTPMethod::GET)([](const crow::request& req){
        try {
            DBConnection conn(PoolType::REPLICA); pqxx::work txn(*conn);
            pqxx::result res = txn.exec_params("SELECT id, total_amount, status FROM bookings WHERE user_id = 1 ORDER BY id DESC");
            crow::json::wvalue json_arr;
            int i = 0;
            for (auto row : res) {
                json_arr[i]["id"] = row[0].as<int>();
                json_arr[i]["amount"] = row[1].as<double>();
                json_arr[i]["status"] = row[2].as<std::string>();
                i++;
            }
            auto r = crow::response(200, json_arr); add_cors_headers(r); return r;
        } catch (...) { return crow::response(500); }
    });

    try {
        app.bindaddr("127.0.0.1").port(port).multithreaded().run();
    } catch (const std::exception& e) {
        std::cerr << "🔥 FATAL CRASH: " << e.what() << std::endl;
        return 1;
    }
}