QT       -= core gui

TARGET = xlors
TEMPLATE = app

DEFINES += XNETENGINE_STATICLIB
include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
INCLUDEPATH += ../base ../module ../jinraknet

LIBS +=  -ljinraknet -ljinmodule -ljinbase
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

SOURCES += \
    xlors.cpp \
    clientinfoallocator.cpp \
    xlors_main.cpp

HEADERS += \
    xlors.h \
    clientinfoallocator.h \
    xlors_share.h


