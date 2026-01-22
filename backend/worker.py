import pika
import psycopg2
import json
import time

# 1. DB CONFIG (Must match your C++ settings)
DB_CONFIG = {
    "dbname": "ticketmaster",
    "user": "postgres",
    "password": "password123",
    "host": "localhost",
    "port": "5432"
}

def get_db_connection():
    return psycopg2.connect(**DB_CONFIG)

# 2. THE WORKER LOGIC
def callback(ch, method, properties, body):
    msg = body.decode()
    print(f"📥 RECEIVED: {msg}")
    
    # Message Format: "BOOK {seat_id} {user_id}"
    parts = msg.split(" ")
    if len(parts) < 3:
        print("❌ Invalid Message Format")
        ch.basic_ack(delivery_tag=method.delivery_tag)
        return

    seat_id = int(parts[1])
    user_id = int(parts[2])

    try:
        conn = get_db_connection()
        cur = conn.cursor()

        # A. SIMULATE PROCESSING TIME (e.g., Bank transaction)
        print("   💳 Contacting Bank API...", end="", flush=True)
        time.sleep(2) # Simulate 2s delay
        print(" PAID!")

        # B. DB TRANSACTION: Create Booking & Link Seat
        # 1. Insert Booking
        cur.execute(
            "INSERT INTO bookings (user_id, show_id, status, total_amount) VALUES (%s, %s, 'CONFIRMED', 50.00) RETURNING id",
            (user_id, 1) # Hardcoded Show ID 1 for now
        )
        booking_id = cur.fetchone()[0]

        # 2. Link Seat (Map plain Seat ID to Screen Seat ID)
        # In our simplified schema, we assume Seat ID matches Screen Seat ID directly
        cur.execute(
            "INSERT INTO booking_seats (booking_id, screen_seat_id) VALUES (%s, %s)",
            (booking_id, seat_id)
        )
        
        # 3. Update Seat Status in 'seats' table (Legacy table support)
        cur.execute("UPDATE seats SET status = 'BOOKED' WHERE id = %s", (seat_id,))

        conn.commit()
        cur.close()
        conn.close()
        
        print(f"   ✅ TICKET GENERATED! Booking ID: {booking_id}")
        
        # Acknowledge message (Tell RabbitMQ it's safe to delete)
        ch.basic_ack(delivery_tag=method.delivery_tag)

    except Exception as e:
        print(f"   ❌ DB ERROR: {e}")
        # Negative Ack (Tell RabbitMQ to retry later)
        ch.basic_nack(delivery_tag=method.delivery_tag, requeue=True)

# 3. SETUP RABBITMQ LISTENER
print("👷 WORKER SERVICE STARTED. Waiting for tickets...")
connection = pika.BlockingConnection(pika.ConnectionParameters('127.0.0.1'))
channel = connection.channel()
channel.queue_declare(queue='bookings', durable=True)

channel.basic_qos(prefetch_count=1) # Handle 1 ticket at a time
channel.basic_consume(queue='bookings', on_message_callback=callback)
channel.start_consuming()