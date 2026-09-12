// EXPECT: InlineAlign must be power-of-two
#include "tiny_delegate.hpp"
int main() { tiny::delegate<void(), 32, 3> d; }
