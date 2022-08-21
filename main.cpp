#include <cassert>
#include <cstddef>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

struct User {
    std::string name_;

    User(std::string name) : name_{std::move(name)} {
        std::cout << "construct " << name_ << "\n";
    }

    ~User() { std::cout << "destroy " << name_ << "\n"; }
};

struct Widget {
    int id;
    std::string label;

    Widget(int id, std::string label) : id{id}, label{std::move(label)} {}
};

struct ThrowingWidget {
    ThrowingWidget() { throw std::runtime_error{"construction failed"}; }
};

template <class T, std::size_t N> class FixedPool {
    static_assert(N > 0, "FixedPool capacity must be greater than zero");

  private:
    static constexpr std::size_t capacity_ = N;

    alignas(T) std::byte storage_[capacity_ * sizeof(T)];
    std::size_t free_indices_[capacity_];
    std::size_t free_count_;

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

    ~FixedPool() { assert(in_use() == 0); }

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) = delete;
    FixedPool& operator=(FixedPool&&) = delete;

    template <class... Args> T* create(Args&&... args) {
        if (free_count_ == 0) {
            return nullptr;
        }

        std::size_t index = pop_free_index();
        try {
            return new (raw(index)) T(std::forward<Args>(args)...);
        } catch (...) {
            push_free_index(index);
            throw;
        }
    }

    void destroy(T* p) noexcept {
        std::size_t index = index_from_pointer(p);
        p->~T();
        push_free_index(index);
    }

    static constexpr std::size_t capacity() noexcept { return capacity_; }

    std::size_t available() const noexcept { return free_count_; }

    std::size_t in_use() const noexcept { return capacity_ - free_count_; }
};

int main() {
    {
        // Ordinary new allocates storage and constructs the object.
        User* p = new User{"ordinary"};
        delete p;
    }

    {
        // This is raw storage only. No User object is alive yet.
        alignas(User) std::byte buffer[sizeof(User)];
        User* p = new (buffer) User{"placement"};

        // Placement new does not own the storage, so we destroy explicitly.
        p->~User();
    }

    {
        alignas(User) std::byte buffer[sizeof(User)];
        User* first = new (buffer) User{"first"};
        std::cout << "address of first: " << static_cast<void*>(first) << "\n";
        first->~User();

        User* second = new (buffer) User{"second"};
        std::cout << "address of second: " << static_cast<void*>(second)
                  << "\n";
        second->~User();
    }

    {
        alignas(User) std::byte storage[2 * sizeof(User)];
        std::byte* slot0 = storage + 0 * sizeof(User);
        std::byte* slot1 = storage + 1 * sizeof(User);

        User* a = new (slot0) User{"slot0"};
        User* b = new (slot1) User{"slot1"};

        std::cout << "address of a: " << static_cast<void*>(a) << "\n";
        std::cout << "address of b: " << static_cast<void*>(b) << "\n";

        auto distance =
            reinterpret_cast<std::byte*>(b) - reinterpret_cast<std::byte*>(a);

        assert(distance == sizeof(User));
        std::cout << "distance: " << distance << "\n";
        std::cout << "sizeof(User): " << sizeof(User) << "\n";

        b->~User();
        a->~User();
    }

    {
        alignas(User) std::byte storage[2 * sizeof(User)];

        // Free slots are tracked by index. The valid stack range is
        // free_indices[0..free_count), with the top at free_count - 1.
        std::size_t free_indices[2] = {1, 0};
        std::size_t free_count = 2;

        // Convert a slot index into the byte address where a User can live.
        auto raw = [&](std::size_t index) -> std::byte* {
            return storage + index * sizeof(User);
        };

        // Pop a free slot index in O(1).
        auto pop_free_index = [&]() -> std::size_t {
            assert(free_count > 0);
            return free_indices[--free_count];
        };

        // Push a slot index back after the object in that slot is destroyed.
        auto push_free_index = [&](std::size_t index) {
            assert(free_count < 2);
            free_indices[free_count++] = index;
        };

        auto index_from_pointer = [&](User* p) -> std::size_t {
            auto* bytes = reinterpret_cast<std::byte*>(p);
            auto offset = bytes - storage;

            assert(offset >= 0);

            auto byte_offset = static_cast<std::size_t>(offset);
            assert(byte_offset < 2 * sizeof(User));
            assert(byte_offset % sizeof(User) == 0);

            return byte_offset / sizeof(User);
        };

        auto create_user = [&](std::string name) -> User* {
            if (free_count == 0) {
                return nullptr;
            }

            std::size_t index = pop_free_index();
            return new (raw(index)) User(std::move(name));
        };

        auto destroy_user = [&](User* p) {
            std::size_t index = index_from_pointer(p);
            p->~User();
            push_free_index(index);
        };

        User* a = create_user("A");
        User* b = create_user("B");

        assert(free_count == 0);

        User* full = create_user("full");
        assert(full == nullptr);

        destroy_user(a);

        User* c = create_user("C");
        assert(c != nullptr);
        assert(static_cast<void*>(c) == static_cast<void*>(a));

        destroy_user(b);
        destroy_user(c);
        assert(free_count == 2);
    }

    {
        FixedPool<User, 2> pool;

        assert(pool.capacity() == 2);
        assert(pool.available() == 2);
        assert(pool.in_use() == 0);

        User* a = pool.create("A");
        User* b = pool.create("B");
        User* full = pool.create("full");

        assert(a != nullptr);
        assert(b != nullptr);
        assert(full == nullptr);
        assert(pool.available() == 0);
        assert(pool.in_use() == 2);

        pool.destroy(a);

        User* c = pool.create("C");
        assert(c != nullptr);
        assert(static_cast<void*>(c) == static_cast<void*>(a));

        pool.destroy(b);
        pool.destroy(c);

        assert(pool.available() == 2);
        assert(pool.in_use() == 0);
    }

    {
        FixedPool<User, 3> pool;

        User* a = pool.create("A");
        User* b = pool.create("B");
        User* c = pool.create("C");
        User* full = pool.create("full");

        assert(a != nullptr);
        assert(b != nullptr);
        assert(c != nullptr);
        assert(full == nullptr);
        assert(pool.in_use() == 3);

        pool.destroy(a);
        pool.destroy(b);
        pool.destroy(c);
    }

    {
        FixedPool<Widget, 2> pool;

        Widget* first = pool.create(42, "worker");
        Widget* second = pool.create(7, std::string{"listener"});

        assert(first != nullptr);
        assert(second != nullptr);

        assert(first->id == 42);
        assert(first->label == "worker");

        assert(second->id == 7);
        assert(second->label == "listener");

        pool.destroy(first);
        pool.destroy(second);

        assert(pool.in_use() == 0);
    }

    {
        FixedPool<ThrowingWidget, 1> pool;

        try {
            pool.create();
            assert(false);
        } catch (const std::runtime_error&) {
        }

        assert(pool.available() == 1);
        assert(pool.in_use() == 0);
    }

    return 0;
}
