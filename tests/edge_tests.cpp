// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "tiny_delegate.hpp"
#include <cassert>
#include <memory>
#include <cstdio>

// 1) Reference-повертаюча сигнатура: operator() і call_or ПРАЦЮЮТЬ
static int g_cell = 7;
int& getCell() { return g_cell; }
static void refReturn() {
    tiny::delegate_ref<int&()> d{&getCell};
    d() = 42;                                   // пишемо через повернене посилання
    assert(g_cell == 42);
    int backup = 0;
    auto alt = [&backup]() -> int& { return backup; };
    tiny::delegate_ref<int&()> empty;
    empty.call_or(alt) = 5;                     // fallback теж повертає посилання
    assert(backup == 5);
}

// 2) Move-only аргументи наскрізь (усі три типи + call_if)
static void moveOnlyArgs() {
    auto take = [](std::unique_ptr<int> p) { return *p; };
    tiny::delegate_sbo<int(std::unique_ptr<int>)> d{take};
    assert(d(std::make_unique<int>(9)) == 9);
    auto r = d.call_if(std::make_unique<int>(4));
    assert(r && *r == 4);
    static auto t2 = take;
    tiny::delegate_ref<int(std::unique_ptr<int>)> dr{tiny::borrow(t2)};
    assert(dr(std::make_unique<int>(3)) == 3);
}

// 3) noexcept-функції: runtime fnptr і compile-time bind
static int nxAdd(int a) noexcept { return a + 1; }
static void noexceptFns() {
    tiny::delegate_ref<int(int)> d1{&nxAdd};            // noexcept fp -> fp
    assert(d1(1) == 2);
    auto d2 = tiny::delegate_ref<int(int)>::bind<&nxAdd>();  // compile-time
    assert(d2(2) == 3);
    tiny::delegate<int(int)> d3{&nxAdd};                // owning шлях
    assert(d3(3) == 4);
}

// 4) bind<fn> БЕЗ амперсанда (function-to-pointer decay в auto-параметрі)
static int plain(int x) { return x * 3; }
static void bindNoAmp() {
    auto d = tiny::delegate_ref<int(int)>::bind<plain>();
    assert(d(3) == 9);
}

// 5) const-інстанс + const-метод у compile-time bind
struct CSvc { int v = 11; int get() const { return v; } };
static const CSvc g_csvc;
static void constInstance() {
    auto d = tiny::delegate_ref<int()>::bind<&CSvc::get, g_csvc>();
    assert(d() == 11);
}

// 6) вкладення delegate_ref у delegate_sbo (ref — копійований callable)
static void nestedRef() {
    static auto fn = [](int x) { return x + 100; };
    tiny::delegate_ref<int(int)> inner{tiny::borrow(fn)};
    tiny::delegate_sbo<int(int)> outer{inner};          // копія ref всередину
    assert(outer(1) == 101);
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
