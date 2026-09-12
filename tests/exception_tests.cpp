#include "tiny_delegate.hpp"
#include "check.hpp"
#include <new>
#include <utility>

struct Failure {};
template <std::size_t Bytes>
struct ThrowingCopy {
    static inline int alive = 0;
    static inline int allocations = 0;
    static inline int frees = 0;
    static inline bool fail = false;
    char data[Bytes]{};
    ThrowingCopy() { ++alive; }
    ThrowingCopy(const ThrowingCopy&) { if (fail) throw Failure{}; ++alive; }
    ThrowingCopy(ThrowingCopy&&) noexcept { ++alive; }
    ~ThrowingCopy() { --alive; }
    int operator()() const { return 23; }
    static void* operator new(std::size_t n) { ++allocations; return ::operator new(n); }
    static void operator delete(void* p) noexcept { ++frees; ::operator delete(p); }
};

template <class D, class F>
static void throwingConstruction() {
    CHECK(F::alive == 0);
    {
        F source;
        F::fail = true;
        try { D bad{source}; CHECK(false); } catch (const Failure&) {}
        CHECK(F::alive == 1 && F::allocations == F::frees);

        D d{[] { return 1; }};
        try { d = source; CHECK(false); } catch (const Failure&) {}
        CHECK(!d && !d.call_if()); // Basic exception guarantee: empty and reusable.
        CHECK(F::alive == 1 && F::allocations == F::frees);

        F::fail = false;
        d = source;
        CHECK(d() == 23 && F::alive == 2);
        auto moved = std::move(d);
        CHECK(!d && moved() == 23 && F::alive == 2);
    }
    CHECK(F::alive == 0 && F::allocations == F::frees);
}

template <class D>
static void throwingInvocation() {
    auto f = []() -> int { throw Failure{}; };
    D d{f};
    try { (void)d(); CHECK(false); } catch (const Failure&) {}
    try { (void)d.call_if(); CHECK(false); } catch (const Failure&) {}
    CHECK(d);
    d.reset();
    try { (void)d.call_or(f); CHECK(false); } catch (const Failure&) {}
    CHECK(!d);
}

#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
struct AllocationFailure {
    static inline volatile bool fail = true;
    char data[128]{};
    int operator()() const { return 0; }
    static void* operator new(std::size_t n) {
        if (fail) throw std::bad_alloc{};
        return ::operator new(n);
    }
    static void operator delete(void* p) noexcept { ::operator delete(p); }
};
struct CopyOnly {
    static inline int copies = 0;
    CopyOnly() = default;
    CopyOnly(const CopyOnly&) { ++copies; }
    CopyOnly(CopyOnly&&) = delete;
    int operator()() const { return 42; }
};
template <class D>
static void heapContracts() {
    D d{[] { return 1; }};
    try { d = AllocationFailure{}; CHECK(false); } catch (const std::bad_alloc&) {}
    CHECK(!d);
    CopyOnly source;
    const int before = CopyOnly::copies;
    d = source;
    CHECK(d.uses_heap() && d() == 42 && CopyOnly::copies == before + 1);
    D moved = std::move(d);
    CHECK(!d && moved() == 42 && CopyOnly::copies == before + 1);
}
#endif

int main() {
    throwingConstruction<tiny::delegate<int()>, ThrowingCopy<8>>();
    throwingConstruction<tiny::delegate_sbo<int()>, ThrowingCopy<8>>();
    throwingInvocation<tiny::delegate<int()>>();
    throwingInvocation<tiny::delegate_sbo<int()>>();
    throwingInvocation<tiny::delegate_ref<int()>>();
#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
    throwingConstruction<tiny::delegate<int()>, ThrowingCopy<128>>();
    throwingConstruction<tiny::delegate_sbo<int()>, ThrowingCopy<128>>();
    heapContracts<tiny::delegate<int()>>();
    heapContracts<tiny::delegate_sbo<int()>>();
#endif
}
