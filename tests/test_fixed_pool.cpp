#include "fixed_pool.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct User {
    std::string name;

    explicit User(std::string name) : name{std::move(name)} {
    }
};

struct Widget {
    int id;
    std::string label;

    Widget(int id, std::string label) : id{id}, label{std::move(label)} {
    }
};

struct ThrowingWidget {
    ThrowingWidget() {
        throw std::runtime_error{"construction failed"};
    }
};

struct LifetimeTracked {
    int& live_count;

    explicit LifetimeTracked(int& count) : live_count{count} {
        ++live_count;
    }

    ~LifetimeTracked() {
        --live_count;
    }

    LifetimeTracked(const LifetimeTracked&) = delete;
    LifetimeTracked& operator=(const LifetimeTracked&) = delete;
};

struct MoveOnly {
    int value;

    explicit MoveOnly(int value) : value{value} {
    }

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
};

void test_initial_state_and_exhaustion() {
    FixedPool<User, 2> pool;

    assert(pool.capacity() == 2);
    assert(pool.available() == 2);
    assert(pool.in_use() == 0);

    User* first = pool.create("A");
    User* second = pool.create("B");
    User* full = pool.create("full");

    assert(first != nullptr);
    assert(second != nullptr);
    assert(full == nullptr);
    assert(pool.available() == 0);
    assert(pool.in_use() == 2);

    pool.destroy(first);
    pool.destroy(second);
}

void test_slot_reuse() {
    FixedPool<User, 2> pool;

    User* first = pool.create("A");
    User* second = pool.create("B");

    assert(first != nullptr);
    assert(second != nullptr);

    pool.destroy(first);

    User* reused = pool.create("C");
    assert(reused != nullptr);
    assert(static_cast<void*>(reused) == static_cast<void*>(first));

    pool.destroy(second);
    pool.destroy(reused);

    assert(pool.available() == 2);
    assert(pool.in_use() == 0);
}

void test_generic_capacity() {
    FixedPool<User, 3> pool;

    User* first = pool.create("A");
    User* second = pool.create("B");
    User* third = pool.create("C");
    User* full = pool.create("full");

    assert(first != nullptr);
    assert(second != nullptr);
    assert(third != nullptr);
    assert(full == nullptr);
    assert(pool.in_use() == 3);

    pool.destroy(first);
    pool.destroy(second);
    pool.destroy(third);
}

void test_perfect_forwarding() {
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
}

void test_constructor_exception_rolls_back_slot() {
    FixedPool<ThrowingWidget, 1> pool;

    try {
        pool.create();
        assert(false);
    } catch (const std::runtime_error&) {
    }

    assert(pool.available() == 1);
    assert(pool.in_use() == 0);
}

void test_object_lifetime() {
    int live_count = 0;
    FixedPool<LifetimeTracked, 2> pool;

    auto* first = pool.create(live_count);
    auto* second = pool.create(live_count);

    assert(first != nullptr);
    assert(second != nullptr);
    assert(live_count == 2);

    pool.destroy(first);
    assert(live_count == 1);

    pool.destroy(second);
    assert(live_count == 0);
}

void test_move_only_type() {
    FixedPool<MoveOnly, 1> pool;

    auto* object = pool.create(42);

    assert(object != nullptr);
    assert(object->value == 42);

    pool.destroy(object);
}

} // namespace

int main() {
    test_initial_state_and_exhaustion();
    test_slot_reuse();
    test_generic_capacity();
    test_perfect_forwarding();
    test_constructor_exception_rolls_back_slot();
    test_object_lifetime();
    test_move_only_type();
}
