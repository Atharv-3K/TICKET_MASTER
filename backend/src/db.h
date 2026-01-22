#pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iostream>

// 🏭 THE CONNECTION POOL
// Keeps N connections open and recycles them.
class DBPool {
private:
    std::queue<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    const int POOL_SIZE = 10;
    const std::string conn_str = "postgresql://postgres:password123@localhost:5432/ticketmaster";

    // Singleton Stuff
    static DBPool* instance;
    DBPool() {
        for (int i = 0; i < POOL_SIZE; ++i) {
            try {
                auto conn = std::make_shared<pqxx::connection>(conn_str);
                if (conn->is_open()) {
                    pool_.push(conn);
                }
            } catch (const std::exception& e) {
                std::cerr << "❌ Pool Init Error: " << e.what() << std::endl;
            }
        }
        std::cout << "💧 DB Pool Initialized with " << pool_.size() << " connections." << std::endl;
    }

public:
    static DBPool* GetInstance() {
        if (!instance) instance = new DBPool();
        return instance;
    }

    // Borrow a connection
    std::shared_ptr<pqxx::connection> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until a connection is available
        cv_.wait(lock, [this] { return !pool_.empty(); });

        auto conn = pool_.front();
        pool_.pop();
        return conn;
    }

    // Return it
    void release(std::shared_ptr<pqxx::connection> conn) {
        std::unique_lock<std::mutex> lock(mutex_);
        pool_.push(conn);
        lock.unlock();
        cv_.notify_one();
    }
};

DBPool* DBPool::instance = nullptr;

// 🪄 RAII WRAPPER
// Automatically releases connection when it goes out of scope.
class DBConnection {
    std::shared_ptr<pqxx::connection> conn_;
public:
    DBConnection() {
        conn_ = DBPool::GetInstance()->acquire();
    }
    ~DBConnection() {
        DBPool::GetInstance()->release(conn_);
    }
    pqxx::connection& operator*() { return *conn_; }
    pqxx::connection* operator->() { return conn_.get(); }
};