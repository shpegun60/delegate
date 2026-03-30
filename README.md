# tiny_delegate

`tiny_delegate.hpp` is a compact C++17/C++20 callback library with three related delegate types:

- `tiny::delegate_ref<Sig>`: non-owning, cheap to copy and rebind
- `tiny::delegate_sbo<Sig, InlineBytes, InlineAlign>`: owning, move-only, SBO-first
- `tiny::delegate<Sig, InlineBytes, InlineAlign>`: hybrid, move-only, owns by default but can switch into explicit ref mode

It is aimed at projects that want a small, explicit, predictable callback abstraction without immediately defaulting to `std::function`.

## Screenshot

![tiny_delegate Qt demon test UI](img/qt.png)

## Features

- C++17/C++20 compatible
- No exceptions required by design
- No built-in assert policy forced on the user
- Move-only owning delegates
- Non-owning ref delegate
- Small-buffer optimization
- Optional heap fallback
- Explicit `borrow` and `bind` APIs
- Compile-time fit checks for size and alignment
- Works with move-only callables

## Header

```cpp
#include "tiny_delegate.hpp"
```

## The Three Main Types

### `tiny::delegate_ref<Sig>`

Non-owning callback view.

- Stores a free-function pointer, or
- stores a pointer to an external callable/object

Properties:

- copyable
- does not manage lifetime
- very cheap to copy and reassign

Use it when:

- the target object definitely outlives the callback
- you want maximum cheapness
- you do not want ownership at all

### `tiny::delegate_sbo<Sig, InlineBytes, InlineAlign>`

Owning callback.

Properties:

- move-only
- owns the callable
- stores inline when it fits
- can optionally fall back to heap if enabled
- has no borrow/ref mode

Use it when:

- you want ownership only
- you want the type itself to communicate "this callback owns its target"
- you want SBO behavior without the hybrid ref path

### `tiny::delegate<Sig, InlineBytes, InlineAlign>`

Hybrid callback.

Properties:

- move-only
- function pointer path for free functions and captureless non-generic lambdas
- owning path for normal lambdas/functors
- explicit ref path via `tiny::borrow(x)`
- explicit ref path via `tiny::bind<&T::method>(obj)`

Use it when:

- you want one type for most callback use cases
- sometimes you want ownership, sometimes borrow/bind

## Which Type Should I Use?

| Need | Recommended type |
|---|---|
| Pure non-owning callback view | `tiny::delegate_ref<Sig>` |
| Always own the callable | `tiny::delegate_sbo<Sig, ...>` |
| One general-purpose callback type | `tiny::delegate<Sig, ...>` |
| Embedded code with strict lifetime discipline | `tiny::delegate_ref<Sig>` or `tiny::delegate<Sig, ...>` with heap fallback off |

## Capability Matrix

| Source / behavior | `delegate_ref` | `delegate_sbo` | `delegate` |
|---|---|---|---|
| Free function pointer | yes | yes | yes |
| Captureless non-generic lambda | yes, through function-pointer conversion | yes | yes |
| Stateful lambda | yes, only via `tiny::borrow(lvalue)` | yes, as owned callable | yes, as owned callable |
| Move-only lambda | no | yes | yes |
| Functor object | yes, only via `tiny::borrow(lvalue)` | yes | yes |
| Direct `tiny::borrow(...)` API | yes | no | yes |
| Direct method-bind API | yes | no | yes |
| Copyable type | yes | no | no |
| Can use heap fallback | no | yes | yes |
| Can be in explicit ref mode | yes | no | yes |

## What Each Type Can Be Constructed From

### `delegate_ref`

Accepted forms:

- free-function pointer
- captureless non-generic lambda, via conversion to function pointer
- `tiny::borrow(functor_lvalue)`
- `tiny::borrow(generic_lambda_lvalue)`
- `tiny::delegate_ref<Sig>::bind<&T::method>(obj)`

Examples:

```cpp
int plus_one(int x) { return x + 1; }

struct Functor {
    int operator()(int x) { return x + 10; }
};

struct Device {
    int read(int x) { return x + 100; }
};

Functor fn;
Device dev;

tiny::delegate_ref<int(int)> a = &plus_one;
tiny::delegate_ref<int(int)> b = tiny::borrow(fn);
tiny::delegate_ref<int(int)> c =
    tiny::delegate_ref<int(int)>::bind<&Device::read>(dev);
```

Not supported:

- owning a temporary lambda/functor
- move-only callable ownership
- heap fallback

### `delegate_sbo`

Accepted forms:

- free-function pointer
- captureless non-generic lambda
- generic lambda, as owned callable
- stateful lambda
- move-only lambda
- functor object

Examples:

```cpp
tiny::delegate_sbo<int(int), 64> a = &plus_one;
tiny::delegate_sbo<int(int), 64> b = [](int x) { return x + 2; };
tiny::delegate_sbo<int(int), 64> c = [sum = 0](int x) mutable { return sum += x; };
tiny::delegate_sbo<int(int), 64> d = Functor{};
```

Not supported:

- `tiny::borrow(...)`
- `tiny::bind<&T::method>(obj)`
- ref mode of any kind

If you really need to call an external object through `delegate_sbo`, you can still wrap it manually:

```cpp
struct Device {
    int read(int x) { return x + 1; }
};

Device dev;

tiny::delegate_sbo<int(int), 64> cb = [&dev](int x) {
    return dev.read(x);
};
```

But note what this means:

- `delegate_sbo` owns the lambda wrapper
- it does not own `dev`
- `dev` must still outlive callback use
- this is not the same as library-level `borrow` / `bind`

### `delegate`

Accepted forms:

- free-function pointer
- captureless non-generic lambda
- generic lambda, as owned callable
- stateful lambda
- move-only lambda
- functor object
- `tiny::borrow(functor_lvalue)`
- `tiny::bind<&T::method>(obj)`

Examples:

```cpp
tiny::delegate<int(int)> a = &plus_one;
tiny::delegate<int(int)> b = [](int x) { return x + 2; };
tiny::delegate<int(int)> c = [sum = 0](int x) mutable { return sum += x; };
tiny::delegate<int(int)> d = Functor{};
tiny::delegate<int(int)> e = tiny::borrow(fn);
tiny::delegate<int(int)> f = tiny::bind<&Device::read>(dev);
```

This is the "automatic chooser" type:

- if the input behaves like a function pointer, it uses the function-pointer path
- if the input is a normal callable object, it owns it
- if the input is `borrow(...)`, it becomes non-owning
- if the input is `bind(...)`, it becomes non-owning method binding

## Configuration Macros

Define these before including the header.

```cpp
#define TINY_DELEGATE_DEFAULT_BYTES 64
#define TINY_DELEGATE_DEFAULT_ALIGN alignof(std::max_align_t)
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 0
#define TINY_DELEGATE_ASSERT(expr, msg) ((void)0)
#include "tiny_delegate.hpp"
```

### `TINY_DELEGATE_DEFAULT_BYTES`

Default inline byte capacity used by:

- `tiny::delegate<Sig>`
- free `tiny::bind<&T::method>(obj)` helper

Default:

```cpp
64
```

### `TINY_DELEGATE_DEFAULT_ALIGN`

Default inline alignment used by `tiny::delegate<Sig>`.

Default:

```cpp
alignof(std::max_align_t)
```

### `TINY_DELEGATE_ENABLE_HEAP_FALLBACK`

Controls behavior when an owning callable does not fit inline.

- `0`: compile-time failure
- `1`: allocate on heap

Default:

```cpp
0
```

### `TINY_DELEGATE_ASSERT(expr, msg)`

User hook for runtime assertions.

Default:

```cpp
#define TINY_DELEGATE_ASSERT(expr, msg) ((void)0)
```

Example:

```cpp
#include <cassert>
#define TINY_DELEGATE_ASSERT(expr, msg) assert((expr) && (msg))
#include "tiny_delegate.hpp"
```

## Quick Start

```cpp
#include "tiny_delegate.hpp"

int plus_one(int x) { return x + 1; }

struct Worker {
    int base = 10;
    int add(int x) { return base + x; }
};

int main() {
    tiny::delegate<int(int)> a = &plus_one;

    Worker w{20};
    tiny::delegate<int(int)> b = tiny::bind<&Worker::add>(w);

    auto counter = [sum = 0](int x) mutable {
        sum += x;
        return sum;
    };
    tiny::delegate<int(int)> c = counter;

    return a(1) + b(2) + c(3);
}
```

## About `auto`

Yes, you can use `auto` for local variables when the initializer already creates a delegate object.

Examples:

```cpp
auto counter = [sum = 0](int x) mutable { return sum += x; };
Worker w{20};

auto a = tiny::delegate<int(int)>{&plus_one};
auto b = tiny::delegate_sbo<int(int), 64>{[](int x) { return x + 2; }};
auto c = tiny::delegate_ref<int(int)>{tiny::borrow(counter)};
auto d = tiny::bind<&Worker::add>(w);
```

Important:

```cpp
auto cb = &plus_one;
```

This is only a raw function pointer:

```cpp
int (*)(int)
```

It is not a `tiny::delegate`.

So these are different:

```cpp
auto a = &plus_one;                         // raw function pointer
auto b = tiny::delegate<int(int)>{&plus_one}; // delegate object
```

Practical rule:

- for API surface, class members, typedefs and examples that teach the library, prefer the explicit delegate type
- for local variables, `auto` is fine if the initializer already makes the delegate type obvious

## Basic Semantics

### Empty delegates

```cpp
tiny::delegate<void()> cb;

if (!cb) {
    // empty
}

cb = nullptr;
```

The same idea works for:

- `tiny::delegate_ref<Sig>`
- `tiny::delegate_sbo<Sig, ...>`
- `tiny::delegate<Sig, ...>`

Calling an empty delegate is only guarded by `TINY_DELEGATE_ASSERT`, so if your project wants hard failure on misuse, override that macro.

### Copy and move rules

- `tiny::delegate_ref<Sig>` is copyable
- `tiny::delegate_sbo<Sig, ...>` is move-only
- `tiny::delegate<Sig, ...>` is move-only

Owning delegates are move-only so they can hold move-only callables.

## Usage Examples

### 1. Free function

```cpp
int on_value(int x) { return x + 5; }

tiny::delegate<int(int)> cb = &on_value;
int y = cb(7); // 12
```

This uses the function-pointer path.

### 2. Captureless non-generic lambda

```cpp
tiny::delegate<int(int)> cb = [](int x) { return x * 2; };
int y = cb(9); // 18
```

Because the lambda is captureless and non-generic, it converts to a function pointer.

### 3. Stateful lambda with owned copy

```cpp
auto stateful = [sum = 0](int x) mutable {
    sum += x;
    return sum;
};

tiny::delegate<int(int)> cb = stateful;

int a = cb(1);      // 1
int b = cb(2);      // 3
int c = stateful(5); // 5, separate state
```

Important:

- `stateful` keeps its own state
- `cb` stores its own copied state

This is expected behavior.

### 4. Move-only lambda

```cpp
#include <memory>

tiny::delegate<int(int)> cb = [ptr = std::make_unique<int>(40)](int x) {
    return *ptr + x;
};

int y = cb(2); // 42
```

Also works with `delegate_sbo`:

```cpp
tiny::delegate_sbo<int(int), 64> cb = [ptr = std::make_unique<int>(7)](int x) {
    return *ptr + x;
};
```

### 5. Pure non-owning callable with `delegate_ref`

```cpp
struct Accumulator {
    int total = 0;

    int operator()(int x) {
        total += x;
        return total;
    }
};

Accumulator acc;
tiny::delegate_ref<int(int)> ref = tiny::borrow(acc);

int a = ref(3); // 3
int b = ref(4); // 7
```

`ref` does not own `acc`.

### 6. Non-owning callable with `delegate`

```cpp
Accumulator acc;
tiny::delegate<int(int)> cb = tiny::borrow(acc);

int a = cb(3); // 3
int b = cb(4); // 7

bool is_ref = cb.non_owning(); // true
bool own = cb.owning();        // false
bool inl = cb.uses_inline();   // false
bool heap = cb.uses_heap();    // false
```

This is the hybrid delegate in explicit ref mode.

### 7. Borrowing a const functor

```cpp
struct Adder {
    int bias = 10;
    int operator()(int x) const { return bias + x; }
};

const Adder add{};
tiny::delegate<int(int)> cb = tiny::borrow(add);

int y = cb(5); // 15
```

This works because the callable is invocable as `const`.

### 8. Binding a non-const member function

```cpp
struct Device {
    int base = 100;
    int read(int x) { return base + x; }
};

Device dev{100};
tiny::delegate<int(int)> cb = tiny::bind<&Device::read>(dev);

int y = cb(23); // 123
```

### 9. Binding a const member function

```cpp
struct Sensor {
    int base = 50;
    int sample(int x) const { return base + x; }
};

const Sensor s{50};
tiny::delegate<int(int)> cb = tiny::bind<&Sensor::sample>(s);

int y = cb(2); // 52
```

### 10. `delegate_ref::bind`

If you want a pure ref delegate for a method:

```cpp
struct Driver {
    void tick() {}
};

Driver d;
tiny::delegate_ref<void()> ref =
    tiny::delegate_ref<void()>::bind<&Driver::tick>(d);
```

### 11. Owning-only delegate with `delegate_sbo`

```cpp
using Task = tiny::delegate_sbo<void(), 32>;

Task t = [] {
    // owned callable
};
```

This type never enters borrow/bind ref mode.

### 12. Custom-sized owning delegate

```cpp
using SmallCb = tiny::delegate<int(int), 16>;

SmallCb cb = [](int x) { return x + 4; };
int y = cb(3); // 7
```

If the callable does not fit and heap fallback is off, compilation fails.

### 13. Over-aligned callable

Sometimes a callable fits by size but not by alignment.

```cpp
struct alignas(32) BigAlign {
    int operator()(int x) const { return x + 1; }
};
```

On many platforms the default inline alignment is `16`, so this does not fit inline by default.

Use a larger alignment if you want inline storage:

```cpp
using AlignedCb = tiny::delegate<int(int), 64, 32>;

AlignedCb cb = BigAlign{};
int y = cb(5); // 6
```

The same idea applies to `tiny::delegate_sbo`.

### 14. Compile-time fit checks

```cpp
struct MyFunctor {
    int operator()(int) const { return 0; }
};

using D = tiny::delegate<int(int), 64>;

static_assert(D::fits_inline<MyFunctor>());
static_assert(D::required_inline_bytes<MyFunctor>() == sizeof(MyFunctor));
static_assert(D::required_inline_align<MyFunctor>() == alignof(MyFunctor));
```

If you want a clearer compile-time failure:

```cpp
using D = tiny::delegate<int(int), 64, 16>;
D::static_assert_fits_inline<MyFunctor>();
```

Also available on `tiny::delegate_sbo`.

### 15. Oversized callable with heap fallback on

```cpp
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1
#include "tiny_delegate.hpp"

struct Large {
    char payload[256]{};
    int operator()(int x) const { return x + 1; }
};

tiny::delegate<int(int), 64> cb = Large{};

bool heap = cb.uses_heap();    // true
bool inl = cb.uses_inline();   // false
```

The same pattern works for `tiny::delegate_sbo`.

### 16. Oversized callable with heap fallback off

With the default policy:

```cpp
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 0
#include "tiny_delegate.hpp"
```

an oversized or over-aligned owning callable becomes a compile-time error.

This is often exactly what embedded code wants.

### 17. Resetting with `nullptr`

```cpp
tiny::delegate<void()> cb = [] {};
cb = nullptr;

if (!cb) {
    // empty again
}
```

The same works for:

- `tiny::delegate_ref<Sig>`
- `tiny::delegate_sbo<Sig, ...>`

### 18. Rebinding

```cpp
int plus_one(int x) { return x + 1; }
int plus_two(int x) { return x + 2; }

tiny::delegate<int(int)> cb = &plus_one;
cb = &plus_two;
cb = nullptr;
cb = &plus_one;
```

Rebinding an owning delegate destroys the previous owned target and installs the new one.

### 19. `sig_of_t` for signature deduction

```cpp
int plus_one(int x) { return x + 1; }

struct Worker {
    int run(int x) const { return x; }
};

using Sig1 = tiny::sig_of_t<decltype(&plus_one)>;     // int(int)
using Sig2 = tiny::sig_of_t<decltype(&Worker::run)>;  // int(int)
```

The free `tiny::bind<&T::method>(obj)` helper uses this same idea internally.

### 20. Free `bind` vs typed `bind`

Free helper:

```cpp
auto cb = tiny::bind<&Device::read>(dev);
```

Return type:

```cpp
tiny::delegate<tiny::sig_of_t<decltype(&Device::read)>>
```

That means the free helper uses the global defaults:

- `TINY_DELEGATE_DEFAULT_BYTES`
- `TINY_DELEGATE_DEFAULT_ALIGN`

If you need custom inline capacity or alignment, use the typed form:

```cpp
using FastCb = tiny::delegate<int(int), 32, 32>;
FastCb cb = FastCb::bind<&Device::read>(dev);
```

### 21. Generic lambda

Generic lambdas are not function pointers, but they still work as callable objects if the requested signature is valid.

Owned case:

```cpp
auto generic = [](auto x) { return x + 1; };

tiny::delegate<int(int)> a = generic;
tiny::delegate_sbo<int(int), 64> b = generic;
```

Borrowed case:

```cpp
tiny::delegate_ref<int(int)> ref = tiny::borrow(generic);
tiny::delegate<int(int)> borrowed = tiny::borrow(generic);
```

The important distinction is:

- captureless non-generic lambda can use the function-pointer path
- generic lambda is treated like a normal functor object

## Example Cookbook

This section is intentionally repetitive. It is here so the README can be used as a quick reference when you are trying to remember "can I do X with this delegate type?".

### 22. Void callback

```cpp
tiny::delegate<void()> cb = [] {
    // do something
};

if (cb) {
    cb();
}
```

### 23. Callback with multiple arguments

```cpp
tiny::delegate<int(int, int)> add = [](int a, int b) {
    return a + b;
};

int sum = add(2, 3); // 5
```

### 24. Store a delegate as a class member

```cpp
class JobQueue {
public:
    using DoneCb = tiny::delegate<void(int), 32>;

    void setDoneCallback(DoneCb cb) {
        done_ = std::move(cb);
    }

    void finish(int code) {
        if (done_) done_(code);
    }

private:
    DoneCb done_;
};
```

### 25. Pass a delegate into a function

```cpp
using Callback = tiny::delegate<void(int), 32>;

void subscribe(Callback cb) {
    if (cb) cb(42);
}

subscribe([](int value) {
    // value == 42
});
```

### 26. Return a delegate from a factory

```cpp
tiny::delegate<int(int)> make_scaler(int base) {
    return [base](int x) {
        return base * x;
    };
}

auto cb = make_scaler(3);
int y = cb(4); // 12
```

### 27. Return a bound method

```cpp
struct Device {
    int base = 7;
    int read(int x) const { return base + x; }
};

tiny::delegate<int(int)> make_reader(const Device& dev) {
    return tiny::bind<&Device::read>(dev);
}
```

The object must still outlive the returned delegate.

### 28. `delegate_ref` copied to multiple views

```cpp
struct Counter {
    int total = 0;

    int operator()(int x) {
        total += x;
        return total;
    }
};

Counter counter;

tiny::delegate_ref<int(int)> a = tiny::borrow(counter);
tiny::delegate_ref<int(int)> b = a;

int x = a(2); // 2
int y = b(3); // 5
```

Both refs target the same external object.

### 29. Switch one `delegate` between owning and ref modes

```cpp
struct Device {
    int base = 100;
    int operator()(int x) { return base + x; }
    int read(int x) { return base + x; }
};

Device dev;
tiny::delegate<int(int)> cb = [](int x) { return x + 1; }; // owning

cb = tiny::borrow(dev); // ref mode through operator()
cb = tiny::bind<&Device::read>(dev); // ref mode by method binding
cb = [value = 5](int x) { return value + x; }; // back to owning
```

`tiny::delegate` is the only one of the three types that can switch like this.

### 30. Capture by reference

```cpp
int external = 10;

tiny::delegate<int()> cb = [&external] {
    return external;
};

external = 20;
int y = cb(); // 20
```

The delegate owns the lambda object, but the lambda object still refers to `external`.

### 31. Mutable lambda with internal state

```cpp
tiny::delegate<int(int)> cb = [count = 0](int x) mutable {
    count += x;
    return count;
};

int a = cb(1); // 1
int b = cb(1); // 2
int c = cb(5); // 7
```

### 32. Method callback for a scheduler

```cpp
struct Task {
    void run() {
        // do work
    }
};

class Scheduler {
public:
    using Callback = tiny::delegate<void(), 32>;

    void set(Callback cb) {
        cb_ = std::move(cb);
    }

    void tick() {
        if (cb_) cb_();
    }

private:
    Callback cb_;
};

Task task;
Scheduler sch;
sch.set(tiny::bind<&Task::run>(task));
```

### 33. Ref-only callback for a scheduler

```cpp
struct Task {
    void run() {}
};

class Scheduler {
public:
    using Callback = tiny::delegate_ref<void()>;

    void set(Callback cb) { cb_ = cb; }
    void tick() { if (cb_) cb_(); }

private:
    Callback cb_;
};

Task task;
Scheduler sch;
sch.set(tiny::delegate_ref<void()>::bind<&Task::run>(task));
```

### 34. Use `delegate_sbo` when ownership must be explicit

```cpp
class WorkerPool {
public:
    using Job = tiny::delegate_sbo<void(), 48>;

    void setJob(Job job) {
        job_ = std::move(job);
    }

    void execute() {
        if (job_) job_();
    }

private:
    Job job_;
};
```

### 35. Check whether heap fallback was used

```cpp
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1
#include "tiny_delegate.hpp"

struct Large {
    char payload[256]{};
    void operator()() const {}
};

tiny::delegate<void(), 64> cb = Large{};

if (cb.uses_heap()) {
    // oversized callable went to heap
}
```

Only meaningful when heap fallback is enabled.

### 36. Check whether `delegate_sbo` stayed inline

```cpp
tiny::delegate_sbo<void(), 64> cb = [] {
    // small callable
};

bool inl = cb.uses_inline();
bool heap = cb.uses_heap();
```

### 37. Use the convenience aliases

```cpp
tiny::delegate64<void()> a = [] {};
tiny::delegate32<int(int)> b = [](int x) { return x + 1; };

tiny::delegate_sbo64<void()> c = [] {};
tiny::delegate_sbo32<int(int)> d = [](int x) { return x + 2; };
```

These are only shorthand for common byte sizes.

### 38. Explicit typed `bind` when you need custom alignment

```cpp
struct Device {
    int read(int x) const { return x + 1; }
};

using Callback = tiny::delegate<int(int), 64, 32>;

Device dev;
Callback cb = Callback::bind<&Device::read>(dev);
```

### 39. Temporary objects are intentionally rejected for `borrow`

```cpp
auto ok_lambda = [count = 0](int x) mutable { return count += x; };
auto ok = tiny::borrow(ok_lambda); // ok

// auto bad = tiny::borrow([count = 0](int x) mutable { return count += x; });
// error: cannot borrow a temporary
```

### 40. Temporary objects are intentionally rejected for `bind`

```cpp
struct Device {
    int read(int x) const { return x + 1; }
};

Device dev;
auto ok = tiny::bind<&Device::read>(dev); // ok

// auto bad = tiny::bind<&Device::read>(Device{});
// error: cannot bind a temporary
```

### 41. Generic lambda with explicit borrow

```cpp
auto generic = [](auto x) { return x + 10; };

tiny::delegate_ref<int(int)> ref = tiny::borrow(generic);
tiny::delegate<int(int)> cb = tiny::borrow(generic);
```

This stays non-owning.

### 42. Generic lambda as owned callback

```cpp
auto generic = [](auto x) { return x * 2; };

tiny::delegate<int(int)> cb = generic;
tiny::delegate_sbo<int(int), 64> box = generic;
```

This stores the generic lambda as a normal callable object.

### 43. Reconfigure the same API between `delegate_ref` and `delegate`

```cpp
struct Device {
    int read(int x) { return x + 1; }
};

class FastApi {
public:
    using Callback = tiny::delegate_ref<int(int)>;
    void set(Callback cb) { cb_ = cb; }
private:
    Callback cb_;
};

class FlexibleApi {
public:
    using Callback = tiny::delegate<int(int), 32>;
    void set(Callback cb) { cb_ = std::move(cb); }
private:
    Callback cb_;
};
```

Same overall pattern, different ownership model.

### 44. Factory with `auto` and explicit delegate construction

```cpp
int plus_one(int x) { return x + 1; }

auto make_callback() {
    return tiny::delegate<int(int)>{&plus_one};
}
```

This is a good use of `auto`, because the returned object is already a delegate.

### 45. Local `auto` from free `bind`

```cpp
struct Device {
    int read(int x) const { return x + 5; }
};

Device dev;
auto cb = tiny::bind<&Device::read>(dev);
```

Here `auto` is fine because `tiny::bind` already returns a delegate object.

### 46. Local `auto` that is not a delegate

```cpp
int plus_one(int x) { return x + 1; }

auto raw = &plus_one; // raw function pointer, not tiny::delegate
```

This is the main `auto` pitfall to remember.

## Introspection APIs

### `tiny::delegate`

```cpp
tiny::delegate<int(int)> cb;

bool empty = !cb;
bool ref = cb.non_owning();
bool own = cb.owning();
bool inl = cb.uses_inline();
bool heap = cb.uses_heap();
```

Meaning:

- `non_owning()`: callback is in borrow/bind ref mode
- `owning()`: callback owns something
- `uses_inline()`: callback owns inline storage
- `uses_heap()`: callback owns heap storage

Important:

If `cb` is in ref mode, then:

```cpp
cb.non_owning(); // true
cb.owning();     // false
cb.uses_inline();// false
cb.uses_heap();  // false
```

`uses_inline()` is intentionally about owned inline storage, not "anything that is not heap".

### `tiny::delegate_sbo`

```cpp
tiny::delegate_sbo<int(int), 64> cb = [](int x) { return x + 1; };

bool inl = cb.uses_inline();
bool heap = cb.uses_heap();
```

## Practical Patterns

### Event subscription

```cpp
class Button {
public:
    using Callback = tiny::delegate<void(), 32>;

    void setOnClick(Callback cb) {
        onClick_ = std::move(cb);
    }

    void click() {
        if (onClick_) onClick_();
    }

private:
    Callback onClick_;
};
```

Usage:

```cpp
Button b;
b.setOnClick([] {
    // ...
});
```

### Ref-only hot path

```cpp
class Dispatcher {
public:
    using RxCb = tiny::delegate_ref<void(const std::uint8_t*, std::size_t)>;

    void setHandler(RxCb cb) { handler_ = cb; }

    void receive(const std::uint8_t* data, std::size_t size) {
        if (handler_) handler_(data, size);
    }

private:
    RxCb handler_;
};
```

This is a good fit when:

- lifetime is guaranteed externally
- rebinding should be cheap
- you want no ownership overhead

### Embedded / STM32-friendly policy

```cpp
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 0
#define TINY_DELEGATE_DEFAULT_BYTES 64
#define TINY_DELEGATE_DEFAULT_ALIGN alignof(std::max_align_t)
#include "tiny_delegate.hpp"
```

Effect:

- small callables fit inline
- oversized callables fail at compile time
- no hidden heap allocation

For extremely hot paths with stable lifetime, `tiny::delegate_ref` is often the cleanest choice.

## Lifetime Rules

This section is the most important one.

### `tiny::borrow(x)` requires a long-lived lvalue

Correct:

```cpp
auto counter = [sum = 0](int x) mutable { return sum += x; };
tiny::delegate<int(int)> cb = tiny::borrow(counter);
```

Wrong:

```cpp
tiny::delegate<int(int)> cb =
    tiny::borrow([sum = 0](int x) mutable { return sum += x; }); // error
```

Borrowing a temporary is forbidden.

### `tiny::bind<&T::method>(obj)` requires a long-lived lvalue object

Correct:

```cpp
Device dev;
auto cb = tiny::bind<&Device::read>(dev);
```

Wrong:

```cpp
auto cb = tiny::bind<&Device::read>(Device{}); // error
```

Binding a temporary is forbidden.

### Owning a lambda is not the same as owning the external data it references

```cpp
int external = 10;
tiny::delegate<int()> cb = [&external] {
    return external;
};
```

The delegate owns the lambda object.

But the lambda object still refers to `external`, so `external` must outlive callback use.

## Alignment and Size Rules

Inline fit depends on both:

- `sizeof(callable) <= InlineBytes`
- `alignof(callable) <= InlineAlign`

That means:

- enough bytes is not enough
- over-aligned callables may require custom `InlineAlign`

This is expected behavior, not a bug.

## Convenience Aliases

```cpp
template <class Sig> using delegate64 = delegate<Sig, 64>;
template <class Sig> using delegate32 = delegate<Sig, 32>;

template <class Sig> using delegate_sbo64 = delegate_sbo<Sig, 64>;
template <class Sig> using delegate_sbo32 = delegate_sbo<Sig, 32>;
```

These are only convenience aliases.

## What This Library Does Not Do

- It does not manage lifetime of borrowed/bound targets
- It does not emulate `std::bind` full argument binding
- It does not copy owning delegates
- It does not silently heap-allocate unless you enabled heap fallback

## Common Pitfalls

### "Why is my original lambda state different from the delegate state?"

Because:

```cpp
auto lambda = [count = 0](int x) mutable { return count += x; };
tiny::delegate<int(int)> cb = lambda;
```

stores a copy of `lambda`.

### "Why does my over-aligned callable not fit?"

Because alignment is checked independently from size.

Use a larger alignment:

```cpp
using D = tiny::delegate<int(int), 64, 32>;
```

### "Why did free `bind` use the default inline size?"

Because free `tiny::bind<&T::method>(obj)` always returns `tiny::delegate<sig>` with the global defaults.

If you need a custom configuration, use:

```cpp
using D = tiny::delegate<int(int), 32, 32>;
D cb = D::bind<&T::method>(obj);
```

## Summary

Use:

- `tiny::delegate_ref<Sig>` for non-owning callback views
- `tiny::delegate_sbo<Sig, ...>` for explicit owning SBO delegates
- `tiny::delegate<Sig, ...>` for the general case

Use:

- `tiny::borrow(x)` when lifetime is guaranteed externally
- `tiny::bind<&T::method>(obj)` when you want to bind only the object
- `fits_inline`, `required_inline_bytes`, `required_inline_align`, and `static_assert_fits_inline` when you want compile-time confidence

For embedded systems, keeping heap fallback off is usually the most honest and predictable policy.
