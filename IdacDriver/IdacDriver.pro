# -------------------------------------------------
# Project created by QtCreator 2009-04-17T11:33:42
# -------------------------------------------------
QT -= gui
TEMPLATE = lib
CONFIG += warn_on \
    staticlib \
    create_prl \
    debug_and_release
INCLUDEPATH += . \
    .. \
    ../Core
HEADERS += IdacDriver.h \
    IdacEnums.h \
    IdacChannelSettings.h \
    IdacSettings.h \
    Sample.h \
    Sleeper.h \
    IdacDriverUsb.h \
	IdacDriverSamplingThread.h \
    IdacCaps.h \
	IdacDriverWithThread.h \
	IdacDriverUsbEs.h	
SOURCES += IdacDriver.cpp \
    IdacDriverUsb.cpp \
	IdacDriverWithThread.cpp \
	IdacDriverUsbEs.cpp
win32:INCLUDEPATH += ../extern/win32
unix:INCLUDEPATH += ../extern/libusbx/include
CONFIG(debug, debug|release):DESTDIR = ../debug
else:DESTDIR = ../release
