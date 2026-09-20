// EXPECT: function cannot be null
#include "tiny_delegate.hpp"
struct Context {};
using Adapter = int (*)(Context&);
int main() { Context c; auto d = tiny::delegate_ref<int()>::bind_context<static_cast<Adapter>(nullptr)>(c); (void)d; }
