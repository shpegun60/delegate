// EXPECT: Method cannot be null
#include "tiny_delegate.hpp"
struct S { void m() {} }; S s;
int main() { auto d = tiny::delegate<void()>::bind<static_cast<void (S::*)()>(nullptr), s>(); }
