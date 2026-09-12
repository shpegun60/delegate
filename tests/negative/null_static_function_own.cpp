// EXPECT: function cannot be null
#include "tiny_delegate.hpp"
int main() { auto d = tiny::delegate<void()>::bind<static_cast<void (*)()>(nullptr)>(); }
