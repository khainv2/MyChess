#ifndef TESTVIEWERDIALOG_H
#define TESTVIEWERDIALOG_H

#include <QDialog>
#include <QString>
#include <QVector>
#include <QMap>

class ChessBoardView;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;

struct TestEntry {
    QString id;
    QString fen;
    QString expectedSan;
    QString expectedUci;  // comma-separated if multiple
    QString gotUci;
    QString score;
    QString status;       // "OK" or "FAIL"
};

class TestViewerDialog : public QDialog
{
    Q_OBJECT
public:
    // engineCsv: Stockfish results, myCsv: MyChess engine results (optional)
    explicit TestViewerDialog(const QString &engineCsv, const QString &myCsv = "",
                              QWidget *parent = nullptr);

private slots:
    void goToIndex(int index);
    void goPrev();
    void goNext();
    void onFilterChanged();
    void runMyEngine();

private:
    bool loadCsv(const QString &path, QVector<TestEntry> &out);
    void updateDisplay();
    int parseSquare(const QString &uci, int offset);

    QVector<TestEntry> _engineEntries;        // Stockfish results
    QMap<QString, TestEntry> _myEntries;       // MyChess results by id
    QVector<int> _filteredIndices;
    int _currentFiltered = 0;
    QString _epdPath;
    QString _myCsvPath;

    // Widgets
    ChessBoardView *_boardView;
    QLabel *_lblId;
    QLabel *_lblFen;
    QLabel *_lblExpected;
    QLabel *_lblEngine;
    QLabel *_lblMy;
    QLabel *_lblScore;
    QLabel *_lblStatus;
    QLabel *_lblNav;
    QPushButton *_btnPrev;
    QPushButton *_btnNext;
    QPushButton *_btnRunMy;
    QCheckBox *_chkFailOnly;
    QSpinBox *_spinGoto;
};

#endif // TESTVIEWERDIALOG_H
