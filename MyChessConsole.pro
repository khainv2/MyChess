QT += core
QT -= gui

CONFIG += c++1z c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -std=c++17
QMAKE_CXXFLAGS_RELEASE += -O3 -march=native -flto
QMAKE_CFLAGS_RELEASE += -O3 -march=native -flto
QMAKE_LFLAGS_RELEASE += -Wl,-O1 -Wl,--as-needed -Wl,--gc-sections -flto

SOURCES += \
    algorithm/attack.cpp \
    algorithm/board.cpp \
    algorithm/define.cpp \
    algorithm/engine.cpp \
    algorithm/evaluation.cpp \
    algorithm/movegen.cpp \
    algorithm/perft.cpp \
    algorithm/tt.cpp \
    algorithm/util.cpp \
    console_main.cpp

HEADERS += \
    algorithm/attack.h \
    algorithm/board.h \
    algorithm/define.h \
    algorithm/engine.h \
    algorithm/evaluation.h \
    algorithm/movegen.h \
    algorithm/perft.h \
    algorithm/tt.h \
    algorithm/util.h

TARGET = MyChessConsole
