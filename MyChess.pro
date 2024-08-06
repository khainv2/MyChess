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
    algorithm/chessboard.cpp \
    algorithm/define.cpp \
    algorithm/engine.cpp \
    algorithm/evaluation.cpp \
    algorithm/movegenerator.cpp \
    algorithm/util.cpp \
    chessboardview.cpp \
    main.cpp \
    mainwindow.cpp \
    test.cpp

HEADERS += \
    algorithm/attack.h \
    algorithm/chessboard.h \
    algorithm/define.h \
    algorithm/engine.h \
    algorithm/evaluation.h \
    algorithm/movegenerator.h \
    algorithm/util.h \
    chessboardview.h \
    mainwindow.h \
    test.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/main.qrc
