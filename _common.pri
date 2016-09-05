#-------------------------------------------------
# Project share setting. target for all
# First create @ Timelink Inc. 2013/01/04
# Last modify @ Home. 2015/01/11
# 这个文件设计来给所有子工程包含的，里面定义一些工程公共的东西，
# 另外不同的东西进不同的pri额外去定义
#-------------------------------------------------

! include( ./_autodef.pri )  {  #括号必须在同一行!! 坑.
  warning(" <1> Couldn't include autodef.pri! ")
  ! include( ../_autodef.pri )  {
    warning(" <2> Couldn't include autodef.pri! ")
  }
}

#CONFIG(debug, debug|release) {
#  JOUTSUFFIX = "-debug"
#}

#INCLUDEPATH += ../_define ../_interface ../_sharecode
#LIBS += -L$$QTDIR/lib
#LIBS += -L../../src/lib
LIBS += -L../lib$${JOUTSUFFIX}  #生成lib

#third-party path
#THIRDINC = "../../../third/include"
#THIRDLIB = "../../../third/lib"
#INCLUDEPATH += $${THIRDINC}
#LIBS += -L$${THIRDLIB}

#QMAKE_LFLAGS += /Profile
#win32{
#        LIBS += -lshell32 -lUser32 -lAdvapi32
#}
#linux{
#        INCLUDEPATH +=  /usr/include
#}

# 其他限定举例
# !win32
# macx:debug
# win32|macx


CONFIG(staticlib) {
 DESTDIR = ../lib$${JOUTSUFFIX}
#$$_PRO_FILE_PWD_/
}
else{
 DESTDIR = ../bin$${JOUTSUFFIX}  #基于生成目录.
}

message(Out DIR: $$DESTDIR)

LIBS += -L$$DESTDIR  #原来这也可以基于生成目录[makefile所在]
#LIBS += -L.

message(libs INC: $$LIBS)

#QMAKE_LFLAGS += /Profile
