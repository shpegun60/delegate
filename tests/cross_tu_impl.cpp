// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "cross_tu_shared.hpp"
D makeA() { return D{Shared{1}}; }
bool inlineA(const D& d) { return d.uses_inline(); }
Hybrid borrowA(Shared& target) { return Hybrid{tiny::borrow(target)}; }
tiny::delegate_ref<int()> refA(Shared& target) { return tiny::borrow(target); }
bool nonOwningA(const Hybrid& d) { return d.non_owning(); }
