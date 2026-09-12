// EXPECT: reference_result_requires_compatible_reference
#include "tiny_delegate.hpp"
struct F { int operator()() { return 1; } };
int main() { tiny::delegate<const int&()> d{F{}}; }
