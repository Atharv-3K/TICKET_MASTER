import pika
import json
import time
import sys

# 1. Connect to RabbitMQ
def connect():
    try:
        connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
        channel = connection.channel()
        channel.queue_declare(queue='bookings', durable=True)
        print(' [*] Waiting for messages. To exit press CTRL+C')
        return channel
    except Exception as e:
        print(f"❌ Connection Failed: {e}")
        sys.exit(1)

# 2. Define the "Job" (What to do when a message arrives)
def callback(ch, method, properties, body):
    data = json.loads(body)
    print(f" [x] Received Event: {data['event']}")
    
    # Simulate Sending an Email (Takes time!)
    print(f"     📧 Sending Confirmation Email to User ID: {data['user_id']}...")
    time.sleep(2) # Simulate 2 seconds of work (SMTP server delay)
    print(f"     ✅ Email Sent for Seat {data['seat_id']}!")
    
    # 3. Acknowledge completion (Tell RabbitMQ "I'm done, delete the message")
    ch.basic_ack(delivery_tag=method.delivery_tag)

# 4. Start Listening
if __name__ == '__main__':
    channel = connect()
    channel.basic_qos(prefetch_count=1) # Only take 1 job at a time
    channel.basic_consume(queue='bookings', on_message_callback=callback)
    channel.start_consuming()