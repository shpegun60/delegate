QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    delegate_demon_tests.cpp \
    delegate_demon_tests_heap.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    tiny_delegate.hpp \
    delegate_demon_suite_common.h \
    delegate_demon_tests.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
