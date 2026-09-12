// EXPECT: signature_mismatch
#include "tiny_delegate.hpp"
int main() { tiny::delegate<void(int)> d; d = [](const char*) {}; }
