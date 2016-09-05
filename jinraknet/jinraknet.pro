QT       -= core gui  #改成去qt依赖.

TARGET = jinraknet
TEMPLATE = lib

#for dynamic dll
#DEFINES += XNETENGINE_LIBRARY

#for static lib
CONFIG += staticlib
DEFINES += XNETENGINE_STATICLIB

!include( ../_common.pri ){
error("Can't include _common.pri")
 }

JWIN {
   LIBS += -lWs2_32
}
JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}

INCLUDEPATH += ../base ../module
INCLUDEPATH += ./RakNet
include( ./jinraknet.pri )

LIBS += -ljinbase -ljinmodule
SOURCES += XNetEngine.cpp

HEADERS += \
    XNetEngine.h \
    ImplictLibraryLoad.hpp \
    INetEngine.h \
    ISinkForNetEngine.h \
    macro.h \
    NetEngineDefs.h \
    oMisc.hpp \
    prefix.h \



#OTHER_FILES += \

