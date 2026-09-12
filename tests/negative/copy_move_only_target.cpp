// EXPECT: source_cannot_construct_callable
#include "tiny_delegate.hpp"
struct F { F() = default; F(const F&) = delete; F(F&&) noexcept = default; void operator()() {} };
int main() { F f; tiny::delegate<void()> d{f}; }
