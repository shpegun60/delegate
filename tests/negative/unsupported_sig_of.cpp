// EXPECT: expected a function pointer or member function pointer type
#include "tiny_delegate.hpp"
using Invalid = tiny::sig_of_t<int>;
