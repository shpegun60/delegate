// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
int main() {
    tiny::delegate<void()> a{+[]{}};
    tiny::delegate<void()> b = a;  // копіювання заборонено (move-only)
}
