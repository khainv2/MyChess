#include "logger.h"
#include <stdio.h>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

QString logger::currentLogFileName = "";
bool logger::logInfoMessage = false;
QString formatDateTime(quint64 timestamp){
    auto pattern = "dd/MM hh:mm:ss.zzz";
    return QDateTime::fromMSecsSinceEpoch(timestamp).toString(pattern);
}

void logger::handlerToFile(QtMsgType type, const QMessageLogContext &c, const QString &msg){
    if (type == QtInfoMsg && !logInfoMessage){
        return;
    }
    static QString logFileName = "";
    static QString folderName = "";
    if (logFileName.isEmpty()){
        QDir dir;
        dir.mkdir("logs");
        dir.cd("logs");
        logFileName = QString("logs/%1.log")
            .arg(QDateTime::currentDateTime().toString("dd_mm_yy_hh_mm_ss"));
    }
    currentLogFileName = logFileName;

    FILE *pFile = fopen(logFileName.toLocal8Bit().constData(), "a");
    auto localMsg = msg.toLocal8Bit();
    auto currentMs = QDateTime::currentMSecsSinceEpoch();
    auto dateTime = formatDateTime(currentMs);
    switch (type) {
    case QtDebugMsg:
        fprintf(pFile, "[%s] %s\n",
                dateTime.toLocal8Bit().constData(),
                localMsg.constData());
        break;
    case QtInfoMsg: {
        if (logInfoMessage){
            fprintf(pFile, "[%s] Info %s\n",
                    dateTime.toLocal8Bit().constData(),
                    localMsg.constData());
        }
        break;
    }
    case QtWarningMsg:
        fprintf(pFile, "[%s] WARN %s\n",
                dateTime.toLocal8Bit().constData(),
                localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(pFile, "[%s] CRITICAL %s\n",
                dateTime.toLocal8Bit().constData(),
                localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(pFile, "[%s] FATAL %s\n",
                dateTime.toLocal8Bit().constData(),
                localMsg.constData());
        break;
    }
    fclose(pFile);


    QTextStream stream(stdout);
    QString msgType;
    switch (type){
    case QtDebugMsg     : msgType = "[D]"; break;
    case QtInfoMsg      : msgType = "[I]"; break;
    case QtWarningMsg   : msgType = "[W]"; break;
    case QtCriticalMsg  : msgType = "[C]"; break;
    case QtFatalMsg     : msgType = "[F]"; break;
    }
    stream << "[" << dateTime << "]"
           << msgType
           << ": "
           << msg <<  "\n";
}

void logger::handlerToConsole(QtMsgType type, const QMessageLogContext &c, const QString &msg){
    if (type == QtInfoMsg && !logInfoMessage){
        return;
    }
    if (msg.contains("Ignoring to load")) return;
    if (msg.contains("QFSFileEngine")) return;
    if (msg.contains("Cannot mix debug and release libraries")) return;
    if (msg.contains("QWidgetWindow")) return;
    if (msg.contains("WM_DESTROY")) return;
    if (msg.contains("Cannot open file '', because: No file name specified")) return;
    if (msg.contains("libpng warning: iCCP: known incorrect sRGB profile")) return;

    auto currentMs = QDateTime::currentMSecsSinceEpoch();
    auto dateTime =  QDateTime::fromMSecsSinceEpoch(currentMs)
            .toString("hh:mm:ss.zzz");

    QTextStream stream(stdout);
    QString msgType;
    switch (type){
    case QtDebugMsg     : msgType = "[D]"; break;
    case QtInfoMsg      : msgType = "[I]"; break;
    case QtWarningMsg   : msgType = "[W]"; break;
    case QtCriticalMsg  : msgType = "[C]"; break;
    case QtFatalMsg     : msgType = "[F]"; break;
    }
    stream << "[" << dateTime << "]"
           << msgType
           << ": "
           << msg <<  "\n";
}

void logger::init(){
//
#ifdef QT_DEBUG
    qInstallMessageHandler(handlerToConsole);
//    qInstallMessageHandler(handlerToFile);
#else
    qInstallMessageHandler(handlerToFile);
#endif
}
