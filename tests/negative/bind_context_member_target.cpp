// EXPECT: free function pointer
#include "tiny_delegate.hpp"
struct Context { int read() { return 0; } };
int main() { Context c; auto d = tiny::delegate_ref<int()>::bind_context<&Context::read>(c); (void)d; }
