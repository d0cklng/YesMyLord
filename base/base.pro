QT       -= core gui
#QT       += network

TARGET = jinbase
TEMPLATE = lib

CONFIG += staticlib

include( ../_common.pri )

SOURCES += \
    jinassert.cpp \
    jinbuffer.cpp \
    jincodec.cpp \
    jindatatime.cpp \
    jinendian.cpp \
    jinfile.cpp \
    jinmemorycheck.cpp \
    jinmutex.cpp \
    jinrandom.cpp \
    jinsharedata.cpp \
    jinsignaledevent.cpp \
    jinthread.cpp \
    jintimer.cpp \
    jinlogger.cpp \
    jinstring.cpp \
    jinsetting.cpp \
    jinfiledir.cpp \
    jinplatformgap.cpp \
    jinfifobuff.cpp \
    jinsharememory.cpp

HEADERS += \
    jinassert.h \
    jinbuffer.h \
    jincodec.h \
    jindatatime.h \
    jinendian.h \
    jinfile.h \
    jinhashmap.h \
    jinlinkedlist.h \
    jinmemorycheck.h \
    jinmempool.h \
    jinmutex.h \
    jinrandom.h \
    jinsharedata.h \
    jinsignaledevent.h \
    jinsingleton.h \
    jinthread.h \
    jintimer.h \
    jinlogger.h \
    jinstring.h \
    jinsetting.h \
    jinfiledir.h \
    jinformatint.h \
    jinplatformgap.h \
    jinfifobuff.h \
    jinsharememory.h



#OTHER_FILES += \

