// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
using D = tiny::delegate_sbo<int()>;
struct Shared { int v; int operator()() const { return v; } };
D makeA() { return D{Shared{1}}; }
bool inlineA(const D& d) { return d.uses_inline(); }
