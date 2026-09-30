#pragma once

#include "lru_cache/memory_pool.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace lru {

// A byte-budgeted LRU for copyable keys/values. The budget tracks a documented
// logical estimate (node plus string payload bytes); allocator/hash-table
// bookkeeping is implementation-dependent and is not included.
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class LruCache {
    struct Node {
        Key key;
        Value value;
        std::size_t charge;
        Node* previous = nullptr;
        Node* next = nullptr;

        Node(Key k, Value v, std::size_t bytes)
            : key(std::move(k)), value(std::move(v)), charge(bytes) {}
    };

public:
    explicit LruCache(std::size_t budget_bytes, std::size_t max_entries =
                      std::numeric_limits<std::size_t>::max())
        : budget_bytes_(budget_bytes), max_entries_(max_entries) {
        entries_.reserve(max_entries == std::numeric_limits<std::size_t>::max()
                             ? 0U : max_entries);
    }

    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    ~LruCache() { clear(); }

    bool get(const Key& key, Value& out) {
        const auto found = entries_.find(key);
        if (found == entries_.end()) { ++misses_; return false; }
        Node* node = found->second;
        out = node->value;
        move_to_front(node);
        ++hits_;
        return true;
    }

    const Value* get(const Key& key) {
        const auto found = entries_.find(key);
        if (found == entries_.end()) { ++misses_; return nullptr; }
        move_to_front(found->second);
        ++hits_;
        return &found->second->value;
    }

    // Returns false if the item cannot fit or the entry limit is zero.
    bool put(Key key, Value value) {
        const std::size_t charge = estimate_charge(key, value);
        if (charge > budget_bytes_ || max_entries_ == 0) return false;

        const auto found = entries_.find(key);
        if (found != entries_.end()) {
            Node* node = found->second;
            const std::size_t old_charge = node->charge;
            if (charge > old_charge) {
                evict_until_fits(charge - old_charge, node);
            }
            // Stage value first; update charge only after assignment succeeds.
            node->value = std::move(value);
            used_bytes_ = used_bytes_ - old_charge + charge;
            node->charge = charge;
            move_to_front(node);
            return true;
        }

        evict_until_fits(charge, nullptr);
        while (entries_.size() >= max_entries_) evict_lru();
        Node* node = pool_.create(std::move(key), std::move(value), charge);
        try {
            entries_.emplace(node->key, node);
        } catch (...) {
            pool_.destroy(node);
            throw;
        }
        link_front(node);
        used_bytes_ += charge;
        return true;
    }

    bool erase(const Key& key) {
        const auto found = entries_.find(key);
        if (found == entries_.end()) return false;
        remove_node(found->second);
        return true;
    }

    void clear() noexcept {
        while (least_recent_) erase_node(least_recent_);
        entries_.clear();
        used_bytes_ = 0;
    }

    std::size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t budget_bytes() const noexcept { return budget_bytes_; }
    std::size_t used_bytes() const noexcept { return used_bytes_; }
    std::size_t hits() const noexcept { return hits_; }
    std::size_t misses() const noexcept { return misses_; }
    std::size_t evictions() const noexcept { return evictions_; }
    std::size_t allocated_node_slots() const noexcept { return pool_.allocated_slots(); }

    // Snapshot in MRU-to-LRU order; useful for diagnostics and tests.
    template <typename Visitor>
    void for_each_mru(Visitor&& visitor) const {
        for (const Node* node = most_recent_; node; node = node->next)
            visitor(node->key, node->value);
    }

private:
    static std::size_t key_payload(const Key& key, std::true_type) noexcept { return key.size(); }
    static std::size_t key_payload(const Key&, std::false_type) noexcept { return 0; }
    static std::size_t value_payload(const Value& value, std::true_type) noexcept { return value.size(); }
    static std::size_t value_payload(const Value&, std::false_type) noexcept { return 0; }

    static std::size_t estimate_charge(const Key& key, const Value& value) noexcept {
        return sizeof(Node) + key_payload(key, typename std::is_same<Key, std::string>::type{}) +
               value_payload(value, typename std::is_same<Value, std::string>::type{});
    }

    void link_front(Node* node) noexcept {
        node->previous = nullptr;
        node->next = most_recent_;
        if (most_recent_) most_recent_->previous = node;
        else least_recent_ = node;
        most_recent_ = node;
    }
    void unlink(Node* node) noexcept {
        if (node->previous) node->previous->next = node->next;
        else most_recent_ = node->next;
        if (node->next) node->next->previous = node->previous;
        else least_recent_ = node->previous;
    }
    void move_to_front(Node* node) noexcept {
        unlink(node);
        link_front(node);
    }
    void erase_node(Node* node) noexcept {
        unlink(node);
        used_bytes_ -= node->charge;
        entries_.erase(node->key);
        pool_.destroy(node);
    }
    void remove_node(Node* node) noexcept { erase_node(node); }
    void evict_lru() noexcept {
        if (empty()) return;
        erase_node(least_recent_);
        ++evictions_;
    }
    void evict_until_fits(std::size_t extra, const Node* protected_node) noexcept {
        while (used_bytes_ > budget_bytes_ - extra) {
            Node* victim = least_recent_;
            if (!victim) return;
            if (victim == protected_node) {
                victim = victim->previous;
                if (!victim) return;
            }
            erase_node(victim);
            ++evictions_;
        }
    }

    std::size_t budget_bytes_;
    std::size_t max_entries_;
    std::size_t used_bytes_ = 0;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    std::size_t evictions_ = 0;
    Node* most_recent_ = nullptr;
    Node* least_recent_ = nullptr;
    MemoryPool<Node> pool_;
    std::unordered_map<Key, Node*, Hash, KeyEqual> entries_;
};

}  // namespace lru
