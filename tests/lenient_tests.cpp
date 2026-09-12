// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "check.hpp"
#include <cstdio>
#include <utility>
// Count assertion failures without terminating.
static int g_asserts = 0;
#define TINY_DELEGATE_ASSERT(expr, msg) do { if (!(expr)) { ++g_asserts; } } while (0)
#include "tiny_delegate.hpp"

#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
struct NullAllocation {
    static inline int constructions = 0;
    char data[128]{};
    NullAllocation() { ++constructions; }
    NullAllocation(const NullAllocation&) { ++constructions; }
    NullAllocation(NullAllocation&&) noexcept { ++constructions; }
    void operator()() const {}
    static void* operator new(std::size_t) noexcept { return nullptr; }
    static void operator delete(void*) noexcept {}
};
#endif

int main()
{
    // A null function pointer triggers the hook and leaves an empty delegate.
    tiny::delegate<void()> d{static_cast<void (*)()>(nullptr)};
    CHECK(g_asserts == 1 && !d);

    // Null pointers adapted to a different return/argument type must not
    // bypass the guard merely because the generic constructor wins.
    tiny::delegate<void()> adapted{static_cast<int (*)()>(nullptr)};
    tiny::delegate_sbo<void()> adapted_sbo{static_cast<int (*)()>(nullptr)};
    tiny::delegate_ref<void()> ref{static_cast<void (*)()>(nullptr)};
    CHECK(g_asserts == 4 && !adapted && !adapted_sbo && !ref);
    struct Target { int method() { return 1; } };
    tiny::delegate<int(Target&)> member{static_cast<int (Target::*)()>(nullptr)};
    tiny::delegate_sbo<int(Target&)> member_sbo{static_cast<int (Target::*)()>(nullptr)};
    CHECK(g_asserts == 6 && !member && !member_sbo);

    auto f = [] {};
    ref = tiny::borrow(f);
    d = tiny::borrow(f);
    ref = tiny::borrow_t<decltype(f)>{nullptr};
    d = tiny::borrow_t<decltype(f)>{nullptr};
    CHECK(g_asserts == 8 && !ref && !d && !d.call_if());

#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
    NullAllocation source;
    tiny::delegate<void()> no_memory{source};
    tiny::delegate_sbo<void()> no_memory_sbo{source};
    CHECK(g_asserts == 10 && !no_memory && !no_memory_sbo);
    CHECK(NullAllocation::constructions == 1);
#endif

    // A larger wrapper may own a smaller delegate moved into it.
    tiny::delegate_sbo<int(), 32> inner{[] { return 5; }};
    tiny::delegate_sbo<int(), 64> outer{std::move(inner)};
    CHECK(!inner && outer && outer() == 5);

    // A second move preserves the nested target.
    tiny::delegate_sbo<int(), 64> outer2{std::move(outer)};
    CHECK(!outer && outer2() == 5);

    std::puts("DELEGATE EXTRA: OK");
    return 0;
}
