// EXPECT: object template argument cannot be a reference
#include "tiny_delegate.hpp"
struct Object { int get() const { return 1; } };
int main() { auto d = tiny::bind<&Object::get, const Object&>(Object{}); }
