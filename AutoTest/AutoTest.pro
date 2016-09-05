QT       -= core gui

TARGET = jintest
TEMPLATE = app

include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
INCLUDEPATH += ../base ../crypto ../module ../jinchart
INCLUDEPATH += ../crypto/headers

LIBS += -ljinchart -ljinmodule -ljinbase -ljincrypto
CONFIG += console
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
    TestHelper.cpp \
    usecasefileasync.cpp \
    usecasetcpsocket.cpp \
    usecaseudpsocket.cpp \
    usecasestar.cpp \
    usecasebase.cpp \
    usecasedns.cpp \
    usecasecrypto.cpp \
    usecaseipc.cpp

HEADERS  += \
    TestHelper.h \
    usecasefileasync.h \
    usecasetcpsocket.h \
    usecaseudpsocket.h \
    usecasestar.h \
    usecasebase.h \
    usecasedns.h \
    usecasecrypto.h \
    usecaseipc.h

#FORMS    += mainwidget.ui
