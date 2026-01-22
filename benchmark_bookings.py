import threading
import requests
import time
import random

URL_BOOK = "http://127.0.0.1:8080/api/book"
URL_LOGIN = "http://127.0.0.1:8080/api/login"

# 1. Login to get a Token
def get_token():
    try:
        # Assuming you have a user 'atharv' with password 'password'
        # If not, use the signup logic or hardcode a known user
        res = requests.post(URL_LOGIN, json={"email": "atharv@iitkgp.ac.in", "password": "password123"})
        if res.status_code == 200:
            return res.json()['token']
    except:
        pass
    return None

TOKEN = get_token()
if not TOKEN:
    print("❌ Login Failed. Make sure the user exists!")
    exit()

HEADERS = {"Authorization": TOKEN}

def book_seat(i):
    # Try to book a random seat between 1 and 200
    seat_id = random.randint(1, 200)
    start = time.time()
    try:
        response = requests.post(URL_BOOK, json={"seat_id": seat_id}, headers=HEADERS)
        end = time.time()
        
        # We only care about the SERVER response time, not if the seat was actually free
        print(f"User {i} -> Status: {response.status_code} | ⏱️ Time: {(end-start)*1000:.2f} ms")
    except Exception as e:
        print(f"User {i}: FAILED")

print(f"🔥 Launching 50 Concurrent Booking Requests...")
threads = []
start_total = time.time()

for i in range(50):
    t = threading.Thread(target=book_seat, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

end_total = time.time()
print(f"✅ Total Batch Time: {end_total - start_total:.2f} seconds")