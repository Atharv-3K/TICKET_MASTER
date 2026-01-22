import redis

try:
    # Connect to local Redis
    r = redis.Redis(host='localhost', port=6379, db=0)

    # Delete the specific catalog key (or use r.flushall() to wipe everything)
    r.delete("shows:theater:1")
    print("✅ Cache Cleared! The 'Urn' is now empty.")

except Exception as e:
    print(f"❌ Error: {e}")