#include "tiny_delegate.hpp"
#include "check.hpp"
#include <type_traits>
#include <utility>

namespace {

struct Context {
    int value = 0;
};

Context live{40};
const Context fixed{70};

int add(Context& context, int value) noexcept
{
    return context.value + value;
}

int addConst(const Context& context, int value) noexcept
{
    return context.value + value;
}

void store(Context& context, int value) noexcept
{
    context.value = value;
}

int& select(Context& context, bool) noexcept
{
    return context.value;
}

struct Left { int padding = -1; };
struct Base { int value = 6; };
struct Derived : Left, Base {};

int throughBase(Base& context, int factor) noexcept
{
    return context.value * factor;
}

using Reader = tiny::delegate_ref<int(int)>;
constexpr Reader tableEntry = Reader::bind_context<&add>(live);
constexpr Reader constEntry = Reader::bind_context<&addConst>(fixed);
static_assert(sizeof(Reader) == sizeof(void*) * 2);
static_assert(std::is_trivially_copyable_v<Reader>);

void referenceDelegate()
{
    CHECK(tableEntry(2) == 42);
    live.value = 100;
    CHECK(tableEntry(2) == 102);

    auto copy = tableEntry;
    live.value = 9;
    CHECK(copy(3) == 12);
    CHECK(constEntry(2) == 72);

    auto writer = tiny::delegate_ref<void(int)>::bind_context<&store>(live);
    writer(81);
    CHECK(live.value == 81);

    auto reference = tiny::delegate_ref<int&(bool)>::bind_context<&select>(live);
    CHECK(&reference(true) == &live.value);
    reference(false) = 91;
    CHECK(live.value == 91);

    Derived object;
    auto adjusted = Reader::bind_context<&throughBase>(object);
    CHECK(adjusted(7) == 42);
}

void hybridDelegate()
{
    live.value = 20;
    auto callback = tiny::delegate<int(int)>::bind_context<&add>(live);
    CHECK(callback.non_owning() && !callback.owning() && !callback.uses_inline());
    CHECK(callback(22) == 42);

    auto moved = std::move(callback);
    CHECK(!callback && moved.non_owning() && moved(1) == 21);
    live.value = 41;
    CHECK(moved(1) == 42);
}

} // namespace

int main()
{
    referenceDelegate();
    hybridDelegate();
}
