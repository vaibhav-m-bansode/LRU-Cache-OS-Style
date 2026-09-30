#include "lru_cache/lru_cache.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {
bool parse_size(const std::string& text, std::size_t& result) {
    if (text.empty() || text.front() == '-') return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        parsed > std::numeric_limits<std::size_t>::max()) return false;
    result = static_cast<std::size_t>(parsed);
    return true;
}

void print_help() {
    std::cout << "Commands: PUT <key> <value>, GET <key>, ERASE <key>, STATS, ORDER, CLEAR, HELP, QUIT\n";
}
}

int main(int argc, char** argv) {
    std::size_t budget = 4096;
    if (argc == 3 && std::string(argv[1]) == "--budget") {
        if (!parse_size(argv[2], budget)) {
            std::cerr << "Invalid --budget value (expected non-negative bytes).\n";
            return 2;
        }
    } else if (argc != 1) {
        std::cerr << "Usage: lru-cli [--budget BYTES]\n";
        return 2;
    }

    lru::LruCache<std::string, std::string> cache(budget);
    std::cout << "LRU cache ready (logical byte budget: " << budget << "). Type HELP.\n";
    print_help();
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command.empty()) continue;
        if (command == "QUIT" || command == "EXIT") break;
        if (command == "HELP") { print_help(); continue; }
        if (command == "PUT") {
            std::string key, value;
            if (!(input >> key >> value)) { std::cout << "Usage: PUT <key> <value>\n"; continue; }
            const auto before = cache.evictions();
            if (!cache.put(std::move(key), std::move(value))) std::cout << "Rejected: item exceeds budget.\n";
            else std::cout << "Stored. Evicted " << (cache.evictions() - before) << " item(s).\n";
        } else if (command == "GET") {
            std::string key, value;
            if (!(input >> key)) { std::cout << "Usage: GET <key>\n"; continue; }
            if (cache.get(key, value)) std::cout << value << '\n';
            else std::cout << "MISS\n";
        } else if (command == "ERASE") {
            std::string key;
            if (!(input >> key)) { std::cout << "Usage: ERASE <key>\n"; continue; }
            std::cout << (cache.erase(key) ? "Erased.\n" : "Not found.\n");
        } else if (command == "STATS") {
            std::cout << "entries=" << cache.size() << " used=" << cache.used_bytes()
                      << '/' << cache.budget_bytes() << " hits=" << cache.hits()
                      << " misses=" << cache.misses() << " evictions=" << cache.evictions()
                      << " node_slots=" << cache.allocated_node_slots() << '\n';
        } else if (command == "ORDER") {
            bool first = true;
            cache.for_each_mru([&](const std::string& key, const std::string& value) {
                if (!first) std::cout << " -> ";
                std::cout << key << '=' << value;
                first = false;
            });
            std::cout << (first ? "(empty)" : "") << '\n';
        } else if (command == "CLEAR") {
            cache.clear(); std::cout << "Cleared.\n";
        } else {
            std::cout << "Unknown command. Type HELP.\n";
        }
    }
    return 0;
}
