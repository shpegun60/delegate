// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
// Heap-fallback build: exactly-one-allocation accounting.
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1

#include "check.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

static int g_news = 0;
static int g_deletes = 0;
static int g_aligned_news = 0;
static int g_aligned_deletes = 0;
void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n)) return p;
    std::abort();
}
void operator delete(void* p) noexcept { if (p) { ++g_deletes; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { ++g_deletes; std::free(p); } }

void* operator new(std::size_t n, std::align_val_t alignment) {
    ++g_aligned_news;
    const auto a = static_cast<std::size_t>(alignment);
#if defined(_MSC_VER)
    void* p = _aligned_malloc(n, a);
#else
    void* p = std::aligned_alloc(a, ((n + a - 1u) / a) * a);
#endif
    if (!p) std::abort();
    return p;
}
void operator delete(void* p, std::align_val_t) noexcept {
    if (!p) return;
    ++g_aligned_deletes;
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept {
    ::operator delete(p, a);
}

#include "tiny_delegate.hpp"

struct Big {
    char blob[128];                 // does not fit into 32-byte SBO
    int tag;
    explicit Big(int t) : blob{}, tag(t) {}
    int operator()() const { return tag; }
};

int main()
{
    using D = tiny::delegate_sbo<int(), 32>;

    {
        // Small callable: still inline even in the fallback build.
        D small{[] { return 1; }};
        CHECK(small.uses_inline() && !small.uses_heap());
        CHECK(g_news == 0);

        // Big callable: exactly ONE allocation.
        D big{Big{42}};
        CHECK(big.uses_heap() && !big.uses_inline());
        CHECK(g_news == 1 && g_deletes == 0);
        CHECK(big() == 42);

        // Move of a heap delegate: pointer handoff, NO extra alloc/free.
        D big2{std::move(big)};
        CHECK(!big && big2.uses_heap());
        CHECK(g_news == 1 && g_deletes == 0);
        CHECK(big2() == 42);

        // Reassign over heap payload: old freed, new allocated.
        big2 = Big{7};
        CHECK(g_deletes == 1 && g_news == 2);
        CHECK(big2() == 7);

        // reset frees.
        big2.reset();
        CHECK(g_deletes == 2);
    }
    CHECK(g_news == g_deletes);    // perfect balance, no leaks

    // ---- The hybrid tiny::delegate has its own (parallel) heap manager;
    //      it must obey exactly the same accounting. ----
    {
        const int base_news = g_news;
        const int base_deletes = g_deletes;
        using H = tiny::delegate<int(), 32>;

        H h{Big{9}};                              // oversized -> heap
        CHECK(h.uses_heap() && h.owning() && !h.uses_inline());
        CHECK(g_news == base_news + 1);
        CHECK(h() == 9);

        // Heap payload moved INTO an engaged inline delegate: the inline
        // payload is destroyed, the pointer is handed off, nothing leaks.
        H inline_one{[] { return 5; }};
        CHECK(inline_one.uses_inline());
        inline_one = static_cast<H&&>(h);
        CHECK(!h && inline_one.uses_heap() && inline_one() == 9);
        CHECK(g_news == base_news + 1);          // no extra allocation

        // Ref mode never allocates, even in the fallback build.
        auto fn = [] { return 11; };
        H ref{tiny::borrow(fn)};
        CHECK(ref.non_owning() && !ref.uses_heap());
        CHECK(g_news == base_news + 1);

        // Reassigning heap -> inline frees the heap block.
        inline_one = [] { return 6; };
        CHECK(inline_one.uses_inline());
        CHECK(g_deletes == base_deletes + 1);
        CHECK(inline_one() == 6);
    }
    CHECK(g_news == g_deletes);

    // ---- Over-aligned oversized callable: C++17 aligned new must hold. ----
    {
        struct alignas(64) BigAligned {
            char blob[128];
            std::uintptr_t operator()() const {
                return reinterpret_cast<std::uintptr_t>(this);
            }
        };
        tiny::delegate_sbo<std::uintptr_t(), 32> d{BigAligned{}};
        CHECK(d.uses_heap());
        CHECK(g_aligned_news == 1 && g_aligned_deletes == 0);
        CHECK(d() % 64u == 0u);                  // heap block respects alignas
        tiny::delegate_sbo<std::uintptr_t(), 32> d2{static_cast<
            tiny::delegate_sbo<std::uintptr_t(), 32>&&>(d)};
        CHECK(d2() % 64u == 0u);                 // and survives a move
    }
    CHECK(g_news == g_deletes);
    CHECK(g_aligned_news == 1 && g_aligned_deletes == 1);

    // ---- A small callable whose move may throw fits the buffer but must
    //      NOT be stored inline (delegate moves are noexcept). With the
    //      fallback enabled it goes to the heap, exactly like std::function
    //      would — but visibly, via uses_heap(). ----
    {
        const int base_news = g_news;
        struct ThrowingMove {
            void* p = nullptr;
            ThrowingMove() = default;
            ThrowingMove(const ThrowingMove& o) : p(o.p) {}   // not noexcept
            int operator()() const { return 7; }
        };
        static_assert(sizeof(ThrowingMove) <= 32, "fits the buffer by size");

        tiny::delegate_sbo<int(), 32> d{ThrowingMove{}};
        CHECK(d.uses_heap() && !d.uses_inline());            // rerouted
        CHECK(g_news == base_news + 1);
        CHECK(d() == 7);

        tiny::delegate_sbo<int(), 32> d2{static_cast<
            tiny::delegate_sbo<int(), 32>&&>(d)};             // ptr handoff
        CHECK(g_news == base_news + 1);
        CHECK(d2() == 7);

        tiny::delegate<int(), 32> h{ThrowingMove{}};          // hybrid too
        CHECK(h.uses_heap() && h() == 7);
        CHECK(g_news == base_news + 2);
    }
    CHECK(g_news == g_deletes);

    std::printf("heap: news=%d deletes=%d\n", g_news, g_deletes);
    std::puts("PARANOID DELEGATE SUITE (heap-fallback build): ALL OK");
    return 0;
}
