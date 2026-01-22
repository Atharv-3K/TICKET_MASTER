import requests
import threading
import time

BASE_URL = "http://127.0.0.1:8090/api/reserve"

def attack_bot(seat_id, bot_id):
    try:
        start = time.time()
        res = requests.post(BASE_URL, json={"seat_id": seat_id})
        duration = time.time() - start
        
        status = res.status_code
        msg = "Unknown"
        if status == 200: msg = "✅ Reserved!"
        if status == 409: msg = "⚠️ Taken (Redis)"
        if status == 404: msg = "🛡️ BLOCKED (Bloom)"
        
        print(f"Bot {bot_id} -> Seat {seat_id}: {status} {msg} ({duration:.4f}s)")
        
    except Exception as e:
        print(f"Bot {bot_id} Error: {e}")

print("🔥 LAUNCHING TAYLOR SWIFT BOT ATTACK...")

threads = []
# 1. Launch 10 Bots targeting a REAL seat (Seat 5)
# These should hit Redis (409 or 200)
for i in range(10):
    t = threading.Thread(target=attack_bot, args=(5, i))
    threads.append(t)
    t.start()

# 2. Launch 10 Bots targeting a FAKE seat (Seat 999999)
# These should be BLOCKED INSTANTLY (404)
for i in range(10):
    t = threading.Thread(target=attack_bot, args=(999999, i+10))
    threads.append(t)
    t.start()

for t in threads:
    t.join()