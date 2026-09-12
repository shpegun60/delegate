// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
#include "check.hpp"
#include <memory>
#include <cstdio>

// 1) Reference results work with operator() and call_or.
static int g_cell = 7;
int& getCell() { return g_cell; }
static void refReturn() {
    tiny::delegate_ref<int&()> d{&getCell};
    d() = 42;                                   // write through the returned reference
    CHECK(g_cell == 42);
    int backup = 0;
    auto alt = [&backup]() -> int& { return backup; };
    tiny::delegate_ref<int&()> empty;
    empty.call_or(alt) = 5;                     // fallback also returns a reference
    CHECK(backup == 5);
}

// 2) Move-only arguments are forwarded through invocation and call_if.
static void moveOnlyArgs() {
    auto take = [](std::unique_ptr<int> p) { return *p; };
    tiny::delegate_sbo<int(std::unique_ptr<int>)> d{take};
    CHECK(d(std::make_unique<int>(9)) == 9);
    auto r = d.call_if(std::make_unique<int>(4));
    CHECK(r && *r == 4);
    static auto t2 = take;
    tiny::delegate_ref<int(std::unique_ptr<int>)> dr{tiny::borrow(t2)};
    CHECK(dr(std::make_unique<int>(3)) == 3);
}

// 3) noexcept functions support runtime pointers and static binding.
static int nxAdd(int a) noexcept { return a + 1; }
static void noexceptFns() {
    tiny::delegate_ref<int(int)> d1{&nxAdd};            // noexcept fp -> fp
    CHECK(d1(1) == 2);
    auto d2 = tiny::delegate_ref<int(int)>::bind<&nxAdd>();  // compile-time
    CHECK(d2(2) == 3);
    tiny::delegate<int(int)> d3{&nxAdd};                // owning path
    CHECK(d3(3) == 4);
}

// 4) A function NTTP also accepts the function name without an ampersand.
static int plain(int x) { return x * 3; }
static void bindNoAmp() {
    auto d = tiny::delegate_ref<int(int)>::bind<plain>();
    CHECK(d(3) == 9);
}

// 5) Compile-time binding supports const instances and methods.
struct CSvc { int v = 11; int get() const { return v; } };
static const CSvc g_csvc;
static void constInstance() {
    auto d = tiny::delegate_ref<int()>::bind<&CSvc::get, g_csvc>();
    CHECK(d() == 11);
}

// 6) An owning delegate may store a copy of a delegate_ref.
static void nestedRef() {
    static auto fn = [](int x) { return x + 100; };
    tiny::delegate_ref<int(int)> inner{tiny::borrow(fn)};
    tiny::delegate_sbo<int(int)> outer{inner};          // copy the view into the owner
    CHECK(outer(1) == 101);
}

int main()
{
    refReturn();
    moveOnlyArgs();
    noexceptFns();
    bindNoAmp();
    constInstance();
    nestedRef();
    std::puts("EDGE AUDIT: ALL OK");
    return 0;
}
