// EXPECT: signature mismatch
#include "tiny_delegate.hpp"
struct Context {};
int wrong(Context&, const char*) { return 0; }
int main() { Context c; auto d = tiny::delegate_ref<int(int)>::bind_context<&wrong>(c); (void)d; }
