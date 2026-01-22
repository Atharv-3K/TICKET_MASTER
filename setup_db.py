import psycopg2

# DB CONFIG
DB_CONFIG = {
    "dbname": "ticketmaster",
    "user": "postgres",
    "password": "password123",
    "host": "localhost",
    "port": "5432"
}

def setup_database():
    try:
        conn = psycopg2.connect(**DB_CONFIG)
        cur = conn.cursor()
        
        print("🔨 Building Database Schema...")

        # 1. CLEANUP (Drop old tables if they exist to start fresh)
        # Note: We use CASCADE to remove linked tables
        cur.execute("DROP TABLE IF EXISTS booking_seats CASCADE;")
        cur.execute("DROP TABLE IF EXISTS bookings CASCADE;")
        cur.execute("DROP TABLE IF EXISTS screen_seats CASCADE;")
        cur.execute("DROP TABLE IF EXISTS shows CASCADE;")
        cur.execute("DROP TABLE IF EXISTS screens CASCADE;")
        cur.execute("DROP TABLE IF EXISTS movies CASCADE;")
        cur.execute("DROP TABLE IF EXISTS theaters CASCADE;")
        cur.execute("DROP TABLE IF EXISTS cities CASCADE;")

        # 2. CREATE TABLES
        # Cities & Theaters
        cur.execute("""
            CREATE TABLE cities (id SERIAL PRIMARY KEY, name VARCHAR(50));
            CREATE TABLE theaters (id SERIAL PRIMARY KEY, city_id INT, name VARCHAR(100));
        """)

        # Screens & Movies
        cur.execute("""
            CREATE TABLE screens (id SERIAL PRIMARY KEY, theater_id INT, name VARCHAR(50));
            CREATE TABLE movies (id SERIAL PRIMARY KEY, title VARCHAR(100), duration_mins INT);
        """)

        # Shows (The actual movie time)
        cur.execute("""
            CREATE TABLE shows (
                id SERIAL PRIMARY KEY, 
                screen_id INT, 
                movie_id INT, 
                start_time TIMESTAMP, 
                price DECIMAL(10,2)
            );
        """)

        # Seats (Physical seats in the screen)
        cur.execute("""
            CREATE TABLE screen_seats (
                id SERIAL PRIMARY KEY, 
                screen_id INT, 
                row_code VARCHAR(5), 
                seat_number INT
            );
        """)

        # BOOKINGS (The Missing Table!)
        cur.execute("""
            CREATE TABLE bookings (
                id SERIAL PRIMARY KEY, 
                user_id INT, 
                show_id INT, 
                status VARCHAR(20), 
                total_amount DECIMAL(10,2),
                booking_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        """)

        # Booking_Seats (Linking a booking to specific seats)
        cur.execute("""
            CREATE TABLE booking_seats (
                booking_id INT REFERENCES bookings(id), 
                screen_seat_id INT REFERENCES screen_seats(id),
                PRIMARY KEY (booking_id, screen_seat_id)
            );
        """)
        
        # 3. SEED DATA (So the Worker has something to link to)
        print("🌱 Seeding Dummy Data...")
        
        # Create a User (ID 1)
        cur.execute("INSERT INTO users (username, email, password_hash) VALUES ('Admin', 'admin@test.com', '123') ON CONFLICT DO NOTHING;")
        
        # Create dummy infrastructure
        cur.execute("INSERT INTO cities (name) VALUES ('Bangalore');")
        cur.execute("INSERT INTO theaters (city_id, name) VALUES (1, 'PVR');")
        cur.execute("INSERT INTO screens (theater_id, name) VALUES (1, 'Audi 1');")
        cur.execute("INSERT INTO movies (title, duration_mins) VALUES ('Avengers', 180);")
        cur.execute("INSERT INTO shows (screen_id, movie_id, start_time, price) VALUES (1, 1, '2025-10-10 10:00:00', 200.00);")

        # Create Seats (ID 1 to 500)
        # We need to make sure Seat ID 5 and 123 exist because your logs referenced them
        for i in range(1, 200):
            cur.execute(f"INSERT INTO screen_seats (id, screen_id, row_code, seat_number) VALUES ({i}, 1, 'A', {i}) ON CONFLICT DO NOTHING;")

        # Update the sequence so new inserts don't crash
        cur.execute("SELECT setval('screen_seats_id_seq', (SELECT MAX(id) FROM screen_seats));")

        conn.commit()
        cur.close()
        conn.close()
        print("✅ Database Setup Complete! You are ready.")

    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    setup_database()