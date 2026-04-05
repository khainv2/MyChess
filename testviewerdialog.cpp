#include "testviewerdialog.h"
#include "chessboardview.h"
#include "algorithm/util.h"
#include "tests/wactest.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QShortcut>
#include <QApplication>

TestViewerDialog::TestViewerDialog(const QString &engineCsv, const QString &myCsv,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("WAC Test Viewer");
    resize(900, 650);

    // Derive paths
    QFileInfo csvInfo(engineCsv);
    _epdPath = csvInfo.absolutePath() + "/wac.epd";

    // Auto-detect my CSV next to engine CSV
    _myCsvPath = myCsv;
    if (_myCsvPath.isEmpty()) {
        QString autoPath = csvInfo.absolutePath() + "/wac_my_results.csv";
        if (QFile::exists(autoPath))
            _myCsvPath = autoPath;
    }

    // Board
    _boardView = new ChessBoardView(this);
    _boardView->setInteractive(false);
    _boardView->setMinimumSize(420, 420);

    // Info panel
    _lblId       = new QLabel;
    _lblFen      = new QLabel;
    _lblExpected = new QLabel;
    _lblEngine   = new QLabel;
    _lblMy       = new QLabel;
    _lblScore    = new QLabel;
    _lblStatus   = new QLabel;
    _lblNav      = new QLabel;

    _lblFen->setWordWrap(true);
    _lblFen->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont boldFont = _lblStatus->font();
    boldFont.setPointSize(14);
    boldFont.setBold(true);
    _lblStatus->setFont(boldFont);
    _lblId->setFont(boldFont);

    // Navigation
    _btnPrev = new QPushButton("<< Prev");
    _btnNext = new QPushButton("Next >>");
    _chkFailOnly = new QCheckBox("FAIL only");
    _spinGoto = new QSpinBox;
    _spinGoto->setMinimum(1);
    _spinGoto->setPrefix("Go #");
    _btnRunMy = new QPushButton("Run MyChess Engine");

    auto *navLayout = new QHBoxLayout;
    navLayout->addWidget(_btnPrev);
    navLayout->addWidget(_btnNext);
    navLayout->addStretch();
    navLayout->addWidget(_chkFailOnly);
    navLayout->addWidget(_spinGoto);
    navLayout->addWidget(_btnRunMy);

    // Legend
    auto *legendLayout = new QHBoxLayout;
    auto makeLegend = [](const QString &text, const QString &color) {
        auto *lbl = new QLabel(text);
        lbl->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
        return lbl;
    };
    legendLayout->addWidget(makeLegend(QString::fromUtf8("\xe2\x96\xa0 Expected"), "#00aa00"));
    legendLayout->addWidget(makeLegend(QString::fromUtf8("\xe2\x96\xa0 Stockfish"), "#cc0000"));
    legendLayout->addWidget(makeLegend(QString::fromUtf8("\xe2\x96\xa0 MyChess"), "#cc9900"));
    legendLayout->addStretch();

    // Info group
    auto *infoGroup = new QGroupBox("Position Info");
    auto *infoLayout = new QVBoxLayout(infoGroup);
    infoLayout->addWidget(_lblId);
    infoLayout->addWidget(_lblStatus);
    infoLayout->addSpacing(8);
    infoLayout->addWidget(new QLabel("FEN:"));
    infoLayout->addWidget(_lblFen);
    infoLayout->addSpacing(8);
    infoLayout->addWidget(_lblExpected);
    infoLayout->addWidget(_lblEngine);
    infoLayout->addWidget(_lblMy);
    infoLayout->addWidget(_lblScore);
    infoLayout->addSpacing(8);
    infoLayout->addLayout(legendLayout);
    infoLayout->addStretch();
    infoLayout->addWidget(_lblNav);

    // Right panel
    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(infoGroup);

    // Main layout
    auto *mainLayout = new QVBoxLayout(this);
    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(_boardView, 1);
    bodyLayout->addLayout(rightLayout);
    mainLayout->addLayout(bodyLayout, 1);
    mainLayout->addLayout(navLayout);

    // Connections
    connect(_btnPrev, &QPushButton::clicked, this, &TestViewerDialog::goPrev);
    connect(_btnNext, &QPushButton::clicked, this, &TestViewerDialog::goNext);
    connect(_chkFailOnly, &QCheckBox::toggled, this, &TestViewerDialog::onFilterChanged);
    connect(_btnRunMy, &QPushButton::clicked, this, &TestViewerDialog::runMyEngine);
    connect(_spinGoto, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
        QString targetId = QString("WAC.%1").arg(val, 3, 10, QChar('0'));
        for (int i = 0; i < _filteredIndices.size(); i++) {
            if (_engineEntries[_filteredIndices[i]].id == targetId) {
                _currentFiltered = i;
                updateDisplay();
                return;
            }
        }
    });

    // Keyboard shortcuts
    new QShortcut(QKeySequence(Qt::Key_Left), this, SLOT(goPrev()));
    new QShortcut(QKeySequence(Qt::Key_Right), this, SLOT(goNext()));

    // Load data
    if (!loadCsv(engineCsv, _engineEntries)) {
        QMessageBox::warning(this, "Error",
            QString("Cannot load CSV: %1\n\nRun test_wac.py first.").arg(engineCsv));
        return;
    }

    // Load my engine results if available
    if (!_myCsvPath.isEmpty()) {
        QVector<TestEntry> myVec;
        if (loadCsv(_myCsvPath, myVec)) {
            for (const auto &e : myVec)
                _myEntries[e.id] = e;
        }
    }

    _spinGoto->setMaximum(_engineEntries.size());
    onFilterChanged();
}

bool TestViewerDialog::loadCsv(const QString &path, QVector<TestEntry> &out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    in.readLine(); // skip header

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(';');
        if (fields.size() < 7) continue;

        TestEntry entry;
        entry.id          = fields[0];
        entry.fen         = fields[1];
        entry.expectedSan = fields[2];
        entry.expectedUci = fields[3];
        entry.gotUci      = fields[4];
        entry.score       = fields[5];
        entry.status      = fields[6];
        out.append(entry);
    }

    return !out.isEmpty();
}

void TestViewerDialog::onFilterChanged()
{
    _filteredIndices.clear();
    bool failOnly = _chkFailOnly->isChecked();

    for (int i = 0; i < _engineEntries.size(); i++) {
        if (!failOnly || _engineEntries[i].status == "FAIL")
            _filteredIndices.append(i);
    }

    _currentFiltered = 0;
    updateDisplay();
}

void TestViewerDialog::goToIndex(int index)
{
    if (_filteredIndices.isEmpty()) return;
    _currentFiltered = qBound(0, index, _filteredIndices.size() - 1);
    updateDisplay();
}

void TestViewerDialog::goPrev() { goToIndex(_currentFiltered - 1); }
void TestViewerDialog::goNext() { goToIndex(_currentFiltered + 1); }

int TestViewerDialog::parseSquare(const QString &uci, int offset)
{
    if (offset + 1 >= uci.size()) return -1;
    int file = uci[offset].toLatin1() - 'a';
    int rank = uci[offset + 1].toLatin1() - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;
    return rank * 8 + file;
}

void TestViewerDialog::updateDisplay()
{
    if (_filteredIndices.isEmpty()) {
        _lblId->setText("No positions");
        return;
    }

    const TestEntry &e = _engineEntries[_filteredIndices[_currentFiltered]];

    // Update board
    kc::Board board;
    std::string fen = e.fen.toStdString() + " 0 1";
    parseFENString(fen, &board);
    _boardView->setBoard(board);

    // Build arrows
    std::vector<MoveArrow> arrows;

    // Expected move (green)
    QStringList expectedMoves = e.expectedUci.split(',', QString::SkipEmptyParts);
    if (!expectedMoves.isEmpty()) {
        QString first = expectedMoves.first().trimmed();
        int srcE = parseSquare(first, 0);
        int dstE = parseSquare(first, 2);
        arrows.push_back({srcE, dstE, QColor(0, 180, 0, 200)});
    }

    // Stockfish's move (red if wrong, blue if correct)
    if (!e.gotUci.isEmpty()) {
        int srcG = parseSquare(e.gotUci, 0);
        int dstG = parseSquare(e.gotUci, 2);
        QColor color = (e.status == "OK") ? QColor(0, 100, 255, 180) : QColor(220, 0, 0, 200);
        arrows.push_back({srcG, dstG, color});
    }

    // MyChess engine's move (yellow/orange)
    QString myMoveStr;
    QString myStatus;
    auto it = _myEntries.find(e.id);
    if (it != _myEntries.end()) {
        myMoveStr = it->gotUci;
        myStatus = it->status;
        if (!myMoveStr.isEmpty()) {
            int srcM = parseSquare(myMoveStr, 0);
            int dstM = parseSquare(myMoveStr, 2);
            QColor myColor = (it->status == "OK") ? QColor(200, 160, 0, 200) : QColor(200, 100, 0, 200);
            arrows.push_back({srcM, dstM, myColor});
        }
    }

    _boardView->setArrows(arrows);

    // Update labels
    _lblId->setText(e.id);
    _lblFen->setText(e.fen);
    _lblExpected->setText(QString("Expected:  %1  (%2)").arg(e.expectedSan, e.expectedUci));
    _lblEngine->setText(QString("Stockfish: %1  [%2]").arg(e.gotUci, e.status));
    _lblScore->setText(QString("SF Score:  %1").arg(e.score));

    if (it != _myEntries.end()) {
        _lblMy->setText(QString("MyChess:   %1  [%2]  score=%3")
            .arg(it->gotUci, it->status, it->score));
    } else {
        _lblMy->setText("MyChess:   (no data)");
    }

    if (e.status == "OK") {
        _lblStatus->setText("SF: OK");
        _lblStatus->setStyleSheet("color: #00aa00;");
    } else {
        _lblStatus->setText("SF: FAIL");
        _lblStatus->setStyleSheet("color: #cc0000;");
    }

    _lblNav->setText(QString("%1 / %2").arg(_currentFiltered + 1).arg(_filteredIndices.size()));

    _spinGoto->blockSignals(true);
    QString numStr = e.id.mid(4);
    _spinGoto->setValue(numStr.toInt());
    _spinGoto->blockSignals(false);
}

void TestViewerDialog::runMyEngine()
{
    if (!QFile::exists(_epdPath)) {
        QMessageBox::warning(this, "Error", "EPD file not found: " + _epdPath);
        return;
    }

    _myCsvPath = QFileInfo(_epdPath).absolutePath() + "/wac_my_results.csv";

    _btnRunMy->setEnabled(false);
    _btnRunMy->setText("Running...");
    QApplication::processEvents();

    auto *runner = new WacTestRunner(this);
    connect(runner, &WacTestRunner::progress, [this](int cur, int total, const QString &id,
            const QString &status, const QString &, qint64 ms) {
        _btnRunMy->setText(QString("Running... %1/%2 (%3 %4) %5ms")
            .arg(cur).arg(total).arg(id, status).arg(ms));
        QApplication::processEvents();
    });
    connect(runner, &WacTestRunner::finished, [this, runner](int correct, int total, const QString &summary) {
        _btnRunMy->setText("Run MyChess Engine");
        _btnRunMy->setEnabled(true);

        // Reload my results
        _myEntries.clear();
        QVector<TestEntry> myVec;
        if (loadCsv(_myCsvPath, myVec)) {
            for (const auto &e : myVec)
                _myEntries[e.id] = e;
        }
        updateDisplay();

        QMessageBox::information(this, "MyChess Test Complete", summary);
        runner->deleteLater();
    });

    runner->run(_epdPath, _myCsvPath);
}
