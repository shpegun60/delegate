#ifndef DELEGATE_DEMON_SUITE_COMMON_H
#define DELEGATE_DEMON_SUITE_COMMON_H

#include "delegate_demon_tests.h"

#include <QElapsedTimer>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace delegate_demon {

struct SuiteStats {
    QString name;
    int passed = 0;
    int failed = 0;
    qint64 elapsedMs = 0;

    bool ok() const noexcept { return failed == 0; }
};

class SuiteContext {
public:
    SuiteContext(QString suiteName, const DelegateLogSink& sink)
        : suiteName_(std::move(suiteName))
        , sink_(sink) {}

    void section(const QString& title) {
        push(QStringLiteral("\n=== [%1] %2 ===").arg(suiteName_, title));
    }

    void info(const QString& line) {
        push(QStringLiteral("[INFO] %1").arg(line));
    }

    bool check(bool condition, const QString& label) {
        if (condition) {
            ++passed_;
            push(QStringLiteral("[PASS] %1").arg(label));
            return true;
        }

        ++failed_;
        push(QStringLiteral("[FAIL] %1").arg(label));
        return false;
    }

    template <class A, class B>
    bool checkEq(const A& actual, const B& expected, const QString& label) {
        return check(actual == expected,
                     QStringLiteral("%1 | actual=%2 expected=%3")
                         .arg(label, numberString(actual), numberString(expected)));
    }

    SuiteStats finish(qint64 elapsedMs) {
        push(QStringLiteral("[DONE] %1 | passed=%2 failed=%3 elapsed=%4 ms")
                 .arg(suiteName_)
                 .arg(passed_)
                 .arg(failed_)
                 .arg(elapsedMs));
        return SuiteStats{suiteName_, passed_, failed_, elapsedMs};
    }

private:
    template <class T>
    static QString numberString(const T& value) {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            return value ? QStringLiteral("true") : QStringLiteral("false");
        } else if constexpr (std::is_signed_v<std::decay_t<T>>) {
            return QString::number(static_cast<qlonglong>(value));
        } else {
            return QString::number(static_cast<qulonglong>(value));
        }
    }

    void push(const QString& line) const {
        if (sink_) sink_(line);
    }

    QString suiteName_;
    DelegateLogSink sink_;
    int passed_ = 0;
    int failed_ = 0;
};

struct MethodTarget {
    int base = 0;

    int add(int x) { return base + x; }
    int addConst(int x) const { return base + x + 1; }
};

struct TinyFunctor {
    int bias = 0;

    int operator()(int x) const { return bias + x; }
};

template <std::size_t Align>
struct alignas(Align) AlignedFunctor {
    int bias = 0;
    int marker = 3;

    explicit AlignedFunctor(int value = 0) : bias(value) {}

    int operator()(int x) const { return bias + x + marker; }
};

template <std::size_t Bytes>
struct LargeFunctor {
    char payload[Bytes]{};
    int bias = 0;
    int salt = 5;

    explicit LargeFunctor(int value = 0) : bias(value) {
        payload[0] = static_cast<char>(salt);
    }

    int operator()(int x) const { return bias + x + static_cast<int>(payload[0]); }
};

template <std::size_t Bytes, std::size_t Align>
struct alignas(Align) LargeAlignedFunctor {
    char payload[Bytes]{};
    int bias = 0;
    int salt = 7;

    explicit LargeAlignedFunctor(int value = 0) : bias(value) {
        payload[0] = static_cast<char>(salt);
    }

    int operator()(int x) const { return bias + x + static_cast<int>(payload[0]); }
};

struct LifetimeCounters {
    int alive = 0;
    int copies = 0;
    int moves = 0;
    int destructions = 0;
};

template <std::size_t Bytes, std::size_t Align>
struct alignas(Align) TrackedFunctor {
    static constexpr std::size_t payload_size = Bytes > 0 ? Bytes : 1;

    LifetimeCounters* counters = nullptr;
    bool counts_alive = false;
    int bias = 0;
    char payload[payload_size]{};

    explicit TrackedFunctor(LifetimeCounters& stats, int value = 0)
        : counters(&stats)
        , counts_alive(true)
        , bias(value) {
        ++counters->alive;
        payload[0] = 9;
    }

    TrackedFunctor(const TrackedFunctor& other)
        : counters(other.counters)
        , counts_alive(other.counts_alive)
        , bias(other.bias) {
        std::memcpy(payload, other.payload, payload_size);
        if (counters && counts_alive) {
            ++counters->alive;
            ++counters->copies;
        }
    }

    TrackedFunctor(TrackedFunctor&& other) noexcept
        : counters(other.counters)
        , counts_alive(other.counts_alive)
        , bias(other.bias) {
        std::memcpy(payload, other.payload, payload_size);
        if (counters && counts_alive) {
            ++counters->alive;
            ++counters->moves;
        }
    }

    TrackedFunctor& operator=(const TrackedFunctor&) = delete;
    TrackedFunctor& operator=(TrackedFunctor&&) = delete;

    ~TrackedFunctor() {
        if (counters && counts_alive) {
            --counters->alive;
            ++counters->destructions;
        }
    }

    int operator()(int x) const { return bias + x + payload[0]; }
};

inline int plusOne(int x) { return x + 1; }
inline int plusFive(int x) { return x + 5; }
inline int plusNoexcept(int x) noexcept { return x + 9; }

template <class Api>
SuiteStats runSuite(const DelegateLogSink& sink) {
    using Delegate = typename Api::template delegate<int(int)>;
    using DelegateRef = typename Api::template delegate_ref<int(int)>;
    using DelegateSbo = typename Api::template delegate_sbo<int(int)>;

    constexpr std::size_t defaultBytes = Api::default_bytes;
    constexpr std::size_t defaultAlign = Api::default_align;
    constexpr std::size_t highAlign = defaultAlign * 2u;
    constexpr std::size_t heapBytes = defaultBytes * 2u;

    using OverAligned = AlignedFunctor<highAlign>;
    using Oversized = LargeFunctor<heapBytes>;
    using OversizedAligned = LargeAlignedFunctor<heapBytes, highAlign>;
    using InlineAlignedDelegate = typename Api::template delegate<int(int), heapBytes, highAlign>;
    using InlineAlignedSbo = typename Api::template delegate_sbo<int(int), heapBytes, highAlign>;

    static_assert(std::is_same_v<typename Api::template sig_of_t<decltype(&MethodTarget::add)>, int(int)>,
                  "sig_of for non-const member mismatch");
    static_assert(std::is_same_v<typename Api::template sig_of_t<decltype(&MethodTarget::addConst)>, int(int)>,
                  "sig_of for const member mismatch");
    static_assert(std::is_same_v<typename Api::template sig_of_t<decltype(&plusOne)>, int(int)>,
                  "sig_of for free function mismatch");
    static_assert(std::is_same_v<typename Api::template sig_of_t<decltype(&plusNoexcept)>, int(int)>,
                  "sig_of for noexcept free function mismatch");

    static_assert(Delegate::template fits_inline<TinyFunctor>(), "TinyFunctor must fit default delegate");
    static_assert(DelegateSbo::template fits_inline<TinyFunctor>(), "TinyFunctor must fit default delegate_sbo");
    static_assert(!Delegate::template fits_inline<Oversized>(), "Oversized functor must not fit default delegate");
    static_assert(!DelegateSbo::template fits_inline<Oversized>(), "Oversized functor must not fit default delegate_sbo");
    static_assert(!Delegate::template fits_inline<OverAligned>(), "Over-aligned functor must not fit default delegate");
    static_assert(!DelegateSbo::template fits_inline<OverAligned>(), "Over-aligned functor must not fit default delegate_sbo");
    static_assert(InlineAlignedDelegate::template fits_inline<OverAligned>(), "Custom aligned delegate must fit over-aligned functor");
    static_assert(InlineAlignedSbo::template fits_inline<OverAligned>(), "Custom aligned delegate_sbo must fit over-aligned functor");
    static_assert([] {
        Delegate::template static_assert_fits_inline<TinyFunctor>();
        DelegateSbo::template static_assert_fits_inline<TinyFunctor>();
        return true;
    }(), "inline fit assertions must stay callable");

    static_assert(std::is_same_v<decltype(Api::template bind<&MethodTarget::add>(std::declval<MethodTarget&>())), Delegate>,
                  "free bind helper must return default delegate type");

    QElapsedTimer timer;
    timer.start();

    SuiteContext t(Api::suiteName(), sink);
    t.section(QStringLiteral("Type Surface"));
    t.info(QStringLiteral("default bytes=%1 default align=%2 heap=%3")
               .arg(defaultBytes)
               .arg(defaultAlign)
               .arg(Api::heap_enabled ? QStringLiteral("on") : QStringLiteral("off")));
    t.info(QStringLiteral("sizeof(delegate_ref<int(int)>)=%1 align=%2")
               .arg(sizeof(DelegateRef))
               .arg(alignof(DelegateRef)));
    t.info(QStringLiteral("sizeof(delegate<int(int)>)=%1 align=%2")
               .arg(sizeof(Delegate))
               .arg(alignof(Delegate)));
    t.info(QStringLiteral("sizeof(delegate_sbo<int(int)>)=%1 align=%2")
               .arg(sizeof(DelegateSbo))
               .arg(alignof(DelegateSbo)));
    t.checkEq(Delegate::template required_inline_bytes<TinyFunctor>(), sizeof(TinyFunctor),
              QStringLiteral("delegate required_inline_bytes tracks sizeof"));
    t.checkEq(Delegate::template required_inline_align<OverAligned>(), alignof(OverAligned),
              QStringLiteral("delegate required_inline_align tracks alignof"));
    t.check(!Delegate::template fits_inline<Oversized>(),
            QStringLiteral("default delegate reports oversized callable as non-inline"));
    t.check(!Delegate::template fits_inline<OverAligned>(),
            QStringLiteral("default delegate reports over-aligned callable as non-inline"));
    t.check(InlineAlignedDelegate::template fits_inline<OverAligned>(),
            QStringLiteral("custom aligned delegate reports over-aligned callable as inline"));
    t.check(DelegateSbo::template required_inline_bytes<TinyFunctor>() == sizeof(TinyFunctor),
            QStringLiteral("delegate_sbo mirrors inline size introspection"));

    t.section(QStringLiteral("delegate_ref Basics"));
    {
        DelegateRef ref;
        t.check(!ref, QStringLiteral("empty delegate_ref starts false"));
        ref = &plusOne;
        t.check(ref(9) == 10, QStringLiteral("delegate_ref calls free function"));

        auto borrowed = [total = 0](int x) mutable {
            total += x;
            return total;
        };
        ref = Api::borrow(borrowed);
        t.check(ref(3) == 3, QStringLiteral("delegate_ref borrow observes first state change"));
        t.check(ref(4) == 7, QStringLiteral("delegate_ref borrow shares mutable state"));

        DelegateRef copy = ref;
        t.check(copy(5) == 12, QStringLiteral("delegate_ref copy keeps same target"));

        MethodTarget target{11};
        ref = DelegateRef::template bind<&MethodTarget::add>(target);
        t.check(ref(4) == 15, QStringLiteral("delegate_ref bind works for non-const method"));

        const MethodTarget constTarget{20};
        ref = DelegateRef::template bind<&MethodTarget::addConst>(constTarget);
        t.check(ref(4) == 25, QStringLiteral("delegate_ref bind works for const method"));

        ref = nullptr;
        t.check(!ref, QStringLiteral("delegate_ref reset via nullptr"));
    }

    t.section(QStringLiteral("delegate Semantics"));
    {
        Delegate value = &plusOne;
        t.check(value.owning(), QStringLiteral("delegate function pointer path is owning"));
        t.check(value.uses_inline(), QStringLiteral("delegate function pointer path uses inline storage"));
        t.check(!value.uses_heap(), QStringLiteral("delegate function pointer path does not use heap"));
        t.check(value(7) == 8, QStringLiteral("delegate calls free function"));

        Delegate captureless = [](int x) { return x + 2; };
        t.check(captureless(5) == 7, QStringLiteral("delegate accepts captureless lambda as function pointer"));

        const TinyFunctor constFunctor{14};
        value = Api::borrow(constFunctor);
        t.check(value.non_owning(), QStringLiteral("delegate borrow accepts const functor"));
        t.check(value(3) == 17, QStringLiteral("delegate borrow calls const functor"));

        auto shared = [sum = 0](int x) mutable {
            sum += x;
            return sum;
        };
        value = Api::borrow(shared);
        t.check(value.non_owning(), QStringLiteral("delegate borrow enters ref mode"));
        t.check(!value.uses_inline(), QStringLiteral("delegate borrow does not report inline ownership"));
        t.check(!value.uses_heap(), QStringLiteral("delegate borrow does not use heap"));
        t.check(value(2) == 2, QStringLiteral("delegate borrow first call"));
        t.check(value(5) == 7, QStringLiteral("delegate borrow shares external state"));

        MethodTarget target{30};
        value = Api::template bind<&MethodTarget::add>(target);
        t.check(value.non_owning(), QStringLiteral("delegate bind uses ref mode"));
        t.check(value(6) == 36, QStringLiteral("delegate bind calls non-const member"));

        const MethodTarget constTarget{40};
        value = Api::template bind<&MethodTarget::addConst>(constTarget);
        t.check(value(6) == 47, QStringLiteral("delegate bind calls const member"));

        auto stateful = [count = 0](int x) mutable {
            count += x;
            return count;
        };
        Delegate owned = stateful;
        t.check(owned.owning(), QStringLiteral("delegate stores stateful lambda as owning callable"));
        t.check(owned(1) == 1, QStringLiteral("delegate owning lambda starts with copied state"));
        t.check(owned(2) == 3, QStringLiteral("delegate owning lambda keeps internal state"));
        t.check(stateful(4) == 4, QStringLiteral("original lambda keeps separate state from delegate copy"));

        Delegate moveOnly = [ptr = std::make_unique<int>(9)](int x) {
            return *ptr + x;
        };
        t.check(moveOnly(3) == 12, QStringLiteral("delegate stores move-only lambda"));
        Delegate moved = std::move(moveOnly);
        t.check(!moveOnly, QStringLiteral("moved-from delegate becomes empty"));
        t.check(moved(4) == 13, QStringLiteral("moved delegate keeps callable"));

        typename Delegate::fnptr_t nullFn = nullptr;
        moved = nullFn;
        t.check(!moved, QStringLiteral("delegate null function pointer resets safely"));
    }

    t.section(QStringLiteral("delegate_sbo Semantics"));
    {
        DelegateSbo value = &plusFive;
        t.check(value.uses_inline(), QStringLiteral("delegate_sbo function pointer path uses inline storage"));
        t.check(!value.uses_heap(), QStringLiteral("delegate_sbo function pointer path does not use heap"));
        t.check(value(1) == 6, QStringLiteral("delegate_sbo calls free function"));

        DelegateSbo small = TinyFunctor{12};
        t.check(small.uses_inline(), QStringLiteral("delegate_sbo small functor uses inline storage"));
        t.check(!small.uses_heap(), QStringLiteral("delegate_sbo small functor stays off heap"));
        t.check(small(8) == 20, QStringLiteral("delegate_sbo invokes inline functor"));

        DelegateSbo moveOnly = [ptr = std::make_unique<int>(21)](int x) {
            return *ptr + x;
        };
        t.check(moveOnly.uses_inline(), QStringLiteral("delegate_sbo stores move-only lambda inline"));
        DelegateSbo moved = std::move(moveOnly);
        t.check(!moveOnly, QStringLiteral("delegate_sbo moved-from instance becomes empty"));
        t.check(moved(4) == 25, QStringLiteral("delegate_sbo moved instance keeps move-only lambda"));

        typename DelegateSbo::fnptr_t nullFn = nullptr;
        moved = nullFn;
        t.check(!moved, QStringLiteral("delegate_sbo null function pointer resets safely"));
    }

    t.section(QStringLiteral("Custom Size And Alignment"));
    {
        InlineAlignedDelegate aligned = OverAligned{50};
        t.check(aligned.uses_inline(), QStringLiteral("custom delegate stores over-aligned callable inline"));
        t.check(aligned(2) == 55, QStringLiteral("custom delegate executes over-aligned callable"));

        InlineAlignedSbo alignedSbo = OverAligned{70};
        t.check(alignedSbo.uses_inline(), QStringLiteral("custom delegate_sbo stores over-aligned callable inline"));
        t.check(alignedSbo(2) == 75, QStringLiteral("custom delegate_sbo executes over-aligned callable"));

        using TinyDelegate = typename Api::template delegate<int(int), 16, defaultAlign>;
        using TinySbo = typename Api::template delegate_sbo<int(int), 16, defaultAlign>;
        TinyDelegate tiny = TinyFunctor{4};
        TinySbo tinySbo = TinyFunctor{6};
        t.check(tiny(10) == 14, QStringLiteral("minimum-byte delegate still works for tiny callable"));
        t.check(tinySbo(10) == 16, QStringLiteral("minimum-byte delegate_sbo still works for tiny callable"));
    }

    t.section(QStringLiteral("Lifetime Accounting"));
    {
        LifetimeCounters inlineCounters;
        {
            using InlineTracked = TrackedFunctor<8, alignof(int)>;
            Delegate tracked = InlineTracked{inlineCounters, 10};
            t.checkEq(inlineCounters.alive, 1, QStringLiteral("inline tracked callable alive after construction"));
            Delegate moved = std::move(tracked);
            t.check(!tracked, QStringLiteral("inline tracked delegate becomes empty after move"));
            t.check(moved(1) == 20, QStringLiteral("inline tracked callable still executes after move"));
            moved = nullptr;
            t.checkEq(inlineCounters.alive, 0, QStringLiteral("inline tracked callable destroyed after reset"));
        }
        t.check(inlineCounters.moves >= 1, QStringLiteral("inline tracked callable observed move construction"));
        t.check(inlineCounters.destructions >= 1, QStringLiteral("inline tracked callable observed destruction"));
    }

    t.section(QStringLiteral("Heap And Oversize Paths"));
    if constexpr (Api::heap_enabled) {
        {
            Delegate heapDelegate = Oversized{100};
            t.check(heapDelegate.uses_heap(), QStringLiteral("heap-enabled delegate promotes oversized callable to heap"));
            t.check(!heapDelegate.uses_inline(), QStringLiteral("heap-enabled delegate does not mark heap object as inline"));
            t.check(heapDelegate(1) == 106, QStringLiteral("heap-enabled delegate invokes oversized callable"));

            Delegate alignedHeap = OversizedAligned{200};
            t.check(alignedHeap.uses_heap(), QStringLiteral("heap-enabled delegate promotes oversized over-aligned callable to heap"));
            t.check(alignedHeap(1) == 208, QStringLiteral("heap-enabled delegate invokes oversized over-aligned callable"));
        }

        {
            DelegateSbo heapSbo = Oversized{300};
            t.check(heapSbo.uses_heap(), QStringLiteral("heap-enabled delegate_sbo promotes oversized callable to heap"));
            t.check(heapSbo(1) == 306, QStringLiteral("heap-enabled delegate_sbo invokes oversized callable"));
        }

        {
            LifetimeCounters heapCounters;
            using HeapTracked = TrackedFunctor<heapBytes, alignof(int)>;
            {
                Delegate tracked = HeapTracked{heapCounters, 400};
                t.check(tracked.uses_heap(), QStringLiteral("tracked oversized delegate instance uses heap"));
                t.checkEq(heapCounters.alive, 1, QStringLiteral("heap tracked callable alive after construction"));
                tracked = nullptr;
            }
            t.checkEq(heapCounters.alive, 0, QStringLiteral("heap tracked callable destroyed after reset"));
            t.check(heapCounters.destructions >= 1, QStringLiteral("heap tracked callable observed destruction"));
        }
    } else {
        t.check(!Delegate::template fits_inline<Oversized>(),
                QStringLiteral("no-heap delegate rejects oversized callable at fit-check level"));
        t.check(!DelegateSbo::template fits_inline<Oversized>(),
                QStringLiteral("no-heap delegate_sbo rejects oversized callable at fit-check level"));
        t.info(QStringLiteral("oversized callable construction is intentionally compile-time blocked when heap fallback is off"));
    }

    t.section(QStringLiteral("Stress"));
    {
        std::int64_t checksum = 0;
        MethodTarget target{3};
        auto borrowed = [acc = 0](int x) mutable {
            acc = (acc + x) % 1000;
            return acc;
        };

        for (int i = 0; i < 50000; ++i) {
            Delegate value;
            switch (i % 4) {
            case 0:
                value = &plusOne;
                checksum += value(i);
                break;
            case 1:
                value = Api::borrow(borrowed);
                checksum += value(3);
                break;
            case 2:
                value = Api::template bind<&MethodTarget::add>(target);
                checksum += value(i % 9);
                break;
            default:
                value = [n = i](int x) { return n + x; };
                checksum += value(2);
                break;
            }

            Delegate moved = std::move(value);
            if (moved) checksum += moved(1);
        }

        t.check(checksum > 0, QStringLiteral("delegate mixed-operation stress loop completed"));
    }

    {
        std::int64_t checksum = 0;
        auto counter = [sum = 0](int x) mutable {
            sum += x;
            return sum;
        };
        DelegateRef ref = Api::borrow(counter);
        for (int i = 1; i <= 60000; ++i) {
            checksum += ref(i & 7);
        }
        t.check(checksum > 0, QStringLiteral("delegate_ref stress loop completed"));
    }

    {
        LifetimeCounters counters;
        using InlineTracked = TrackedFunctor<8, alignof(int)>;
        std::int64_t checksum = 0;
        for (int i = 0; i < 12000; ++i) {
            DelegateSbo box = InlineTracked{counters, i & 15};
            checksum += box(i & 3);
            DelegateSbo moved = std::move(box);
            if (moved) checksum += moved(1);
        }
        t.check(checksum > 0, QStringLiteral("delegate_sbo inline stress loop completed"));
        t.checkEq(counters.alive, 0, QStringLiteral("delegate_sbo inline stress leaves no live tracked functors"));
        t.check(counters.destructions >= 12000, QStringLiteral("delegate_sbo inline stress destroys tracked functors"));
    }

    if constexpr (Api::heap_enabled) {
        LifetimeCounters counters;
        using HeapTracked = TrackedFunctor<heapBytes, alignof(int)>;
        std::int64_t checksum = 0;
        bool allHeap = true;
        for (int i = 0; i < 8000; ++i) {
            Delegate value = HeapTracked{counters, i & 31};
            allHeap = allHeap && value.uses_heap();
            Delegate moved = std::move(value);
            if (moved) checksum += moved(2);
        }
        t.check(allHeap, QStringLiteral("heap stress kept all oversized instances on heap"));
        t.check(checksum > 0, QStringLiteral("heap stress loop completed"));
        t.checkEq(counters.alive, 0, QStringLiteral("heap stress leaves no live tracked functors"));
        t.check(counters.destructions >= 8000, QStringLiteral("heap stress destroys tracked functors"));
    }

    return t.finish(timer.elapsed());
}

} // namespace delegate_demon

#endif // DELEGATE_DEMON_SUITE_COMMON_H
