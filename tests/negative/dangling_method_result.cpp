// EXPECT: signature mismatch
#include "tiny_delegate.hpp"
struct S { int m() { return 1; } };
int main() { S s; auto d = tiny::delegate<const int&()>::bind<&S::m>(s); }
