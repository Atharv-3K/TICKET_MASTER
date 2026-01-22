#pragma once
#include <string>
#include <vector>
#include <crow/json.h>

struct Booking {
    int id;
    int user_id;
    int show_id;
    std::string status; // PENDING, CONFIRMED, FAILED
    double total_amount;
    std::string booking_time;
    
    // Helper to send to frontend
    crow::json::wvalue toJson() const {
        crow::json::wvalue json;
        json["id"] = id;
        json["user_id"] = user_id;
        json["show_id"] = show_id;
        json["status"] = status;
        json["amount"] = total_amount;
        json["time"] = booking_time;
        return json;
    }
};