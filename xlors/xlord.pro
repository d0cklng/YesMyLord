QT       -= core gui

TARGET = xlord
TEMPLATE = app

DEFINES += XNETENGINE_STATICLIB
include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
DEFINES += XLORD
INCLUDEPATH += ../base ../module ../jinraknet

LIBS += -ljinraknet -ljinmodule -ljinbase
LIBS += -lminiupnpc
JWIN {
   LIBS += -lIphlpapi
}
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
    xlord.cpp \
    xlord_main.cpp \
    ioutercontact.cpp \
    outerfinderbynetengine.cpp \
    xlors_socket.cpp \
    xproxyassistant.cpp

HEADERS += \
    xlord.h \
    ioutercontact.h \
    outerfinderbynetengine.h \
    xlors_share.h \
    xlors_socket.h \
    xproxyassistant.h


