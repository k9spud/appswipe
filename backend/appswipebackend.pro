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
        importvdb.cpp \
        k9atom.cpp \
        k9atomaction.cpp \
        k9atomlist.cpp \
        k9portage.cpp \
        main.cpp \
        versionstring.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    datastorage.h \
    globals.h \
    importvdb.h \
    k9atom.h \
    k9atomaction.h \
    k9atomlist.h \
    k9portage.h \
    main.h \
    versionstring.h

RESOURCES += \
    resources.qrc
