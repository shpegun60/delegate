// EXPECT: signature mismatch
#include "tiny_delegate.hpp"
int f() { return 1; }
int main() { auto d = tiny::delegate_ref<const int&()>::bind<&f>(); }
