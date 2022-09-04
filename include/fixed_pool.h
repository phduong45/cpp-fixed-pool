#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>

template <class T, std::size_t N>
class FixedPool {
    static_assert(N > 0, "FixedPool capacity must be greater than zero");

  private:
    static constexpr std::size_t capacity_ = N;

    alignas(T) std::byte storage_[capacity_ * sizeof(T)];
    std::size_t free_indices_[capacity_];
    std::size_t free_count_;

#ifndef NDEBUG
    bool occupied_[capacity_]{};
#endif

    std::byte* raw(std::size_t index) noexcept {
        return storage_ + index * sizeof(T);
    }

    std::size_t pop_free_index() noexcept {
        assert(free_count_ > 0);
        return free_indices_[--free_count_];
    }

    void push_free_index(std::size_t index) noexcept {
        assert(free_count_ < capacity_);
        free_indices_[free_count_++] = index;
    }

    std::size_t index_from_pointer(T* p) const noexcept {
        auto* bytes = reinterpret_cast<std::byte*>(p);
        auto offset = bytes - storage_;
        assert(offset >= 0);

        auto byte_offset = static_cast<std::size_t>(offset);
        assert(byte_offset < capacity_ * sizeof(T));
        assert(byte_offset % sizeof(T) == 0);

        return byte_offset / sizeof(T);
    }

  public:
    FixedPool() : free_count_{capacity_} {
        for (std::size_t i = 0; i < capacity_; ++i) {
            free_indices_[i] = capacity_ - 1 - i;
        }
    }

    ~FixedPool() {
        assert(in_use() == 0);
    }

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) = delete;
    FixedPool& operator=(FixedPool&&) = delete;

    template <class... Args>
    T* create(Args&&... args) {
        if (free_count_ == 0) {
            return nullptr;
        }

        std::size_t index = pop_free_index();
        try {
            T* object = new (raw(index)) T(std::forward<Args>(args)...);
#ifndef NDEBUG
            occupied_[index] = true;
#endif
            return object;
        } catch (...) {
            push_free_index(index);
            throw;
        }
    }

    void destroy(T* p) noexcept {
        std::size_t index = index_from_pointer(p);

#ifndef NDEBUG
        assert(occupied_[index] && "destroy called for a non-live slot");
#endif

        p->~T();

#ifndef NDEBUG
        occupied_[index] = false;
#endif

        push_free_index(index);
    }

    static constexpr std::size_t capacity() noexcept {
        return capacity_;
    }

    std::size_t available() const noexcept {
        return free_count_;
    }

    std::size_t in_use() const noexcept {
        return capacity_ - free_count_;
    }
};
