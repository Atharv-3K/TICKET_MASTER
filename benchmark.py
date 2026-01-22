import threading
import requests
import time

# URL of your C++ Server
URL = "http://127.0.0.1:8080/api/seats"
THREADS = 50  # Simulate 50 users clicking at once

def fetch_seats(i):
    start = time.time()
    try:
        # This request hits your C++ server
        response = requests.get(URL)
        end = time.time()
        
        # Check if we got a valid response
        if response.status_code == 200:
            print(f"User {i}: {(end-start)*1000:.2f} ms")
        else:
            print(f"User {i}: ERROR {response.status_code}")
            
    except Exception as e:
        print(f"User {i}: FAILED ({e})")

print(f"🔥 Launching {THREADS} concurrent requests...")
threads = []
start_total = time.time()

# Launch 50 threads immediately
for i in range(THREADS):
    t = threading.Thread(target=fetch_seats, args=(i,))
    threads.append(t)
    t.start()

# Wait for all to finish
for t in threads:
    t.join()

end_total = time.time()
print(f"✅ Total Time: {end_total - start_total:.2f} seconds")