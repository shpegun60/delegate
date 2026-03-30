#define tiny tiny_heap
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 1
#include "tiny_delegate.hpp"
#undef tiny
#undef TINY_DELEGATE_ENABLE_HEAP_FALLBACK

#include "delegate_demon_suite_common.h"

namespace {

struct HeapApi {
    static constexpr bool heap_enabled = true;
    static constexpr std::size_t default_bytes = TINY_DELEGATE_DEFAULT_BYTES;
    static constexpr std::size_t default_align = TINY_DELEGATE_DEFAULT_ALIGN;

    static QString suiteName() { return QStringLiteral("shadow/heap"); }

    template <class Sig, std::size_t InlineBytes = default_bytes, std::size_t InlineAlign = default_align>
    using delegate = tiny_heap::delegate<Sig, InlineBytes, InlineAlign>;

    template <class Sig, std::size_t InlineBytes = default_bytes, std::size_t InlineAlign = default_align>
    using delegate_sbo = tiny_heap::delegate_sbo<Sig, InlineBytes, InlineAlign>;

    template <class Sig>
    using delegate_ref = tiny_heap::delegate_ref<Sig>;

    template <class T>
    using sig_of_t = tiny_heap::sig_of_t<T>;

    template <class F>
    static auto borrow(F& f) {
        return tiny_heap::borrow(f);
    }

    template <class F>
    static auto borrow(const F& f) {
        return tiny_heap::borrow(f);
    }

    template <auto Method, class T>
    static auto bind(T& obj) {
        return tiny_heap::bind<Method>(obj);
    }

    template <auto Method, class T>
    static auto bind(const T& obj) {
        return tiny_heap::bind<Method>(obj);
    }
};

} // namespace

delegate_demon::SuiteStats runHeapEnabledDelegateSuite(const DelegateLogSink& logSink)
{
    return delegate_demon::runSuite<HeapApi>(logSink);
}
