QT       += core gui

CONFIG += c++1z c++17 strict_c++ utf8_source warn_on
win32 {
    QMAKE_CXXFLAGS += /std:c++17
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    algorithm/attack.cpp \
    algorithm/bitwise.cpp \
    algorithm/board.cpp \
    algorithm/define.cpp \
    algorithm/engine.cpp \
    algorithm/evaluation.cpp \
    algorithm/movegen.cpp \
    algorithm/util.cpp \
    chessboardview.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    promotionselectoindialog.cpp \
    test.cpp

HEADERS += \
    algorithm/attack.h \
    algorithm/bitwise.h \
    algorithm/board.h \
    algorithm/define.h \
    algorithm/engine.h \
    algorithm/evaluation.h \
    algorithm/movegen.h \
    algorithm/util.h \
    chessboardview.h \
    logger.h \
    mainwindow.h \
    promotionselectoindialog.h \
    test.h

FORMS += \
    mainwindow.ui \
    promotionselectoindialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/main.qrc
