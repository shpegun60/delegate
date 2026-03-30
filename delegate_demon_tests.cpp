#include "delegate_demon_tests.h"

#include "delegate_demon_suite_common.h"
#include "tiny_delegate.hpp"

#include <QElapsedTimer>

namespace {

struct RealApi {
    static constexpr bool heap_enabled = TINY_DELEGATE_ENABLE_HEAP_FALLBACK != 0;
    static constexpr std::size_t default_bytes = TINY_DELEGATE_DEFAULT_BYTES;
    static constexpr std::size_t default_align = TINY_DELEGATE_DEFAULT_ALIGN;

    static QString suiteName() {
        return heap_enabled ? QStringLiteral("default/heap") : QStringLiteral("default/noheap");
    }

    template <class Sig, std::size_t InlineBytes = default_bytes, std::size_t InlineAlign = default_align>
    using delegate = tiny::delegate<Sig, InlineBytes, InlineAlign>;

    template <class Sig, std::size_t InlineBytes = default_bytes, std::size_t InlineAlign = default_align>
    using delegate_sbo = tiny::delegate_sbo<Sig, InlineBytes, InlineAlign>;

    template <class Sig>
    using delegate_ref = tiny::delegate_ref<Sig>;

    template <class T>
    using sig_of_t = tiny::sig_of_t<T>;

    template <class F>
    static auto borrow(F& f) {
        return tiny::borrow(f);
    }

    template <class F>
    static auto borrow(const F& f) {
        return tiny::borrow(f);
    }

    template <auto Method, class T>
    static auto bind(T& obj) {
        return tiny::bind<Method>(obj);
    }

    template <auto Method, class T>
    static auto bind(const T& obj) {
        return tiny::bind<Method>(obj);
    }
};

} // namespace

delegate_demon::SuiteStats runHeapEnabledDelegateSuite(const DelegateLogSink& logSink);

DelegateDemonReport runDelegateDemonTests(const DelegateLogSink& logSink)
{
    QElapsedTimer timer;
    timer.start();

    if (logSink) {
        logSink(QStringLiteral("tiny_delegate demon test starting"));
    }

    const auto defaultSuite = delegate_demon::runSuite<RealApi>(logSink);
    const auto heapSuite = runHeapEnabledDelegateSuite(logSink);

    DelegateDemonReport report;
    report.ok = defaultSuite.ok() && heapSuite.ok();
    report.suitePasses = (defaultSuite.ok() ? 1 : 0) + (heapSuite.ok() ? 1 : 0);
    report.suiteFailures = (defaultSuite.ok() ? 0 : 1) + (heapSuite.ok() ? 0 : 1);
    report.checkPasses = defaultSuite.passed + heapSuite.passed;
    report.checkFailures = defaultSuite.failed + heapSuite.failed;
    report.elapsedMs = timer.elapsed();
    report.headline = QStringLiteral("%1 | suites ok=%2 failed=%3 | checks ok=%4 failed=%5 | elapsed=%6 ms")
                          .arg(report.ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                          .arg(report.suitePasses)
                          .arg(report.suiteFailures)
                          .arg(report.checkPasses)
                          .arg(report.checkFailures)
                          .arg(report.elapsedMs);

    if (logSink) {
        logSink(QStringLiteral("\n=== Overall Summary ==="));
        logSink(report.headline);
    }

    return report;
}
