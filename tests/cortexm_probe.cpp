// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
#include <type_traits>
// Новий дефолт на ARM32: 4*4=16 байтів SBO.
static_assert(TINY_DELEGATE_DEFAULT_BYTES == 16u, "platform-scaled default");
static_assert(sizeof(tiny::delegate<void()>) == 32, "16B SBO + 3 ptrs, rounded to max_align_t(8)");
static_assert(std::is_nothrow_move_constructible_v<tiny::delegate<void()>>);
struct S { void m(int) {} };
int main() {
    S s;
    auto d = tiny::delegate_ref<void(int)>::bind<&S::m>(s);
    d(2);
    tiny::delegate<void()> own{+[]{}};
    tiny::delegate<void()> moved{static_cast<tiny::delegate<void()>&&>(own)};
    moved();
    return 0;
}
