#pragma once

#include <QtDebug>

namespace logger {
    extern bool logInfoMessage;
    extern QString currentLogFileName;
    void init();


    /*!
     * \brief messageHandler Quản lý các bản tin log, lưu vào file
     */
    void handlerToFile(QtMsgType type,
                              const QMessageLogContext &,
                              const QString &msg);

    /*!
     * \brief messageHandlerRemoveMarble Quản lý các bản tin log,
     * remove các log từ marble
     */
    void handlerToConsole(QtMsgType type,
                                   const QMessageLogContext &c,
                                   const QString &msg);

}



