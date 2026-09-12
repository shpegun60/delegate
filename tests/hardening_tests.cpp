#include "tiny_delegate.hpp"
#include "check.hpp"
#include <memory>
#include <type_traits>
#include <utility>

struct ValueResult { int operator()() const { return 42; } };
struct RefResult { int value = 42; int& operator()() { return value; } };
struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    int operator()() const { return 7; }
};
struct TooLarge { char bytes[128]{}; int operator()() { return 1; } };
struct ThrowingMove {
    ThrowingMove() = default;
    ThrowingMove(ThrowingMove&&) noexcept(false) {}
    int operator()() { return 1; }
};
struct ThrowingDestructor {
    ~ThrowingDestructor() noexcept(false) {}
    int operator()() { return 1; }
};
struct ImmovableValue {
    int value = 4;
    ImmovableValue() = default;
    ImmovableValue(const ImmovableValue&) = delete;
    ImmovableValue(ImmovableValue&&) = delete;
};
static ImmovableValue makeImmovable() { return {}; }

using FP = int (*)();
static int seven() { return 7; }
static short adaptedFunction(int value) { return static_cast<short>(value); }

// Conversion is sufficient: this object is large and cannot be copied/moved.
struct PointerSource {
    char bytes[256]{};
    PointerSource() = default;
    PointerSource(const PointerSource&) = delete;
    PointerSource(PointerSource&&) = delete;
    operator FP() & noexcept { return &seven; }
};

struct RvaluePointerSource {
    RvaluePointerSource() = default;
    RvaluePointerSource(const RvaluePointerSource&) = delete;
    RvaluePointerSource(RvaluePointerSource&&) = delete;
    operator FP() && noexcept { return &seven; }
};

template <template <class, std::size_t, std::size_t> class Wrapper>
static void owningContracts() {
    using D = Wrapper<int(), 32, alignof(std::max_align_t)>;
    using RefD = Wrapper<const int&(), 32, alignof(std::max_align_t)>;
    static_assert(!std::is_constructible_v<RefD, ValueResult>);
    static_assert(!std::is_assignable_v<RefD&, ValueResult>);
    static_assert(!std::is_constructible_v<D, MoveOnly&>);
    static_assert(!std::is_assignable_v<D&, const MoveOnly&>);
    static_assert(std::is_constructible_v<D, MoveOnly>);
    static_assert(!std::is_constructible_v<D, ThrowingDestructor>);
    static_assert(!std::is_assignable_v<D&, ThrowingDestructor>);
    static_assert(std::is_constructible_v<D, TooLarge> == bool(TINY_DELEGATE_ENABLE_HEAP_FALLBACK));
    static_assert(std::is_constructible_v<D, ThrowingMove> == bool(TINY_DELEGATE_ENABLE_HEAP_FALLBACK));
    static_assert(std::is_assignable_v<D&, TooLarge> == bool(TINY_DELEGATE_ENABLE_HEAP_FALLBACK));

    PointerSource source;
    D converted{source};
    CHECK(converted() == 7 && converted.uses_inline());
    converted = source;
    CHECK(converted() == 7);
    D rvalue{RvaluePointerSource{}};
    CHECK(rvalue() == 7);
    Wrapper<int(int), 32, alignof(std::max_align_t)> adapted{adaptedFunction};
    adapted = adaptedFunction;
    CHECK(adapted(7) == 7);

    RefResult result;
    RefD reference{result};
    CHECK(reference() == 42);

    // Callable returns itself; rebinding from that reference must copy before
    // destroying the current target, even if the target lives on the heap.
    struct Self {
        std::unique_ptr<int> value{new int(42)};
        Self() = default;
        Self(const Self& rhs) : value(new int(*rhs.value)) {}
        Self(Self&&) noexcept = default;
        Self& operator()() { return *this; }
    };
    using S = Wrapper<Self&(), 32, alignof(std::max_align_t)>;
    S self{Self{}};
    self = self();
    CHECK(*self().value == 42);
}

template <class D>
static void fallbackContracts() {
    D d;
    int calls = 0;
    d.call_or([&calls] { return ++calls; }); // void discards non-void result
    CHECK(calls == 1);
    auto main = [&calls] { calls += 10; };
    if constexpr (std::is_same_v<D, tiny::delegate_ref<void()>>) {
        d = tiny::borrow(main);
    } else {
        d = main;
    }
    d.call_or(static_cast<void (*)()>(nullptr)); // unused fallback is irrelevant
    CHECK(calls == 11);
    CHECK(d.call_if());
    CHECK(calls == 21);
}

static void borrowContracts() {
    using D = tiny::delegate_ref<const int&()>;
    static_assert(!std::is_constructible_v<D, tiny::borrow_t<ValueResult>>);
    static_assert(!std::is_constructible_v<tiny::delegate<const int&()>,
                                           tiny::borrow_t<ValueResult>>);
    static_assert(!std::is_constructible_v<tiny::delegate_ref<const double&()>,
                                           tiny::borrow_t<RefResult>>);
    RefResult target;
    D d{tiny::borrow(target)};
    CHECK(&d() == &target.value);
    target.value = 81;
    CHECK(d() == 81);

    struct Base { int v = 5; };
    struct Derived : Base {} object;
    auto get = [&]() -> Derived& { return object; };
    tiny::delegate_ref<const Base&()> base{tiny::borrow(get)};
    CHECK(&base() == static_cast<Base*>(&object));
}

// A callable destructor may unsubscribe by resetting its owner. The owner
// must already be empty, otherwise this destroys the target recursively.
template <template <class, std::size_t, std::size_t> class Wrapper, std::size_t Padding = 1>
static void destructorReset() {
    using D = Wrapper<void(), 32, alignof(std::max_align_t)>;
    struct Target {
        D* owner;
        int* destroyed;
        char padding[Padding]{};
        Target(D* d, int* n) : owner(d), destroyed(n) {}
        Target(Target&& rhs) noexcept : owner(rhs.owner), destroyed(rhs.destroyed) {
            rhs.owner = nullptr;
        }
        ~Target() {
            if (owner) {
                ++*destroyed;
                CHECK(!*owner);
                owner->reset();
            }
        }
        void operator()() const {}
    };
    D d;
    int destroyed = 0;
    d = Target{&d, &destroyed};
    d.reset();
    CHECK(!d && destroyed == 1);
}

static void invocationContracts() {
    struct Left { int padding = 1; };
    struct Base { int value = 3; virtual int method(int x) { return value + x; } };
    struct Derived : Left, Base { int method(int x) override { return value * x; } } object;
    auto ref = tiny::delegate_ref<int(int)>::bind<&Base::method>(object);
    auto own = tiny::delegate<int(int)>::bind<&Base::method>(object);
    CHECK(ref(4) == 12 && own(5) == 15);
    Base* instance = &object;
    auto pointer_ref = tiny::delegate_ref<int(int)>::bind<&Base::method>(instance);
    CHECK(pointer_ref(6) == 18);
    Base other;
    instance = &other;
    CHECK(pointer_ref(6) == 9);

    struct VolatileTarget {
        int value = 8;
        int get() const volatile & noexcept { return value; }
        int operator()() const volatile { return value; }
        VolatileTarget* operator&() { return nullptr; }
    };
    const volatile VolatileTarget vol{};
    auto method = tiny::delegate_ref<int()>::bind<&VolatileTarget::get>(vol);
    tiny::delegate<int()> callable{tiny::borrow(vol)};
    CHECK(method() == 8 && callable() == 8);
    VolatileTarget overloaded_address;
    tiny::delegate_ref<int()> address{tiny::borrow(overloaded_address)};
    CHECK(address() == 8);

    const tiny::delegate<int()> state{[n = 0]() mutable { return ++n; }};
    CHECK(state() == 1 && state() == 2);
    tiny::delegate_ref<int()> function_borrow{tiny::borrow(seven)};
    CHECK(function_borrow() == 7);

    auto result = [] { return std::make_unique<int>(13); };
    tiny::delegate_sbo<std::unique_ptr<int>()> producer{result};
    auto optional = producer.call_if();
    CHECK(optional && **optional == 13);
    tiny::delegate<bool()> false_result{[] { return false; }};
    CHECK(false_result.call_if().has_value() && !*false_result.call_if());
    tiny::delegate<ImmovableValue()> immovable{&makeImmovable};
    const auto direct = immovable();
    immovable.reset();
    const auto fallback = immovable.call_or(&makeImmovable);
    CHECK(direct.value == 4 && fallback.value == 4);
}

int main() {
    owningContracts<tiny::delegate>();
    owningContracts<tiny::delegate_sbo>();
    fallbackContracts<tiny::delegate_ref<void()>>();
    fallbackContracts<tiny::delegate<void()>>();
    fallbackContracts<tiny::delegate_sbo<void()>>();
    borrowContracts();
    destructorReset<tiny::delegate>();
    destructorReset<tiny::delegate_sbo>();
#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
    destructorReset<tiny::delegate, 128>();
    destructorReset<tiny::delegate_sbo, 128>();
#endif
    invocationContracts();
}
