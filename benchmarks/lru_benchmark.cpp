#include "lru_cache/lru_cache.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const std::size_t operations = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 1'000'000;
    const std::size_t working_set = argc > 2 ? static_cast<std::size_t>(std::stoull(argv[2])) : 16'384;
    if (working_set == 0 || operations == 0) {
        std::cerr << "operations and working-set must be greater than zero\n";
        return 2;
    }
    lru::LruCache<std::uint64_t, std::string> cache(working_set * 128, working_set);
    std::vector<std::uint64_t> trace;
    trace.reserve(operations);
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    for (std::size_t i = 0; i < operations; ++i) {
        state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
        trace.push_back((state * 0x2545F4914F6CDD1DULL) % (working_set * 2));
    }
    std::size_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (const auto key : trace) {
        if (const auto* value = cache.get(key)) checksum += value->size();
        else cache.put(key, std::string("payload-") + std::to_string(key));
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double seconds = std::chrono::duration<double>(elapsed).count();
    std::cout << "operations=" << operations << " working_set=" << working_set
              << " elapsed_seconds=" << seconds
              << " operations_per_second=" << static_cast<double>(operations) / seconds
              << " hits=" << cache.hits() << " misses=" << cache.misses()
              << " evictions=" << cache.evictions() << " checksum=" << checksum << '\n';
}
