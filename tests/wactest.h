#ifndef WACTEST_H
#define WACTEST_H

#include <QObject>
#include <QString>
#include "../algorithm/define.h"
#include "../algorithm/board.h"

class WacTestRunner : public QObject {
    Q_OBJECT
public:
    explicit WacTestRunner(QObject *parent = nullptr);
    void run(const QString &epdPath, const QString &csvPath);

signals:
    void progress(int current, int total, const QString &id,
                  const QString &status, const QString &gotUci, qint64 elapsedMs);
    void finished(int correct, int total, const QString &summary);

private:
    static bool matchSan(const QString &san, kc::Move move, const kc::Board &board);
};

#endif // WACTEST_H
