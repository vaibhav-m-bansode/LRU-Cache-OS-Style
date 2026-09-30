#include "lru_cache/lru_cache.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; ++failures; } } while (false)

void basic_operations() {
    lru::LruCache<int, int> cache(4096, 2);
    CHECK(cache.empty());
    CHECK(cache.put(1, 10)); CHECK(cache.put(2, 20));
    int value = 0;
    CHECK(cache.get(1, value)); CHECK(value == 10);
    CHECK(cache.put(3, 30));
    CHECK(!cache.get(2, value));
    CHECK(cache.get(1, value)); CHECK(cache.get(3, value));
    CHECK(cache.size() == 2); CHECK(cache.evictions() == 1);
}

void updates_and_erase() {
    lru::LruCache<int, int> cache(4096, 3);
    CHECK(cache.put(1, 1)); CHECK(cache.put(1, 9));
    CHECK(cache.size() == 1);
    CHECK(*cache.get(1) == 9);
    CHECK(cache.erase(1)); CHECK(!cache.erase(1));
    CHECK(cache.empty());
}

void byte_budget_and_pool_reuse() {
    using Cache = lru::LruCache<std::string, std::string>;
    const std::size_t one = sizeof(std::string) * 2 + sizeof(void*) * 6 + 1;
    Cache cache(one * 2);
    CHECK(cache.put("a", "1")); CHECK(cache.put("b", "2"));
    CHECK(cache.used_bytes() <= cache.budget_bytes());
    CHECK(cache.put("c", "3"));
    CHECK(cache.size() <= 2);
    const auto slots = cache.allocated_node_slots();
    CHECK(cache.put("d", "4"));
    CHECK(cache.allocated_node_slots() <= slots + 1);

    Cache tiny(1);
    CHECK(!tiny.put("x", "y"));
    CHECK(tiny.empty());
}

void clear_and_order() {
    lru::LruCache<int, int> cache(4096, 4);
    cache.put(1, 1); cache.put(2, 2); cache.put(3, 3);
    (void)cache.get(1);
    int expected[] = {1, 3, 2};
    int index = 0;
    cache.for_each_mru([&](int key, int) { CHECK(index < 3); if (index < 3) CHECK(key == expected[index]); ++index; });
    CHECK(index == 3);
    cache.clear(); CHECK(cache.empty()); CHECK(cache.used_bytes() == 0);
}
}

int main() {
    basic_operations(); updates_and_erase(); byte_budget_and_pool_reuse(); clear_and_order();
    if (failures) return EXIT_FAILURE;
    std::cout << "All LRU cache tests passed.\n";
    return EXIT_SUCCESS;
}
