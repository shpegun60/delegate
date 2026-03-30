#include "mainwindow.h"
#include "delegate_demon_tests.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTimer>

#include <exception>

namespace {

QString formatLogLine(const QString& line)
{
    const QString escaped = line.toHtmlEscaped();

    if (line.startsWith(QStringLiteral("[FAIL]")) || line.startsWith(QStringLiteral("FAIL |"))) {
        return QStringLiteral("<span style=\"color:#b00020;font-weight:700;\">%1</span>").arg(escaped);
    }
    if (line.startsWith(QStringLiteral("[PASS]")) || line.startsWith(QStringLiteral("PASS |"))) {
        return QStringLiteral("<span style=\"color:#0b7a0b;font-weight:700;\">%1</span>").arg(escaped);
    }
    if (line.startsWith(QStringLiteral("[INFO]"))) {
        return QStringLiteral("<span style=\"color:#666666;\">%1</span>").arg(escaped);
    }
    if (line.startsWith(QStringLiteral("==="))) {
        return QStringLiteral("<span style=\"color:#1f4e79;font-weight:700;\">%1</span>").arg(escaped);
    }
    if (line.startsWith(QStringLiteral("[DONE]"))) {
        return QStringLiteral("<span style=\"color:#444444;font-weight:700;\">%1</span>").arg(escaped);
    }

    return escaped;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("tiny_delegate demon test"));

    connect(ui->runButton, &QPushButton::clicked, this, &MainWindow::startTests);

    ui->summaryLabel->setText(
        QStringLiteral("The demon suite covers delegate_ref, delegate, delegate_sbo, bind/borrow, size and alignment "
                       "matrices, heap fallback, lifetime accounting and long stress loops."));
    setStatus(QStringLiteral("Idle"), QStringLiteral("#555555"));
    statusBar()->showMessage(QStringLiteral("Ready to run tiny_delegate demon test"));

    QTimer::singleShot(0, this, &MainWindow::startTests);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::startTests()
{
    if (running_) {
        return;
    }

    running_ = true;
    ui->runButton->setEnabled(false);
    ui->logBrowser->clear();
    ui->summaryLabel->setText(QStringLiteral("Running compile-time and runtime demon suites..."));
    setStatus(QStringLiteral("Running"), QStringLiteral("#b36b00"));
    statusBar()->showMessage(QStringLiteral("Executing demon suite..."));
    appendLog(QStringLiteral("Started at %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    appendLog(QStringLiteral("The suite intentionally mixes smoke, edge, lifetime, alignment, heap and stress checks."));

    DelegateDemonReport report;
    try {
        report = runDelegateDemonTests([this](const QString& line) {
            appendLog(line);
        });
    } catch (const std::exception& ex) {
        report.ok = false;
        report.suiteFailures = 1;
        report.checkFailures = 1;
        report.headline = QStringLiteral("FAIL | exception escaped from demon suite");
        appendLog(QStringLiteral("[FAIL] exception: %1").arg(QString::fromLocal8Bit(ex.what())));
    } catch (...) {
        report.ok = false;
        report.suiteFailures = 1;
        report.checkFailures = 1;
        report.headline = QStringLiteral("FAIL | unknown exception escaped from demon suite");
        appendLog(QStringLiteral("[FAIL] unknown exception escaped from demon suite"));
    }

    ui->summaryLabel->setText(report.headline);
    setStatus(report.ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
              report.ok ? QStringLiteral("#0b7a0b") : QStringLiteral("#b00020"));
    statusBar()->showMessage(report.headline);
    appendLog(QStringLiteral("Finished at %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    ui->runButton->setEnabled(true);
    running_ = false;
    emit suiteFinished(report.ok);
}

void MainWindow::appendLog(const QString& line)
{
    QString remaining = line;
    while (remaining.startsWith(QLatin1Char('\n'))) {
        ui->logBrowser->append(QString());
        remaining.remove(0, 1);
    }

    if (!remaining.isEmpty()) {
        ui->logBrowser->append(formatLogLine(remaining));
    }

    ui->logBrowser->moveCursor(QTextCursor::End);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
}

void MainWindow::setStatus(const QString& text, const QString& color)
{
    ui->statusValueLabel->setText(text);
    ui->statusValueLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-weight: 700; letter-spacing: 0.08em; }").arg(color));
}
