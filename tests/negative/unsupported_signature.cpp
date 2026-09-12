// EXPECT: qualified/noexcept wrapper signatures are not supported
#include "tiny_delegate.hpp"
int main() { tiny::delegate<void() noexcept> d; }
