// EXPECT: alternative signature mismatch
#include "tiny_delegate.hpp"
int main() { tiny::delegate_sbo<const int&()> d; d.call_or([] { return 1; }); }
