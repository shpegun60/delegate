// EXPECT: result must be movable or copyable
#include "tiny_delegate.hpp"
struct Value { Value() = default; Value(const Value&) = delete; Value(Value&&) = delete; };
Value make() { return {}; }
int main() { tiny::delegate<Value()> d{&make}; (void)d.call_if(); }
