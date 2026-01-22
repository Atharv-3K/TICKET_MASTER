#include "crow.h"
#include "db.h"
#include "redis_manager.h"
#include "BloomFilter.h" // ✅ NEW: The Shield
#include "middleware/Idempotency.h"
#include "middleware/RateLimit.h" // ✅ OLD: Rate Limiter
#include "dao/CatalogDAO.h"
#include "dao/BookingDAO.h" 
#include <SimpleAmqpClient/SimpleAmqpClient.h> 

AmqpClient::Channel::ptr_t rabbit_channel;

// 🛡️ GLOBAL BLOOM FILTER (The Shield)
BloomFilter* seatShield = nullptr;

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

// 🛡️ INITIALIZE THE SHIELD (Uses CQRS Read Pool)
void setupBloomFilter() {
    std::cout << "🛡️ PRE-LOADING BLOOM FILTER (Reading DB)..." << std::endl;
    try {
        // ✅ CQRS: Using REPLICA pool for reading seats
        DBConnection conn(PoolType::REPLICA);
        pqxx::work txn(*conn);
        
        pqxx::result res = txn.exec("SELECT id FROM screen_seats");
        
        seatShield = new BloomFilter(res.size() + 1000, 0.001);

        for (auto row : res) {
            std::string seat_id_str = std::to_string(row[0].as<int>());
            seatShield->add(seat_id_str);
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
    
    // ✅ MIDDLEWARE: Rate Limiter is still here!
    crow::App<RateLimitMiddleware> app; 
    
    auto* redis = RedisManager::GetInstance();
    setupRabbitMQ();
    setupBloomFilter(); // Load the shield

    std::cout << "\n🚀 TICKETMASTER BACKEND: READY (Bloom + CQRS + RabbitMQ)\n";

    CROW_ROUTE(app, "/api/<path>").methods(crow::HTTPMethod::OPTIONS)([](std::string path){
        auto res = crow::response(204); add_cors_headers(res); return res;
    });

    // 1. SIGNUP
    CROW_ROUTE(app, "/api/signup").methods(crow::HTTPMethod::POST)([](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");
        try {
            // ✅ CQRS: Using MASTER pool for Writing
            DBConnection conn(PoolType::MASTER); 
            pqxx::work txn(*conn);
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
            // ✅ CQRS: Using REPLICA pool for Reading
            DBConnection conn(PoolType::REPLICA); 
            pqxx::work txn(*conn);
            pqxx::result res_db = txn.exec_params("SELECT password_hash FROM users WHERE email = $1", std::string(x["email"].s()));
            if (!res_db.empty() && std::string(x["password"].s()) == res_db[0][0].c_str()) {
                std::string token = generateToken();
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
            // ✅ CQRS: Using REPLICA pool for Reading
            DBConnection conn(PoolType::REPLICA); 
            pqxx::work txn(*conn);
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

    // 5. CATALOG CACHE
    CROW_ROUTE(app, "/api/theaters/<int>/shows").methods(crow::HTTPMethod::GET)
    ([redis](int theater_id){
        auto cached = redis->getSession("shows:theater:" + std::to_string(theater_id));
        if (cached) { auto r = crow::response(200, *cached); r.add_header("X-Source", "Redis"); add_cors_headers(r); return r; }
        std::vector<Show> shows = CatalogDAO::getShows(theater_id);
        auto r = crow::response(200, "[{\"status\": \"Loaded from DB into Cache\"}]"); add_cors_headers(r); return r;
    });

    // =========================================================
    // 6. ATOMIC RESERVE (WITH BLOOM FILTER PROTECTION 🛡️)
    // =========================================================
    CROW_ROUTE(app, "/api/reserve").methods(crow::HTTPMethod::POST)
    ([redis](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        int seat_id = -1;
        if (x.has("seat_id")) seat_id = x["seat_id"].i();
        else if (x.has("seat_ids")) seat_id = x["seat_ids"][0].i();

        // 🛡️ STEP 1: BLOOM FILTER CHECK (The "Taylor Swift" Defense)
        // If the Bloom Filter says "No", we REJECT INSTANTLY.
        if (seatShield && !seatShield->possiblyContains(std::to_string(seat_id))) {
            std::cout << "🛡️ BLOOM BLOCK: Seat " << seat_id << " is definitely invalid. Rejected in 0ms.\n";
            auto r = crow::response(404, "Invalid Seat (Blocked by Shield)"); 
            add_cors_headers(r); return r;
        }

        // 🛡️ STEP 2: Redis Lock (Existing Logic)
        std::string user_email = "User"; 
        std::vector<std::string> lock_keys = {"seat:" + std::to_string(seat_id)};
        
        bool success = redis->acquireLockBulk(lock_keys, user_email, 120);
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
        auto owner = redis->getSession(lock_key);
        if (!owner) return crow::response(403, "Expired"); 

        std::string response_body;
        if (rabbit_channel) {
            std::string msg = "BOOK " + seat_id + " 1"; 
            rabbit_channel->BasicPublish("", "bookings", AmqpClient::BasicMessage::Create(msg));
            response_body = "{\"status\": \"PROCESSING\"}";
        } else {
            // ✅ DAO internally uses MASTER pool
            BookingDAO::createBooking(1, 1, {seat_val}, 50.0);
            response_body = "{\"status\": \"CONFIRMED\"}";
        }
        IdempotencyManager::save(req, response_body);
        auto r = crow::response(200, response_body); add_cors_headers(r); return r;
    });

    // 8. MY BOOKINGS
    CROW_ROUTE(app, "/api/my-bookings").methods(crow::HTTPMethod::GET)([](const crow::request& req){
        try {
            // ✅ CQRS: Using REPLICA pool for Reading
            DBConnection conn(PoolType::REPLICA); 
            pqxx::work txn(*conn);
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

    std::cout << "\n🚀 COMPLETE SERVER RUNNING ON 8090 (PROD SETTINGS)\n";
    app.bindaddr("127.0.0.1").port(port).multithreaded().run();
}