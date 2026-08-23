// tiny_delegate — compact C++17/C++20 embedded callback library
// https://github.com/shpegun60/delegate
//
// Authors: shpegun60 + Claude (Anthropic)
// SPDX-License-Identifier: MIT
// Adversarial audit suite for tiny_delegate.hpp (no heap fallback build).
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

// ---- Global heap tracker: the whole file must allocate NOTHING. ----
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

// ---- Instrumented callable: counts every special member. ----
struct Probe {
    static int ctors, copies, moves, dtors, calls;
    int payload = 7;
    Probe() { ++ctors; }
    Probe(const Probe& o) : payload(o.payload) { ++copies; }
    Probe(Probe&& o) noexcept : payload(o.payload) { ++moves; o.payload = -1; }
    ~Probe() { ++dtors; }
    int operator()(int x) const { ++calls; return payload + x; }
    static int alive() { return ctors + copies + moves - dtors; }
    static void resetCounters() { ctors = copies = moves = dtors = calls = 0; }
};
int Probe::ctors, Probe::copies, Probe::moves, Probe::dtors, Probe::calls;

static void lifetimeAndMoveSemantics()
{
    Probe::resetCounters();
    {
        tiny::delegate_sbo<int(int)> d{Probe{}};       // temp: 1 ctor, 1 move
        assert(d && d.uses_inline() && !d.uses_heap());
        assert(d(3) == 10);

        // Move: exactly one Obj move, source must become empty.
        tiny::delegate_sbo<int(int)> d2{std::move(d)};
        assert(!d && d2);
        assert(d2(0) == 7);

        // Move-assign over an engaged delegate must destroy the old payload.
        tiny::delegate_sbo<int(int)> d3{Probe{}};
        const int dtorsBefore = Probe::dtors;
        d3 = std::move(d2);
        assert(Probe::dtors > dtorsBefore);             // old payload destroyed
        assert(!d2 && d3 && d3(1) == 8);

        // Self-move-assign must be a no-op, not a destroy.
        auto* self = &d3;
        d3 = std::move(*self);
        assert(d3 && d3(2) == 9);

        // Reassigning a fresh callable destroys the previous one.
        const int dtorsBefore2 = Probe::dtors;
        d3 = Probe{};
        assert(Probe::dtors > dtorsBefore2);
        assert(d3(0) == 7);

        // reset() destroys and empties; double reset is safe.
        d3.reset();
        assert(!d3);
        d3.reset();
    }
    assert(Probe::alive() == 0);                        // no leaks, no double-dtor
    assert(Probe::copies == 0);                         // move-only path: 0 copies
}

static void hybridModesAndIntrospection()
{
    Probe::resetCounters();
    {
        // Owning mode.
        tiny::delegate<int(int)> own{Probe{}};
        assert(own.owning() && !own.non_owning() && own.uses_inline());

        // Ref mode via borrow: must NOT construct/destroy anything.
        Probe local;
        const int ctorsBefore = Probe::ctors + Probe::copies + Probe::moves;
        tiny::delegate<int(int)> ref{tiny::borrow(local)};
        assert(Probe::ctors + Probe::copies + Probe::moves == ctorsBefore);
        assert(ref.non_owning() && !ref.owning() && !ref.uses_inline());
        local.payload = 100;
        assert(ref(1) == 101);                          // sees live object

        // Moving a ref-mode delegate keeps pointing at the same object.
        tiny::delegate<int(int)> ref2{std::move(ref)};
        assert(!ref && ref2.non_owning());
        local.payload = 200;
        assert(ref2(0) == 200);

        // bind<&T::method>: const and non-const.
        struct Svc {
            int base = 40;
            int get(int x) { return base + x; }
            int cget(int x) const { return base * x; }
        } svc;
        auto m = tiny::delegate<int(int)>::bind<&Svc::get>(svc);
        assert(m(2) == 42 && m.non_owning());
        const Svc& csvc = svc;
        auto mc = tiny::delegate<int(int)>::bind<&Svc::cget>(csvc);
        assert(mc(2) == 80);

        // Free-function bind with signature deduction.
        auto md = tiny::bind<&Svc::get>(svc);
        assert(md(2) == 42);
    }
    assert(Probe::alive() == 0);
}

static void delegateRefContract()
{
    // Function pointer path.
    tiny::delegate_ref<int(int)> r{+[](int x) { return x * 2; }};
    assert(r && r(21) == 42);

    // Borrow path; delegate_ref is copyable (non-owning view).
    int hits = 0;
    auto counter = [&hits](int x) { ++hits; return x; };
    r = tiny::borrow(counter);
    auto rCopy = r;
    assert(r(1) == 1 && rCopy(2) == 2 && hits == 2);

    // Reset and bool.
    r.reset();
    assert(!r && rCopy);

    // Rebinding a copy must not affect the original.
    rCopy = +[](int) { return -1; };
    assert(rCopy(0) == -1);
}

static void storedFnPointerSurvivesMove()
{
    // Regression probe: after assigning a raw function pointer, the manager
    // move path swaps the invoker; the delegate must still call correctly.
    tiny::delegate_sbo<int(int)> d{+[](int x) { return x + 1; }};
    assert(d(1) == 2);
    tiny::delegate_sbo<int(int)> d2{std::move(d)};
    assert(!d && d2 && d2(41) == 42);                   // moved fn ptr works
    tiny::delegate_sbo<int(int)> d3;
    d3 = std::move(d2);
    assert(d3(0) == 1);
}

struct alignas(16) Aligned16 {
    static std::uintptr_t last_this;
    char pad[16] = {};
    void operator()() { last_this = reinterpret_cast<std::uintptr_t>(this); }
};
std::uintptr_t Aligned16::last_this = 0;

static void alignmentInsideSbo()
{
    tiny::delegate<void(), 32, 16> d{Aligned16{}};
    d();
    assert(Aligned16::last_this % 16u == 0u);           // SBO respects alignas
    tiny::delegate<void(), 32, 16> d2{std::move(d)};
    d2();
    assert(Aligned16::last_this % 16u == 0u);           // and after a move
}

struct MoveOnlyCallable {
    int value;
    explicit MoveOnlyCallable(int v) : value(v) {}
    MoveOnlyCallable(const MoveOnlyCallable&) = delete;
    MoveOnlyCallable& operator=(const MoveOnlyCallable&) = delete;
    MoveOnlyCallable(MoveOnlyCallable&&) = default;
    int operator()() const { return value; }
};

static void moveOnlyCallableSupport()
{
    tiny::delegate_sbo<int()> d{MoveOnlyCallable{5}};
    assert(d() == 5);
    tiny::delegate_sbo<int()> d2{std::move(d)};
    assert(d2() == 5);
}

static void constDelegateMutableLambda()
{
    int n = 0;
    auto bump = [&n]() mutable { return ++n; };
    const tiny::delegate<int()> d{std::move(bump)};
    assert(d() == 1 && d() == 2);                       // const call, live state
}

static void nullptrClearsDelegate()
{
    tiny::delegate<void()> d{+[] {}};
    assert(d);
    d = nullptr;                                        // explicit clear path
    assert(!d);
    // (Null *function pointer* assignment now traps under the default
    // fail-closed policy; that path is tested in the lenient-assert build.)
}

static void safeCallHelpers()
{
    // --- call_if: порожній стан легальний, без trap ---
    tiny::delegate_ref<int(int)> empty_ref;
    assert(!empty_ref.call_if(1).has_value());          // nullopt, не trap

    auto doubler = [](int x) { return x * 2; };
    tiny::delegate_ref<int(int)> ref{tiny::borrow(doubler)};
    auto got = ref.call_if(21);
    assert(got.has_value() && *got == 42);

    // void-сигнатура: bool "викликано чи ні"
    int hits = 0;
    auto bump = [&hits] { ++hits; };
    tiny::delegate_ref<void()> vref;
    assert(vref.call_if() == false && hits == 0);
    vref = tiny::borrow(bump);
    assert(vref.call_if() == true && hits == 1);

    // --- call_or: fallback з тими самими аргументами ---
    tiny::delegate_ref<int(int)> maybe;
    assert(maybe.call_or([](int x) { return -x; }, 5) == -5);  // порожній -> fallback
    maybe = tiny::borrow(doubler);
    assert(maybe.call_or([](int x) { return -x; }, 5) == 10);  // зайнятий -> основний

    // --- ті самі контракти на владіючих типах ---
    tiny::delegate_sbo<int(int)> sbo_empty;
    assert(!sbo_empty.call_if(1).has_value());
    assert(sbo_empty.call_or([](int x) { return x + 100; }, 1) == 101);
    tiny::delegate_sbo<int(int)> sbo{[](int x) { return x + 1; }};
    assert(*sbo.call_if(41) == 42);
    assert(sbo.call_or([](int) { return -1; }, 41) == 42);

    tiny::delegate<void(int)> hyb_empty;
    int sink = 0;
    assert(hyb_empty.call_if(7) == false);
    hyb_empty.call_or([&sink](int x) { sink = x; }, 7);        // void fallback
    assert(sink == 7);
    tiny::delegate<void(int)> hyb{[&sink](int x) { sink = x * 10; }};
    assert(hyb.call_if(5) == true && sink == 50);
}

// ---- Compile-time binding: functions and instances baked into the type. ----
static int freeDouble(int x) { return x * 2; }
static int freeNegate(int x) { return -x; }

struct StaticSvc {
    int base = 300;
    int shift(int x) const { return base + x; }
};
static StaticSvc g_svc;                            // static storage + linkage

// THE flash-table use case: a constexpr array of fully-bound delegates.
static constexpr tiny::delegate_ref<int(int)> k_rom_table[] = {
    tiny::delegate_ref<int(int)>::bind<&freeDouble>(),
    tiny::delegate_ref<int(int)>::bind<&freeNegate>(),
    tiny::delegate_ref<int(int)>::bind<&StaticSvc::shift, g_svc>(),
};

static void compileTimeBinding()
{
    // ROM table entries are callable and correct.
    assert(k_rom_table[0](21) == 42);
    assert(k_rom_table[1](5) == -5);
    assert(k_rom_table[2](7) == 307);

    // Compile-time instance binding sees the LIVE object, not a copy.
    g_svc.base = 1000;
    assert(k_rom_table[2](7) == 1007);
    g_svc.base = 300;

    // The hybrid delegate offers the same spellings (non-owning mode).
    auto hf = tiny::delegate<int(int)>::bind<&freeDouble>();
    assert(hf(4) == 8 && hf.non_owning());
    auto hm = tiny::delegate<int(int)>::bind<&StaticSvc::shift, g_svc>();
    assert(hm(1) == 301 && hm.non_owning());

    // And they interoperate with the safe-call helpers.
    assert(*hf.call_if(10) == 20);
}

int main()
{
    compileTimeBinding();
    safeCallHelpers();
    lifetimeAndMoveSemantics();
    hybridModesAndIntrospection();
    delegateRefContract();
    storedFnPointerSurvivesMove();
    alignmentInsideSbo();
    moveOnlyCallableSupport();
    constDelegateMutableLambda();
    nullptrClearsDelegate();

    // Hardening contracts: moves are now unconditionally noexcept.
    static_assert(std::is_nothrow_move_constructible_v<tiny::delegate<int(int)>>);
    static_assert(std::is_nothrow_move_assignable_v<tiny::delegate_sbo<int(int)>>);
    static_assert(!std::is_constructible_v<tiny::delegate<void()>, int>);

    std::printf("heap: news=%d deletes=%d\n", g_news, g_deletes);
    assert(g_news == 0 && g_deletes == 0);              // ZERO heap, as promised
    std::puts("PARANOID DELEGATE SUITE (no-heap build): ALL OK");
    return 0;
}
