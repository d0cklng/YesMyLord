#-------------------------------------------------
#
# Project created by QtCreator 2015-09-01T22:31:06
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qcrypto
TEMPLATE = app

include( ../_common.pri )

JMINGW {  #win32-gcc => MINGW
   DEFINES += _TCHAR_DEFINED
}
INCLUDEPATH += ../base ../crypto ../module
INCLUDEPATH += ../crypto/headers

LIBS += -ljinmodule -ljinbase -ljincrypto

SOURCES += main.cpp\
        mainwidget.cpp \
    secretrequestdialog.cpp

HEADERS  += mainwidget.h \
    secretrequestdialog.h

FORMS    += mainwidget.ui \
    secretrequestdialog.ui
