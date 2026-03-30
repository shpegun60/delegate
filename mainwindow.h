#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void suiteFinished(bool ok);

private slots:
    void startTests();

private:
    void appendLog(const QString& line);
    void setStatus(const QString& text, const QString& color);

    Ui::MainWindow *ui;
    bool running_ = false;
};
#endif // MAINWINDOW_H
