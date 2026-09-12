// EXPECT: Method cannot be null
#include "tiny_delegate.hpp"
struct S { void m() const {} };
int main() { const S s{}; auto d = tiny::delegate<void()>::bind<static_cast<void (S::*)() const>(nullptr)>(s); }
