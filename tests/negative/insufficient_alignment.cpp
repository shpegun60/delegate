// EXPECT: function pointer alignment
#include "tiny_delegate.hpp"
int main() { tiny::delegate_sbo<void(), 32, 1> d; }
