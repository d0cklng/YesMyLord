QT       -= core gui
#QT       += network

TARGET = jinlord
TEMPLATE = app

#CONFIG += staticlib
#DEFINES += MINIUPNP_STATICLIB
DEFINES += XNETENGINE_STATICLIB
include( ../_common.pri )

INCLUDEPATH += ../base ../crypto ../module ../jinraknet ../jinchart
INCLUDEPATH += ../crypto/headers

LIBS += -ljinchart -ljinraknet -ljinmodule -ljinbase -ljincrypto
LIBS += -lminiupnpc
JWIN {
   LIBS += -lWs2_32
   LIBS += -lMswsock
   LIBS += -lPsapi
   LIBS += -lIphlpapi
}
JLINUX {
   INCLUDEPATH += ../libev
   LIBS += -lev
}
CONFIG += console   #otherwise, printf is not work.
#DEFINES += CHART_USING_RAKNET
#INCLUDEPATH += ../jinraknet
#LIBS += -ljinraknet
#LIBS += -lminiupnpc

SOURCES += \
    clientinfoallocator.cpp \
    xproxyassistant.cpp \
    jinlord.cpp \
    xoutward.cpp \
    xinward.cpp \
    main.cpp \
    rakagent.cpp \
    xlorssocket.cpp \
    lorderror.cpp

HEADERS += \
    clientinfoallocator.h \
    xproxyassistant.h \
    jinlord.h \
    xinward.h \
    xoutward.h \
    xlorddef.h \
    rakagent.h \
    lordinnerdef.h \
    isinkforrakagent.h \
    xlorssocket.h \
    isinkforlord.h \
    lorderror.h




#OTHER_FILES += \

SUBDIRS += \
    xlord.pro \
    xlors.pro

