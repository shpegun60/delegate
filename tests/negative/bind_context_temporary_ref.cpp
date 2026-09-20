// EXPECT: deleted
#include "tiny_delegate.hpp"
struct Context { int value; };
int read(Context& value) { return value.value; }
int main() { auto d = tiny::delegate_ref<int()>::bind_context<&read>(Context{}); (void)d; }
