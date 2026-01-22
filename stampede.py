import threading
import requests

# Target the Catalog Endpoint
URL = "http://127.0.0.1:8090/api/theaters/1/shows"

def fetch_catalog(i):
    try:
        res = requests.get(URL)
        # The server sends 'X-Source' header to tell us where data came from
        src = res.headers.get("X-Source", "Unknown")
        print(f"User {i}: Status {res.status_code} - Source: {src}")
    except:
        print(f"User {i}: Connection Failed")

print("🐘 RELEASING THE HERD (10 Concurrent Requests)...")

threads = []
# Launch 10 users at the exact same time
for i in range(10):
    t = threading.Thread(target=fetch_catalog, args=(i,))
    threads.append(t)
    t.start()

for t in threads: t.join()