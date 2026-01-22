import requests
import threading
import time

BASE_URL = "http://127.0.0.1:8090/api"
SEAT_ID = 5  # Let's fight for Seat 5
THREADS = 5  # 5 Users, 1 Seat

print("🔥 STARTING BACKEND STRESS TEST...")

# 1. THE AVENGERS TEST (Concurrency)
def attack_seat(user_id):
    # Simulate different users
    payload = {"seat_id": SEAT_ID}
    try:
        res = requests.post(f"{BASE_URL}/reserve", json=payload)
        print(f"User {user_id}: {res.status_code} - {res.text}")
    except Exception as e:
        print(f"User {user_id}: FAILED REQUEST")

print(f"\n⚔️ LAUNCHING {THREADS} THREADS AT SEAT {SEAT_ID}...")
threads = []
for i in range(THREADS):
    t = threading.Thread(target=attack_seat, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

# 2. IDEMPOTENCY TEST (Ghost Booking)
print("\n👻 TESTING IDEMPOTENCY (Double Payment)...")
headers = {"Idempotency-Key": "unique-key-999"}
payload = {"seat_id": SEAT_ID}

# First Charge (Should Process)
t1 = time.time()
r1 = requests.post(f"{BASE_URL}/pay", json=payload, headers=headers)
print(f"Charge 1 (Logic): {r1.status_code} ({r1.elapsed.total_seconds():.4f}s)")

# Second Charge (Should Hit Cache)
t2 = time.time()
r2 = requests.post(f"{BASE_URL}/pay", json=payload, headers=headers)
print(f"Charge 2 (Cache): {r2.status_code} ({r2.elapsed.total_seconds():.4f}s)")


print("\n🤖 LAUNCHING BOT ATTACK (Rate Limit Test)...")
def bot_spam(bot_id):
    for i in range(15): # Try to send 15 reqs in 1 second
        try:
            res = requests.get(f"{BASE_URL}/seats")
            if res.status_code == 429:
                print(f"Bot {bot_id}: 🛡️ BLOCKED by Rate Limiter!")
                break
            else:
                print(f"Bot {bot_id}: Request {i+1} OK")
        except:
            pass

# Launch 1 Bot Thread
t = threading.Thread(target=bot_spam, args=(99,))
t.start()
t.join()