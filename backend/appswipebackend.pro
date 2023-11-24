QT -= gui
QT += core sql

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        datastorage.cpp \
        globals.cpp \
        k9complexmask.cpp \
        k9portage.cpp \
        k9portagemasks.cpp \
        main.cpp \
        rescanthread.cpp \
        versionstring.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    datastorage.h \
    globals.h \
    k9complexmask.h \
    k9portage.h \
    k9portagemasks.h \
    main.h \
    rescanthread.h \
    versionstring.h

RESOURCES += \
    resources.qrc
