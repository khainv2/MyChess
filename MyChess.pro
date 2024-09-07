QT       += core gui

CONFIG += c++1z c++17 strict_c++ utf8_source warn_on
win32 {
    QMAKE_CXXFLAGS += /std:c++17
}

QMAKE_CXXFLAGS_RELEASE += -O3 -march=native -flto
QMAKE_CFLAGS_RELEASE += -O3 -march=native -flto

QMAKE_LFLAGS_RELEASE += -Wl,-O1 -Wl,--as-needed -Wl,--gc-sections -flto
QMAKE_CXXFLAGS_RELEASE += -fno-rtti -fno-exceptions
CONFIG += ltcg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    algorithm/attack.cpp \
    algorithm/board.cpp \
    algorithm/define.cpp \
    algorithm/engine.cpp \
    algorithm/evaluation.cpp \
    algorithm/movegen.cpp \
    algorithm/perft.cpp \
    algorithm/util.cpp \
    chessboardview.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    promotionselectiondialog.cpp \
    treegraph.cpp

HEADERS += \
    algorithm/attack.h \
    algorithm/board.h \
    algorithm/define.h \
    algorithm/engine.h \
    algorithm/evaluation.h \
    algorithm/movegen.h \
    algorithm/perft.h \
    algorithm/util.h \
    chessboardview.h \
    logger.h \
    mainwindow.h \
    promotionselectiondialog.h \
    treegraph.h

FORMS += \
    mainwindow.ui \
    promotionselectiondialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/main.qrc
