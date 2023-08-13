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
    algorithm/define.cpp \
    algorithm/movegenerator.cpp \
    chessboardview.cpp \
    main.cpp \
    mainwindow.cpp \
    util.cpp

HEADERS += \
    algorithm/attack.h \
    algorithm/bitwise.h \
    algorithm/define.h \
    algorithm/movegenerator.h \
    chessboardview.h \
    mainwindow.h \
    util.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/main.qrc
