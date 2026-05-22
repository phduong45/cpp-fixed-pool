# Fixed Memory Pool

A small C++20 fixed-capacity object pool used to study raw storage, placement
new, and object lifetime.

The pool owns storage for `N` objects of type `T`. `create()` constructs an
object in a free slot and returns a pointer. `destroy()` calls the destructor and
returns that slot to the pool.

## Example

```cpp
#include "fixed_pool.h"

struct Widget {
    int id;
};

FixedPool<Widget, 4> pool;

Widget* widget = pool.create(42);
if (widget) {
    pool.destroy(widget);
}
```

`create()` returns `nullptr` when the pool is full.

## Build

```bash
make debug
make release
```

## Test

```bash
make test
```

`make test` runs the test binary with AddressSanitizer and
UndefinedBehaviorSanitizer.

## Notes

- Fixed compile-time capacity: `FixedPool<T, N>`
- O(1) slot reuse through a free index stack
- Debug builds assert on invalid destroy and double destroy
- Intended for thread-local use: each thread owns its own pool
