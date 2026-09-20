// EXPECT: signature mismatch
#include "tiny_delegate.hpp"
struct Context { int value; };
int value(Context& context) { return context.value; }
int main() { Context c{}; auto d = tiny::delegate_ref<const int&()>::bind_context<&value>(c); (void)d; }
