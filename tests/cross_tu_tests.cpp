// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
#include <cassert>
#include <cstdio>
#include <utility>
using D = tiny::delegate_sbo<int()>;
struct Shared { int v; int operator()() const { return v; } };
D makeA();
bool inlineA(const D& d);
int main()
{
    D a = makeA();                 // створено в TU A
    assert(a() == 1);
    assert(inlineA(a));            // TU A бачить той самий менеджер
    D b = std::move(a);            // move виконує TU B над об'єктом з TU A
    assert(!a && b() == 1 && b.uses_inline());
    b.reset();                     // деструктор через менеджер із TU A
    assert(!b);
    std::puts("CROSS-TU (ODR) OK");
    return 0;
}
