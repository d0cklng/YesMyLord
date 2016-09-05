QT       -= core gui

TARGET = xlorb
TEMPLATE = app

DEFINES += XNETENGINE_STATICLIB
include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
DEFINES += XLORB
INCLUDEPATH += ../base ../module ../jinraknet

LIBS += -ljinraknet -ljinmodule -ljinbase
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
    xlorb.cpp \
    xlorb_main.cpp \
    clientinfoallocator.cpp \
    ioutercontact.cpp \
    outeradmitbynetengine.cpp \
    xlors_socket.cpp

HEADERS += \
    xlorb.h \
    clientinfoallocator.h \
    ioutercontact.h \
    outeradmitbynetengine.h \
    xlors_share.h \
    xlors_socket.h


