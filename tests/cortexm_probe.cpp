// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
#include <type_traits>
// The ARM32 default reserves four words for inline storage.
static_assert(TINY_DELEGATE_DEFAULT_BYTES == 16u, "platform-scaled default");
static_assert(sizeof(tiny::delegate<void()>) == 32, "16B SBO + 3 ptrs, rounded to max_align_t(8)");
static_assert(sizeof(tiny::delegate_sbo<void()>) == 32, "owning layout");
static_assert(sizeof(tiny::delegate_ref<void()>) == 8, "two-word reference layout");
static_assert(std::is_nothrow_move_constructible_v<tiny::delegate<void()>>);
static_assert(std::is_trivially_copyable_v<tiny::delegate_ref<void()>>);
struct S { void m(int) {} };
static int increment(int x) { return x + 1; }
extern const tiny::delegate_ref<int(int)> cortex_rom[] = {
    tiny::delegate_ref<int(int)>::bind<&increment>()
};
extern "C" int cortex_rom_call(unsigned index, int value) { return cortex_rom[index](value); }
int main() {
    S s;
    auto d = tiny::delegate_ref<void(int)>::bind<&S::m>(s);
    d(2);
    tiny::delegate<void()> own{+[]{}};
    tiny::delegate<void()> moved{static_cast<tiny::delegate<void()>&&>(own)};
    moved();
    tiny::delegate_sbo<int(int)> state{[sum = 0](int x) mutable { return sum += x; }};
    (void)state.call_if(3);
    state.reset();
    (void)state.call_or(&increment, 3);
    return 0;
}
