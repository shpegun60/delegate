// EXPECT: deleted
#include "tiny_delegate.hpp"
struct Context { int value; };
int context_read(Context& value) { return value.value; }
int main() { auto d = tiny::delegate_ref<int()>::bind_context<&context_read>(Context{}); (void)d; }
