message(qmake spec: $$QMAKESPEC)

win32 {
  DEFINES += JWIN
  CONFIG += JWIN
  gcc {  #win32-gcc => MINGW
    DEFINES += JMINGW
    CONFIG += JMINGW  #for .pro use
  }
  msvc {
    DEFINES += JMSVC
    CONFIG += JMSVC
  }
}

linux {
  DEFINES += JLINUX
  CONFIG += JLINUX
}

CONFIG(debug, debug|release) {
  CONFIG += JDEBUG
  DEFINES += JDEBUG JIN_MEMORY_DEBUG
  JOUTSUFFIX = "-debug"
}
