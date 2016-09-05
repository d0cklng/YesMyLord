QT       -= core gui

TARGET = xplow
TEMPLATE = app

include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
INCLUDEPATH += ../base ../module

LIBS += -ljinmodule -ljinbase
CONFIG += console   #otherwise, printf is not work.
#-ltommath
#for jinmodule depends on 'jinbase'&'jincrypto', here mush let 'jinmodule' at first

JWIN {
   LIBS += -lWs2_32
   LIBS += -lMswsock
   LIBS += -lPsapi
}
JLINUX {
   INCLUDEPATH += ../libev
   LIBS += -lev
}

SOURCES += main.cpp \
    xplow.cpp \
    plowsocket.cpp

HEADERS += \
    xplow.h \
    plowsocket.h


