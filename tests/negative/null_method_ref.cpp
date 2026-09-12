// EXPECT: Method cannot be null
#include "tiny_delegate.hpp"
struct S { void m() {} };
int main() { S s; auto d = tiny::delegate_ref<void()>::bind<static_cast<void (S::*)()>(nullptr)>(s); }
