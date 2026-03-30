#ifndef DELEGATE_DEMON_TESTS_H
#define DELEGATE_DEMON_TESTS_H

#include <QString>
#include <QtGlobal>

#include <functional>

struct DelegateDemonReport {
    bool ok = false;
    int suitePasses = 0;
    int suiteFailures = 0;
    int checkPasses = 0;
    int checkFailures = 0;
    qint64 elapsedMs = 0;
    QString headline;
};

using DelegateLogSink = std::function<void(const QString&)>;

DelegateDemonReport runDelegateDemonTests(const DelegateLogSink& logSink);

#endif // DELEGATE_DEMON_TESTS_H
