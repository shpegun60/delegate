// EXPECT: borrow_target_must_match_signature_and_return_compatible_reference
#include "tiny_delegate.hpp"
struct F { int v{}; int& operator()() { return v; } };
int main() { F f; tiny::delegate<const double&()> d{tiny::borrow(f)}; }
