// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
struct S { void m() {} };
int main() { auto d = tiny::bind<&S::m>(S{}); } // bind до тимчасового — заборонено
