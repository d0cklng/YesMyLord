###########################################
#
#   miniupnpc functional.
#
##########################################
QT       -= core gui

TARGET = miniupnpc
TEMPLATE = lib

CONFIG += staticlib
DEFINES += MINIUPNP_STATICLIB

include( ../_common.pri )

HEADERS  += \
    codelength.h \
    connecthostport.h \
    igd_desc_parse.h \
    minisoap.h \
    minissdpc.h \
    miniupnpc.h \
    miniupnpc_declspec.h \
    miniupnpctypes.h \
    miniwget.h \
    minixml.h \
    portlistingparse.h \
    receivedata.h \
    upnpcommands.h \
    upnpdev.h \
    upnperrors.h \
    upnpreplyparse.h \
    miniupnpcstrings.h

SOURCES += \
    connecthostport.c \
    igd_desc_parse.c \
    listdevices.c \
    #minihttptestserver.c \
    minisoap.c \
    minissdpc.c \
    miniupnpc.c \
    #miniupnpcmodule.c \
    miniwget.c \
    minixml.c \
    minixmlvalid.c \
    portlistingparse.c \
    receivedata.c \
    #testigddescparse.c \
    #testminiwget.c \
    #testminixml.c \
    #testportlistingparse.c \
    #testupnpreplyparse.c \
    #upnpc.c \
    upnpcommands.c \
    upnpdev.c \
    upnperrors.c \
    upnpreplyparse.c \
    #wingenminiupnpcstrings.c



#OTHER_FILES += \
