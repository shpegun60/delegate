// EXPECT: callable_destructor_must_be_noexcept
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1
#include "tiny_delegate.hpp"
struct F { char data[128]; ~F() noexcept(false) {} void operator()() {} };
int main() { tiny::delegate_sbo<void()> d{F{}}; }
