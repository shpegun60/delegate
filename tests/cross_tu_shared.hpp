#ifndef TINY_DELEGATE_TEST_CROSS_TU_SHARED_HPP
#define TINY_DELEGATE_TEST_CROSS_TU_SHARED_HPP
#include "tiny_delegate.hpp"

struct Shared {
    static inline int alive = 0;
    int value;
    explicit Shared(int v) : value(v) { ++alive; }
    Shared(Shared&& other) noexcept : value(other.value) { ++alive; }
    ~Shared() { --alive; }
    int operator()() const { return value; }
};
using D = tiny::delegate_sbo<int()>;
using Hybrid = tiny::delegate<int()>;
D makeA();
bool inlineA(const D& d);
Hybrid borrowA(Shared& target);
tiny::delegate_ref<int()> refA(Shared& target);
bool nonOwningA(const Hybrid& d);
#endif
