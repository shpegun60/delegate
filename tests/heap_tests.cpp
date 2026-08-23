// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
// Heap-fallback build: exactly-one-allocation accounting.
#ifdef NDEBUG
#undef NDEBUG
#endif
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

static int g_news = 0;
static int g_deletes = 0;
void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n)) return p;
    std::abort();
}
void operator delete(void* p) noexcept { if (p) { ++g_deletes; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { ++g_deletes; std::free(p); } }

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
        assert(small.uses_inline() && !small.uses_heap());
        assert(g_news == 0);

        // Big callable: exactly ONE allocation.
        D big{Big{42}};
        assert(big.uses_heap() && !big.uses_inline());
        assert(g_news == 1 && g_deletes == 0);
        assert(big() == 42);

        // Move of a heap delegate: pointer handoff, NO extra alloc/free.
        D big2{std::move(big)};
        assert(!big && big2.uses_heap());
        assert(g_news == 1 && g_deletes == 0);
        assert(big2() == 42);

        // Reassign over heap payload: old freed, new allocated.
        big2 = Big{7};
        assert(g_deletes == 1 && g_news == 2);
        assert(big2() == 7);

        // reset frees.
        big2.reset();
        assert(g_deletes == 2);
    }
    assert(g_news == g_deletes);    // perfect balance, no leaks
    std::printf("heap: news=%d deletes=%d\n", g_news, g_deletes);
    std::puts("PARANOID DELEGATE SUITE (heap-fallback build): ALL OK");
    return 0;
}
