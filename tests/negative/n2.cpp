// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
#include "tiny_delegate.hpp"
struct Big { char b[128]; void operator()() const {} };
int main() { tiny::delegate_sbo<void(), 32> d{Big{}}; } // не влазить, fallback вимкнено
