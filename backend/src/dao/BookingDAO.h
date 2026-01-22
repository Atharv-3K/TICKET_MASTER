#pragma once
#include "../db.h"
#include "../models/Booking.h"
#include <vector>
#include <iostream>

class BookingDAO {
public:
    // 1. CREATE BOOKING (The Complex Transaction)
    // Takes a user, a show, and a LIST of seat IDs.
    // Returns the new Booking ID.
    static int createBooking(int user_id, int show_id, const std::vector<int>& seat_ids, double amount) {
        try {
            DBConnection conn;
            pqxx::work txn(*conn); // Start Transaction

            // A. Insert the main Booking Record
            pqxx::result res = txn.exec_params(
                "INSERT INTO bookings (user_id, show_id, status, total_amount) VALUES ($1, $2, 'CONFIRMED', $3) RETURNING id",
                user_id, show_id, amount
            );
            int booking_id = res[0][0].as<int>();

            // B. Link the Seats (Bulk Insert or Loop)
            // In a real high-perf app, we'd build a single bulk query string.
            // For clarity here, we loop (still fast inside a transaction).
            for (int seat_id : seat_ids) {
                txn.exec_params(
                    "INSERT INTO booking_seats (booking_id, screen_seat_id) VALUES ($1, $2)",
                    booking_id, seat_id
                );
            }

            txn.commit(); // ✅ All or Nothing commit
            return booking_id;

        } catch (const std::exception& e) {
            std::cerr << "❌ BookingDAO Error: " << e.what() << std::endl;
            return -1; // Failed
        }
    }

    // 2. GET USER HISTORY
    static std::vector<Booking> getBookingsByUser(int user_id) {
        std::vector<Booking> list;
        try {
            DBConnection conn;
            pqxx::work txn(*conn);
            
            pqxx::result res = txn.exec_params(
                "SELECT id, user_id, show_id, status, total_amount, booking_time FROM bookings WHERE user_id = $1 ORDER BY id DESC",
                user_id
            );

            for (auto row : res) {
                list.push_back({
                    row[0].as<int>(), row[1].as<int>(), row[2].as<int>(),
                    row[3].as<std::string>(), row[4].as<double>(), row[5].as<std::string>()
                });
            }
        } catch (...) {}
        return list;
    }
};