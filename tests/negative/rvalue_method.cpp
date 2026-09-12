// EXPECT: signature mismatch
#include "tiny_delegate.hpp"
struct S { void m() && {} };
int main() { S s; auto d = tiny::bind<&S::m>(s); }
