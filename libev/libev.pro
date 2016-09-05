###########################################
#
#   libev functional.
#
##########################################
QT       -= core gui

TARGET = ev
TEMPLATE = lib

CONFIG += staticlib

include( ../_common.pri )

HEADERS  += \ 
    ev.h \
    event.h \
    ev_vars.h \
    ev_wrap.h

SOURCES += \
    ev.c \
    event.c



#OTHER_FILES += \
