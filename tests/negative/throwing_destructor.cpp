// EXPECT: callable_destructor_must_be_noexcept
#include "tiny_delegate.hpp"
struct F { F()=default; F(F&&) noexcept {} ~F() noexcept(false) {} void operator()() {} };
int main() { tiny::delegate<void()> d{F{}}; }
