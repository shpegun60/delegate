// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
static int g;
int& get() { return g; }
int main() {
    tiny::delegate_ref<int&()> d{&get};
    auto v = d.call_if();   // optional<int&> неможливий: чітка відмова
}
