QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    browser.cpp \
    browserview.cpp \
    browserwindow.cpp \
    datastorage.cpp \
    globals.cpp \
    k9lineedit.cpp \
    k9portage.cpp \
    k9shell.cpp \
    k9tabbar.cpp \
    main.cpp \
    rescanthread.cpp \
    tabwidget.cpp \
    versionstring.cpp

HEADERS += \
    browser.h \
    browserview.h \
    browserwindow.h \
    datastorage.h \
    globals.h \
    k9lineedit.h \
    k9portage.h \
    k9shell.h \
    k9tabbar.h \
    main.h \
    rescanthread.h \
    tabwidget.h \
    versionstring.h

FORMS += \
    browserwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
