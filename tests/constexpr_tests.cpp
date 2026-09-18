// SPDX-License-Identifier: MIT
// Constant initialization stores a callable; invocation still observes live state.

#include "check.hpp"
#include "tiny_delegate.hpp"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace {

int live_value = 7;

int read_value() { return live_value; }
int read_nothrow() noexcept { return live_value; }
int read_negative() { return -live_value; }
void write_value(int value) noexcept { live_value = value; }
int& value_reference() noexcept { return live_value; }

template <int Offset>
int read_offset() noexcept { return live_value + Offset; }

struct Sensor {
    int value = 11;
    int read() const noexcept { return value; }
    int read_mutable() noexcept { return value; }
};

Sensor sensor;
const Sensor constant_sensor{19};

using Reader = tiny::delegate_ref<int()>;
constexpr auto named_lambda = []() noexcept { return live_value; };

struct Field {
    const char* name;
    Reader get;
};

// A runtime function-pointer constructor here would make the entire test fail
// to compile. The callbacks themselves deliberately are not constexpr functions.
constexpr Field fields[] = {
    {"function", read_value},
    {"address", &read_value},
    {"nothrow", read_nothrow},
    {"lambda", +[]() noexcept { return live_value; }},
    {"bare_lambda", []() noexcept { return live_value; }},
    {"bare_throwing_lambda", [] { return live_value; }},
    {"named_lambda", named_lambda},
#if defined(TINY_DELEGATE_TEST_TEMPLATE_POINTER_COMPARE_LIMITED)
    // Some GCC/UBSan combinations reject even a plain-language constant null
    // comparison of a function-template address. CMake probes that operation;
    // retain the portable NTTP spelling on those affected configurations.
    {"template", Reader::bind<&read_offset<3>>()},
#else
    {"template", &read_offset<3>},
#endif
    {"function_bind", Reader::bind<&read_value>()},
    {"method", Reader::bind<&Sensor::read>(sensor)},
    {"mutable_method", Reader::bind<&Sensor::read_mutable>(sensor)},
    {"const_object", Reader::bind<&Sensor::read>(constant_sensor)},
    {"instance_bind", Reader::bind<&Sensor::read, sensor>()},
};

constexpr Reader empty;
constexpr Reader null_reader{nullptr};
constexpr Reader copied_reader = fields[0].get;
constexpr Reader bare_copy_initialized = [] { return live_value; };
constexpr tiny::delegate_ref<void(int)> writer = write_value;
constexpr tiny::delegate_ref<int&()> reference_reader = value_reference;

static_assert(!empty && !null_reader, "constant empty state must remain supported");
static_assert(std::is_trivially_copyable_v<Reader>, "ref must remain a trivial view");

constexpr Reader copy_and_reset()
{
    Reader copy = fields[0].get;
    copy.reset();
    return copy;
}

static_assert(!copy_and_reset(), "reset must work on a constant function-pointer binding");

void constant_tables_read_live_values()
{
    for (const int value : {7, 42, -5}) {
        live_value = value;
        sensor.value = value + 100;
        const int expected[] = {
            value, value, value, value, value, value, value, value + 3, value,
            value + 100, value + 100, 19, value + 100,
        };
        static_assert(sizeof(fields) / sizeof(fields[0]) == sizeof(expected) / sizeof(expected[0]),
                      "every field needs an independent expected value");
        for (std::size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
            CHECK(fields[i].get);
            CHECK(fields[i].get() == expected[i]);
        }
        CHECK(copied_reader() == value);
        CHECK(bare_copy_initialized() == value);
    }

    writer(31);
    CHECK(live_value == 31);
    CHECK(&reference_reader() == &live_value);
    reference_reader() = 47;
    CHECK(copied_reader() == 47);

    CHECK(!empty.call_if());
    CHECK(null_reader.call_or([] { return -999; }) == -999);
}

struct LvaluePointer {
    constexpr operator Reader::fnptr_t() & noexcept { return &read_value; }
};

struct RvaluePointer {
    constexpr operator Reader::fnptr_t() && noexcept { return &read_negative; }
};

struct ExplicitPointer {
    explicit operator Reader::fnptr_t() const noexcept { return &read_value; }
};

struct MixedPointer {
    constexpr operator Reader::fnptr_t() const & noexcept { return &read_value; }
    explicit constexpr operator Reader::fnptr_t() && noexcept { return &read_negative; }
};

struct CountingPointer {
    int* conversions;
    operator Reader::fnptr_t() const { ++*conversions; return &read_value; }
};

struct DerivedReader : Reader {
    using Reader::Reader;
    operator Reader::fnptr_t() const noexcept { return &read_negative; }
};

static_assert(std::is_convertible_v<LvaluePointer&, Reader>, "lvalue conversion must be forwarded");
static_assert(!std::is_constructible_v<Reader, LvaluePointer>, "rvalue cannot use lvalue-only conversion");
static_assert(!std::is_constructible_v<Reader, const LvaluePointer&>, "const source cannot use mutable conversion");
static_assert(std::is_convertible_v<RvaluePointer, Reader>, "rvalue conversion must be forwarded");
static_assert(!std::is_constructible_v<Reader, RvaluePointer&>, "lvalue cannot use rvalue-only conversion");
static_assert(!std::is_constructible_v<Reader, ExplicitPointer>, "explicit conversions are not implicit callbacks");
static_assert(std::is_assignable_v<Reader&, LvaluePointer&>, "assignment must forward lvalue conversion");
static_assert(!std::is_assignable_v<Reader&, LvaluePointer>, "assignment must reject a wrong value category");
static_assert(std::is_assignable_v<Reader&, RvaluePointer>, "assignment must forward rvalue conversion");
static_assert(!std::is_assignable_v<Reader&, RvaluePointer&>, "assignment must reject a wrong value category");

LvaluePointer lvalue_pointer;
constexpr Reader converted_lvalue = lvalue_pointer;
constexpr Reader converted_rvalue = RvaluePointer{};
constexpr Reader implicitly_converted = MixedPointer{};

template <class D>
void pointer_conversion_compatibility()
{
    live_value = 71;
    auto lambda = [] { return live_value; };
    D from_lvalue = lambda;
    D from_rvalue = std::move(lambda);
    CHECK(from_lvalue() == 71 && from_rvalue() == 71);
    from_lvalue = []() noexcept { return -live_value; };
    CHECK(from_lvalue() == -71);
    from_lvalue = lambda;
    CHECK(from_lvalue() == 71);
    from_lvalue = RvaluePointer{};
    CHECK(from_lvalue() == -71);
    from_lvalue = lvalue_pointer;
    CHECK(from_lvalue() == 71);
    from_lvalue = MixedPointer{};
    CHECK(from_lvalue() == 71); // The explicit alternative must not be selected.
    D mixed = MixedPointer{};
    CHECK(mixed() == 71);
    D lvalue_converted = lvalue_pointer;
    D rvalue_converted = RvaluePointer{};
    CHECK(lvalue_converted() == 71 && rvalue_converted() == -71);

    int conversions = 0;
    D counted = CountingPointer{&conversions};
    CHECK(conversions == 1 && counted() == 71);
    counted = CountingPointer{&conversions};
    CHECK(conversions == 2 && counted() == 71);
}

void implicit_callable_conversions()
{
    live_value = 71;
    CHECK(converted_lvalue() == 71 && converted_rvalue() == -71);
    CHECK(implicitly_converted() == 71);

    // A derived view must retain normal base copying even if it also advertises
    // a conversion to a different function pointer.
    DerivedReader derived{&read_value};
    Reader copied_base = derived;
    Reader direct_base{derived};
    CHECK(copied_base() == 71 && direct_base() == 71);
    copied_base = &read_negative;
    copied_base = derived;
    CHECK(copied_base() == 71);
    Reader moved_base = std::move(derived);
    CHECK(moved_base() == 71);
    copied_base = std::move(derived);
    CHECK(copied_base() == 71);

    int captured = 79;
    auto captureful = [&captured] { return captured; };
    using Captureful = decltype(captureful);
    static_assert(!std::is_constructible_v<Reader, Captureful>, "temporary captures must not be borrowed implicitly");
    static_assert(!std::is_constructible_v<Reader, Captureful&>, "stateful callable still needs explicit borrow");
    Reader borrowed = tiny::borrow(captureful);
    captured = 83;
    CHECK(borrowed() == 83);

    // A generic captureless lambda also converts to the requested signature;
    // runtime coverage does not depend on a compiler's constexpr address rules.
    tiny::delegate_ref<int(int)> generic = [](auto offset) { return live_value + offset; };
    CHECK(generic(9) == 80);
    auto generic_lambda = [](auto offset) { return live_value - offset; };
    generic = generic_lambda;
    CHECK(generic(9) == 62);
    generic = [](auto offset) noexcept { return live_value * offset; };
    CHECK(generic(2) == 142);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct ConversionFailure {};
volatile bool fail_pointer_conversion = true;

struct ThrowingPointer {
    int* conversions;
    operator Reader::fnptr_t() const {
        ++*conversions;
        if (fail_pointer_conversion) throw ConversionFailure{};
        return &read_negative;
    }
};

template <class D>
void throwing_pointer_conversion()
{
    fail_pointer_conversion = true;
    int conversions = 0;
    try {
        D unexpected = ThrowingPointer{&conversions};
        (void)unexpected;
        CHECK(false);
    } catch (const ConversionFailure&) {}
    CHECK(conversions == 1);

    live_value = 97;
    D preserved = &read_value;
    try {
        preserved = ThrowingPointer{&conversions};
        CHECK(false);
    } catch (const ConversionFailure&) {}
    CHECK(conversions == 2 && preserved() == 97);

    if constexpr (!std::is_same_v<D, Reader>) {
        D inline_owned = [value = 113] { return value; };
        CHECK(inline_owned.uses_inline());
        try {
            inline_owned = ThrowingPointer{&conversions};
            CHECK(false);
        } catch (const ConversionFailure&) {}
        CHECK(conversions == 3 && inline_owned.uses_inline() && inline_owned() == 113);

#if TINY_DELEGATE_ENABLE_HEAP_FALLBACK
        struct LargeCallable {
            int value = 127;
            char padding[128]{};
            int operator()() const { return value; }
        };
        D heap_owned = LargeCallable{};
        CHECK(heap_owned.uses_heap());
        try {
            heap_owned = ThrowingPointer{&conversions};
            CHECK(false);
        } catch (const ConversionFailure&) {}
        CHECK(conversions == 4 && heap_owned.uses_heap() && heap_owned() == 127);
#endif
    }

    const int failed_conversions = conversions;
    fail_pointer_conversion = false;
    D recovered = ThrowingPointer{&conversions};
    CHECK(conversions == failed_conversions + 1 && recovered() == -97);
    preserved = ThrowingPointer{&conversions};
    CHECK(conversions == failed_conversions + 2 && preserved() == -97);
}
#endif

void payload_transitions_and_copies()
{
    // Runtime construction from a template function remains unconditional,
    // including configurations with the constant-expression compiler issue.
    live_value = 5;
    Reader template_pointer{&read_offset<3>};
    CHECK(template_pointer() == 8);
    template_pointer = &read_offset<9>;
    CHECK(template_pointer() == 14);

    Reader current = fields[0].get;
    live_value = 10;
    CHECK(current() == 10);

    int borrowed_value = 23;
    auto callable = [&borrowed_value] { return borrowed_value; };
    current = tiny::borrow(callable);
    Reader borrowed_copy = current;
    borrowed_value = 29;
    CHECK(current() == 29 && borrowed_copy() == 29);

    current = &read_negative;
    CHECK(current() == -10 && borrowed_copy() == 29);
    Reader function_copy = current;
    current = Reader::bind<&Sensor::read>(sensor);
    sensor.value = 83;
    CHECK(current() == 83 && function_copy() == -10);

    current.reset();
    CHECK(!current && !current.call_if());
    current = read_nothrow;
    CHECK(current.call_if().value() == 10);
    current = nullptr;
    CHECK(!current && current.call_or([] { return 77; }) == 77);
    current = fields[0].get;
    live_value = 61;
    CHECK(current() == 61 && function_copy() == -61);
}

// Both owning variants retain their existing runtime pointer behavior. This
// shares the checks without asking an owning object to become a literal type.
template <class D>
void runtime_function_pointers()
{
    volatile bool select_positive = true;
    typename D::fnptr_t pointer = select_positive ? &read_value : &read_negative;
    D callback{pointer};
    live_value = 13;
    CHECK(callback() == 13);
    pointer = &read_negative;
    CHECK(callback() == 13); // The pointer value was stored, not borrowed.
    callback = pointer;
    CHECK(callback() == -13);

    D moved{std::move(callback)};
    CHECK(moved() == -13);
    moved = read_nothrow;
    CHECK(moved.call_if().value() == 13);
    CHECK(moved.call_or([] { return -999; }) == 13);
    moved.reset();
    CHECK(!moved && !moved.call_if());
    CHECK(moved.call_or([] { return -999; }) == -999);
    moved = pointer;
    CHECK(moved() == -13);
    moved = nullptr;
    CHECK(!moved);
}

template <class Writer, class ReferenceReader>
void void_and_reference_function_pointers()
{
    Writer callback{&write_value};
    CHECK(callback.call_if(101));
    CHECK(live_value == 101);
    callback.reset();
    CHECK(!callback.call_if(999));
    CHECK(live_value == 101);

    ReferenceReader read{&value_reference};
    int fallback = -1;
    CHECK(&read.call_or([&fallback]() -> int& { return fallback; }) == &live_value);
    read() = 107;
    CHECK(live_value == 107);
    read.reset();
    CHECK(&read.call_or([&fallback]() -> int& { return fallback; }) == &fallback);
}

} // namespace

int main()
{
    constant_tables_read_live_values();
    pointer_conversion_compatibility<tiny::delegate_ref<int()>>();
    pointer_conversion_compatibility<tiny::delegate_sbo<int()>>();
    pointer_conversion_compatibility<tiny::delegate<int()>>();
    implicit_callable_conversions();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    throwing_pointer_conversion<tiny::delegate_ref<int()>>();
    throwing_pointer_conversion<tiny::delegate_sbo<int()>>();
    throwing_pointer_conversion<tiny::delegate<int()>>();
#endif
    payload_transitions_and_copies();
    runtime_function_pointers<tiny::delegate_ref<int()>>();
    runtime_function_pointers<tiny::delegate_sbo<int()>>();
    runtime_function_pointers<tiny::delegate<int()>>();
    void_and_reference_function_pointers<tiny::delegate_ref<void(int)>, tiny::delegate_ref<int&()>>();
    void_and_reference_function_pointers<tiny::delegate_sbo<void(int)>, tiny::delegate_sbo<int&()>>();
    void_and_reference_function_pointers<tiny::delegate<void(int)>, tiny::delegate<int&()>>();
}
