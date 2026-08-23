// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <utility>
// Кастомна assert-політика: рахуємо спрацювання.
static int g_asserts = 0;
#define TINY_DELEGATE_ASSERT(expr, msg) do { if (!(expr)) { ++g_asserts; } } while (0)
#include "tiny_delegate.hpp"

int main()
{
    // 1) null fnptr: assert-хук спрацьовує, делегат стає порожнім (не UB).
    tiny::delegate<void()> d{static_cast<void (*)()>(nullptr)};
    assert(g_asserts == 1 && !d);

    // 2) delegate всередині delegate (вкладення через move) — працює як
    //    звичайний move-only callable.
    tiny::delegate_sbo<int(), 32> inner{[] { return 5; }};
    tiny::delegate_sbo<int(), 64> outer{std::move(inner)};
    assert(!inner && outer && outer() == 5);

    // 3) повторне вкладення після move — стани не плутаються.
    tiny::delegate_sbo<int(), 64> outer2{std::move(outer)};
    assert(!outer && outer2() == 5);

    std::puts("DELEGATE EXTRA: OK");
    return 0;
}
