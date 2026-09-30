#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace lru {

// A cache-local, lazily growing object pool. Freed slots are reused; storage
// remains owned by the pool until its destructor, so cache nodes never leak.
template <typename T>
class MemoryPool {
    static_assert(alignof(T) <= alignof(std::max_align_t), "over-aligned pooled types are unsupported");
    struct FreeSlot { FreeSlot* next; };

public:
    MemoryPool() = default;
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    ~MemoryPool() {
        for (void* slot : storage_) {
            ::operator delete(slot);
        }
    }

    template <typename... Args>
    T* create(Args&&... args) {
        void* memory = nullptr;
        if (free_) {
            memory = free_;
            free_ = free_->next;
        } else {
            memory = ::operator new(sizeof(T));
            try {
                storage_.push_back(memory);
            } catch (...) {
                ::operator delete(memory);
                throw;
            }
        }
        try {
            return new (memory) T(std::forward<Args>(args)...);
        } catch (...) {
            free_ = new (memory) FreeSlot{free_};
            throw;
        }
    }

    void destroy(T* object) noexcept {
        if (!object) return;
        object->~T();
        free_ = new (object) FreeSlot{free_};
    }

    std::size_t allocated_slots() const noexcept { return storage_.size(); }

private:
    std::vector<void*> storage_;
    FreeSlot* free_ = nullptr;
};

}  // namespace lru
