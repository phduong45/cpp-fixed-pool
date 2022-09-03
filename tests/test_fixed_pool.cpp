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

} // namespace

int main() {
    test_initial_state_and_exhaustion();
    test_slot_reuse();
    test_generic_capacity();
    test_perfect_forwarding();
    test_constructor_exception_rolls_back_slot();
}
