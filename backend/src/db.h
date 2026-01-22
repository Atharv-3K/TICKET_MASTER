#pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iostream>

// 🚦 TRAFFIC CONTROL: TWO POOLS
enum class PoolType {
    MASTER, // Writes (INSERT, UPDATE)
    REPLICA // Reads (SELECT)
};

class DBPool {
private:
    std::queue<std::shared_ptr<pqxx::connection>> master_pool_;
    std::queue<std::shared_ptr<pqxx::connection>> replica_pool_;
    
    std::mutex master_mutex_;
    std::mutex replica_mutex_;
    
    std::condition_variable master_cv_;
    std::condition_variable replica_cv_;

    const int MASTER_SIZE = 2;   // Expensive, keep small
    const int REPLICA_SIZE = 10; // Cheap, keep large
    
    const std::string conn_str = "postgresql://postgres:password123@localhost:5432/ticketmaster";

    static DBPool* instance;

    void fillPool(std::queue<std::shared_ptr<pqxx::connection>>& pool, int size) {
        for (int i = 0; i < size; ++i) {
            try {
                auto conn = std::make_shared<pqxx::connection>(conn_str);
                if (conn->is_open()) pool.push(conn);
            } catch (...) {}
        }
    }

    DBPool() {
        fillPool(master_pool_, MASTER_SIZE);
        fillPool(replica_pool_, REPLICA_SIZE);
        std::cout << "⚖️ CQRS INITIALIZED: " << master_pool_.size() << " Master | " << replica_pool_.size() << " Replica Conns.\n";
    }

public:
    static DBPool* GetInstance() {
        if (!instance) instance = new DBPool();
        return instance;
    }

    std::shared_ptr<pqxx::connection> acquire(PoolType type) {
        if (type == PoolType::MASTER) {
            std::unique_lock<std::mutex> lock(master_mutex_);
            
            // 🔍 TRACE LOG: PROOF OF WRITING
            std::cout << "🔴 [CQRS] Borrowing from MASTER POOL (Writes). Available: " << master_pool_.size() << std::endl;
            
            master_cv_.wait(lock, [this] { return !master_pool_.empty(); });
            auto conn = master_pool_.front();
            master_pool_.pop();
            return conn;
        } else {
            std::unique_lock<std::mutex> lock(replica_mutex_);
            
            // 🔍 TRACE LOG: PROOF OF READING
            std::cout << "🟢 [CQRS] Borrowing from REPLICA POOL (Reads). Available: " << replica_pool_.size() << std::endl;
            
            replica_cv_.wait(lock, [this] { return !replica_pool_.empty(); });
            auto conn = replica_pool_.front();
            replica_pool_.pop();
            return conn;
        }
    }

    void release(std::shared_ptr<pqxx::connection> conn, PoolType type) {
        if (type == PoolType::MASTER) {
            std::unique_lock<std::mutex> lock(master_mutex_);
            master_pool_.push(conn);
            lock.unlock();
            master_cv_.notify_one();
        } else {
            std::unique_lock<std::mutex> lock(replica_mutex_);
            replica_pool_.push(conn);
            lock.unlock();
            replica_cv_.notify_one();
        }
    }
};

DBPool* DBPool::instance = nullptr;

// 🪄 SMART CONNECTION WRAPPER
class DBConnection {
    std::shared_ptr<pqxx::connection> conn_;
    PoolType type_;
public:
    // Default to REPLICA (Read) for safety
    DBConnection(PoolType type = PoolType::REPLICA) : type_(type) {
        conn_ = DBPool::GetInstance()->acquire(type);
    }
    ~DBConnection() {
        DBPool::GetInstance()->release(conn_, type_);
    }
    pqxx::connection& operator*() { return *conn_; }
    pqxx::connection* operator->() { return conn_.get(); }
};