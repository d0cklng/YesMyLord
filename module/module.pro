QT       -= core gui
#QT       += network

TARGET = jinmodule
TEMPLATE = lib

CONFIG += staticlib
DEFINES += MINIUPNP_STATICLIB

include( ../_common.pri )

JWIN {
   LIBS += -lWs2_32
   LIBS += -lMswsock
}
JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
JLINUX {
   INCLUDEPATH += ../libev
   LIBS += -lev
}

INCLUDEPATH += ../base
INCLUDEPATH += ../crypto
INCLUDEPATH += ../crypto/headers
INCLUDEPATH += ../miniupnpc

LIBS += -ljinbase -ljincrypto
#LIBS += -lminiupnpc

SOURCES += \
    jintorrentmaker.cpp \
    jintorrent.cpp \
    jinkey160.cpp \
    jinasyncengine.cpp \
    jinfileasync.cpp \
    jinioport.cpp \
    jintcpsocket.cpp \
    jinudpsocket.cpp \
    jinnetaddress.cpp \
    jinnetstartup.cpp \
    jinassistthread.cpp \
    jindnsparser.cpp \
    jinpublickey.cpp \
    jinpackhelper.cpp \
    jincompress.cpp \
#    jinchart/jinchart.cpp \
#    jinchart/jinchartping.cpp \
#    jinchart/jinchartbucket.cpp \
#    jinchart/jinchartbuckets.cpp \
#    jinchart/jinchartrpc.cpp \
#    jinchart/jinchartsearch.cpp \
#    jinchart/jinchartstore.cpp \
    quicklz/quicklz.c \
    jinupnpc.cpp \
    jinhttpget.cpp \
    jinipc.cpp

HEADERS += \
    jintorrentmaker.h \
    jintorrent.h \
    jinkey160.h \
    jinasyncdef.h \
    jinasyncengine.h \
    jinfileasync.h \
    jinioport.h \
    jintcpsocket.h \
    jinudpsocket.h \
    jinnetaddress.h \
    jinnetstartup.h \
    jinerrorcode.h \
    jinassistthread.h \
    jindnsparser.h \
    jinpublickey.h \
    jinpackhelper.h \
    jincompress.h \
#    jinchart/jinchart.h \
#    jinchart/jinchartping.h \
#    jinchart/jinchartbucket.h \
#    jinchart/jinchartbuckets.h \
#    jinchart/jinchartrpc.h \
#    jinchart/jinchartsearch.h \
#    jinchart/jinchartdef.h \
#    jinchart/jinchartstore.h \
    quicklz/quicklz.h \
    jinupnpc.h \
    jinhttpget.h \
    jinipc.h



#OTHER_FILES += \

