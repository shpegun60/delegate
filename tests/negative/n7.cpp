// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
struct S { int m(int); };
int main() {
    // A member pointer without an instance has nothing to call on: the
    // zero-argument bind overloads accept a function pointer or a
    // (Method, Instance) pair, never a lone member pointer.
    auto d = tiny::delegate_ref<int(int)>::bind<&S::m>();
}
