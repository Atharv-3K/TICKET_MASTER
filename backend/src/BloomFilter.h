#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cmath>

// 🛡️ THE MATH SHIELD
// A probabilistic data structure that answers:
// "Is this item definitely NOT in the set?" (100% accurate)
// "Is this item MAYBE in the set?" (99% accurate)

class BloomFilter {
private:
    std::vector<bool> bit_array;
    int size;
    int num_hashes;

    // 🧮 FNV-1a Hash Function (Industry Standard for Speed)
    uint64_t fnv1a_hash(const std::string& str, uint64_t seed = 14695981039346656037ULL) {
        uint64_t hash = seed;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

public:
    // Initialize based on expected items (n) and false positive rate (p)
    // For 1000 seats and 1% error rate: size=9585 bits, hashes=7
    BloomFilter(int expected_items, double false_positive_rate = 0.01) {
        size = - (expected_items * log(false_positive_rate)) / (pow(log(2), 2));
        num_hashes = (size / expected_items) * log(2);
        
        bit_array.resize(size, false);
        std::cout << "🛡️ BLOOM FILTER INITIALIZED: " << size << " bits, " << num_hashes << " hashes.\n";
    }

    void add(const std::string& item) {
        uint64_t hash1 = fnv1a_hash(item);
        uint64_t hash2 = fnv1a_hash(item, 5234235235235235ULL); // Different seed

        for (int i = 0; i < num_hashes; i++) {
            // Double Hashing Technique: h(i) = (h1 + i*h2) % size
            uint64_t combined_hash = (hash1 + i * hash2) % size;
            bit_array[combined_hash] = true;
        }
    }

    // Returns TRUE if item MIGHT exist.
    // Returns FALSE if item DEFINITELY DOES NOT exist.
    bool possiblyContains(const std::string& item) {
        uint64_t hash1 = fnv1a_hash(item);
        uint64_t hash2 = fnv1a_hash(item, 5234235235235235ULL);

        for (int i = 0; i < num_hashes; i++) {
            uint64_t combined_hash = (hash1 + i * hash2) % size;
            if (!bit_array[combined_hash]) {
                return false; // 🚫 100% Certain: It does not exist.
            }
        }
        return true; // ✅ Maybe exists (Check DB to be sure)
    }
};