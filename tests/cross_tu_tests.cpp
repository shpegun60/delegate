// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "cross_tu_shared.hpp"
#include "check.hpp"
#include <cstdio>
#include <utility>
int main()
{
    D a = makeA();                 // constructed in translation unit A
    CHECK(a() == 1);
    CHECK(inlineA(a));            // queried from translation unit A
    D b = std::move(a);            // moved in translation unit B
    CHECK(!a && b() == 1 && b.uses_inline());
    b.reset();                     // destroyed through the manager from A
    CHECK(!b);
    CHECK(Shared::alive == 0);
    Shared target{4};
    auto borrowed = borrowA(target);
    CHECK(borrowed.non_owning() && nonOwningA(borrowed));
    auto moved = std::move(borrowed);
    CHECK(!borrowed && moved.non_owning() && nonOwningA(moved));
    auto reference = refA(target);
    target.value = 9;
    CHECK(reference() == 9 && moved() == 9);
    moved.reset();
    CHECK(Shared::alive == 1);
    std::puts("CROSS-TU (ODR) OK");
    return 0;
}
