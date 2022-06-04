#include <cstddef>
#include <iostream>
#include <new>
#include <string>

struct User {
    std::string name_;

    User(std::string name) : name_{std::move(name)} {
        std::cout << "construct " << name_ << "\n";
    }

    ~User() { std::cout << "destroy " << name_ << "\n"; }
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
        User* p = new (buffer) User("placement");

        // Placement new does not own the storage, so we destroy explicitly.
        p->~User();
    }

    {
        alignas(User) std::byte buffer[sizeof(User)];
        User* first = new (buffer) User("first");
        std::cout << "address of first: " << static_cast<void*>(first) << "\n";
        first->~User();

        User* second = new (buffer) User("second");
        std::cout << "address of second: " << static_cast<void*>(second) << "\n";
        second->~User();
    }

    return 0;
}
