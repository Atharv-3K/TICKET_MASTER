import pika
import time
import json
import psycopg2

# ⚡ DATABASE CONFIG (Matches your C++ Config)
DB_CONFIG = {
    "dbname": "ticketmaster",
    "user": "postgres",
    "password": "password123",
    "host": "localhost"
}

def get_db_connection():
    return psycopg2.connect(**DB_CONFIG)

def process_booking(ch, method, properties, body):
    print(f"📥 [Worker] Received: {body.decode()}")
    
    data = body.decode().split() # Format: "BOOK <seat_id> <user_id>"
    if len(data) < 3:
        print("❌ Invalid Message Format")
        # 💀 DEAD LETTER LOGIC: 
        # We 'nack' (negative ack) it so it doesn't stay in the queue forever
        # false = don't requeue (send to dead letter graveyard)
        ch.basic_nack(delivery_tag=method.delivery_tag, requeue=False)
        return

    action, seat_id, user_id = data[0], data[1], data[2]
    
    try:
        # 🕒 SIMULATE WORK (e.g., Calling Bank API)
        time.sleep(2) 
        
        conn = get_db_connection()
        cur = conn.cursor()
        
        # 1. Create Booking in Postgres
        cur.execute(
            "INSERT INTO bookings (user_id, show_id, status, total_amount) VALUES (%s, 1, 'CONFIRMED', 50.0) RETURNING id",
            (user_id,)
        )
        booking_id = cur.fetchone()[0]
        
        # 2. Link Seat
        cur.execute(
            "INSERT INTO booking_seats (booking_id, screen_seat_id) VALUES (%s, %s)",
            (booking_id, seat_id)
        )
        
        conn.commit()
        cur.close()
        conn.close()
        
        print(f"✅ [Worker] TICKET GENERATED: Booking #{booking_id} for Seat {seat_id}")

        # 🛡️ THE SAFETY NET: MANUAL ACKNOWLEDGMENT
        # We only tell RabbitMQ to delete the message HERE, after DB is saved.
        ch.basic_ack(delivery_tag=method.delivery_tag)

    except Exception as e:
        print(f"🔥 [Worker] CRITICAL FAILURE: {e}")
        # 🔄 RETRY LOGIC:
        # If DB failed, we 'nack' with requeue=True so another worker can try.
        # In a real system, you'd check retry counts to avoid infinite loops.
        ch.basic_nack(delivery_tag=method.delivery_tag, requeue=True)

def start_worker():
    print("👷 RabbitMQ Worker Started (With Manual Acks)...")
    connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
    channel = connection.channel()

    # Make sure queue exists and is durable
    channel.queue_declare(queue='bookings', durable=True)

    # Fair Dispatch: Don't give me more than 1 message at a time
    channel.basic_qos(prefetch_count=1)

    # 🛑 auto_ack=False is the key! We must ack manually.
    channel.basic_consume(queue='bookings', on_message_callback=process_booking, auto_ack=False)

    channel.start_consuming()

if __name__ == "__main__":
    start_worker()