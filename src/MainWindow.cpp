#include "MainWindow.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QSplitter>
#include <QTreeWidget>
#include <QApplication>
#include <QPainter>

// ── ToolbarVU ─────────────────────────────────────────────────────────────────

ToolbarVU::ToolbarVU(QWidget* parent) : QWidget(parent) {
    setFixedSize(80, 20);
    setToolTip("Master L/R output level");
}

void ToolbarVU::setLevels(float l, float r) {
    if (qAbs(m_l - l) > 0.005f || qAbs(m_r - r) > 0.005f) {
        m_l = l; m_r = r; update();
    }
}

void ToolbarVU::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    constexpr int N = 8;
    int W  = width();
    int bH = height() / 2 - 1; // bar height
    float ledW = float(W - (N - 1)) / float(N);
    if (ledW < 1.0f) ledW = 1.0f;

    auto drawBar = [&](float v, int y) {
        float level = qMin(v / 1.2f, 1.0f); // normalise same as channel strip
        for (int i = 0; i < N; i++) {
            bool lit = level > float(i) / float(N);
            QColor col;
            if      (i >= 6) col = lit ? QColor(225,   0,   0) : QColor( 35,  0,  0);
            else if (i >= 4) col = lit ? QColor(255, 200,   0) : QColor( 40, 30,  0);
            else             col = lit ? QColor(  0, 200,   0) : QColor(  0, 28,  0);
            int x = int(float(i) * (ledW + 1.0f));
            p.fillRect(x, y, int(ledW), bH, col);
        }
    };

    drawBar(m_l, 0);
    drawBar(m_r, bH + 2);
}

// ── MainWindow ────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenMPT");
    resize(1280, 760);
    setAcceptDrops(true);

    // Left tree panel
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(160);
    m_tree->setMaximumWidth(300);
    m_tree->setIndentation(12);
    m_tree->setStyleSheet(
        "QTreeWidget { background:#0a0a14; color:#e0e040; border:none; font-size:12px; }"
        "QTreeWidget::item:selected { background:#4040a0; }"
        "QTreeWidget::item:hover { background:#2e2e5a; }");

    m_trackerView = new TrackerView(m_state, this);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_trackerView);
    m_splitter->setSizes({200, 1080});
    m_splitter->setHandleWidth(2);
    m_splitter->setStyleSheet("QSplitter::handle { background: #2e2e5a; }");
    setCentralWidget(m_splitter);

    setupMenu();
    setupToolbar();

    statusBar()->setStyleSheet("QStatusBar { background:#0a0a14; color:#707070; }");
    statusBar()->showMessage("Drag & drop a module file, or use File > Open");

    // Periodic UI update (toolbar info + seek slider + VU)
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(80);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateToolbarInfo);
    m_updateTimer->start();

    // Order seek from TrackerView
    connect(m_trackerView, &TrackerView::orderSeekRequested, this, [this](int32_t order){
        if (m_engine) m_engine->seekToOrder(order);
    });
}

MainWindow::~MainWindow() {
    if (m_engine) { m_engine->stop(); m_engine->wait(); }
}

void MainWindow::setupMenu() {
    QMenu* file = menuBar()->addMenu("&File");
    auto* open = new QAction("&Open…", this);
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::onOpenFile);
    file->addAction(open);
    file->addSeparator();
    auto* quit = new QAction("&Quit", this);
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    file->addAction(quit);

    QMenu* play = menuBar()->addMenu("&Playback");
    m_actPlayPause = play->addAction("▶ Play / Pause");
    m_actPlayPause->setShortcut(Qt::Key_Space);
    connect(m_actPlayPause, &QAction::triggered, this, &MainWindow::onPlayPause);
    m_actStop = play->addAction("■ Stop");
    m_actStop->setShortcut(Qt::Key_Escape);
    connect(m_actStop, &QAction::triggered, this, &MainWindow::onStop);
}

void MainWindow::setupToolbar() {
    QToolBar* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setStyleSheet(
        "QToolBar { background:#0a0a14; border-bottom:1px solid #2e2e5a; spacing:4px; padding:2px; }"
        "QToolButton { color:#e0e040; background:#141428; border:1px solid #2e2e5a;"
        "  padding:2px 6px; }"
        "QToolButton:hover   { background:#2e2e5a; }"
        "QToolButton:pressed { background:#4040a0; }"
        "QLabel { color:#e0e040; font-size:12px; padding:0 2px; }"
    );

    auto* openAct = new QAction("📂 Open", this);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);
    tb->addAction(openAct);
    tb->addSeparator();

    m_actPlayPause = new QAction("▶ Play", this);
    connect(m_actPlayPause, &QAction::triggered, this, &MainWindow::onPlayPause);
    tb->addAction(m_actPlayPause);

    m_actStop = new QAction("■ Stop", this);
    connect(m_actStop, &QAction::triggered, this, &MainWindow::onStop);
    tb->addAction(m_actStop);

    tb->addSeparator();

    // Seek slider
    m_seeker = new QSlider(Qt::Horizontal);
    m_seeker->setRange(0, 1000);
    m_seeker->setFixedWidth(220);
    m_seeker->setStyleSheet(
        "QSlider::groove:horizontal { background:#141428; height:4px; }"
        "QSlider::handle:horizontal { background:#4040a0; width:10px; margin:-4px 0; border-radius:2px; }"
        "QSlider::sub-page:horizontal { background:#4040a0; }");
    connect(m_seeker, &QSlider::sliderReleased, this, [this]{
        if (m_engine && m_state.duration_seconds > 0.0)
            m_engine->seekSeconds(m_seeker->value() / 1000.0 * m_state.duration_seconds.load());
    });
    tb->addWidget(m_seeker);

    tb->addSeparator();

    // Info labels — match OpenMPT toolbar: Tempo / Spd / RPB
    auto mkLabel = [&](const char* prefix) -> QLabel* {
        tb->addWidget(new QLabel(prefix));
        auto* lbl = new QLabel("---");
        lbl->setFixedWidth(46);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color:#f0f050; font-family:Menlo,monospace; font-size:12px;");
        tb->addWidget(lbl);
        tb->addWidget(new QLabel(" "));
        return lbl;
    };

    m_lblTempo = mkLabel("Tempo:");
    m_lblSpeed = mkLabel("Spd:");
    m_lblRPB   = mkLabel("RPB:");

    tb->addSeparator();

    // Master VU
    tb->addWidget(new QLabel("VU:"));
    m_vuMeter = new ToolbarVU(this);
    tb->addWidget(m_vuMeter);
}

// ── Playback control ─────────────────────────────────────────────────────────

void MainWindow::openFile(const QString& path) {
    startPlayback(path);
}

void MainWindow::startPlayback(const QString& path) {
    if (m_engine) { m_engine->stop(); m_engine->wait(); }
    m_state.reset();
    m_trackerView->clearModule();
    clearTree();

    m_engine = std::make_unique<AudioEngine>(m_state, this);
    connect(m_engine.get(), &AudioEngine::loadFailed, this, [this](const QString& msg){
        QMessageBox::critical(this, "Load Error", msg);
    });
    connect(m_engine.get(), &AudioEngine::songFinished, this, &MainWindow::onSongFinished);

    if (!m_engine->loadFile(path)) return;

    m_trackerView->setModule(m_engine->getModule());
    m_trackerView->setFocus();
    m_engine->start();

    populateTree();

    QFileInfo fi(path);
    std::string typeStr;
    { std::lock_guard<std::mutex> lk(m_state.meta_mutex); typeStr = m_state.type_long; }
    setWindowTitle(QString("OpenMPT — %1").arg(fi.fileName()));
    statusBar()->showMessage(QString("%1  ·  %2").arg(fi.fileName(),
                              QString::fromStdString(typeStr)));
    m_actPlayPause->setText("⏸ Pause");
    m_actStop->setEnabled(true);
}

void MainWindow::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(this, "Open Module File", {},
        "Module Files (*.mod *.xm *.s3m *.it *.mptm *.669 *.amf *.ams *.dbm "
        "*.digi *.dmf *.dsm *.far *.gdm *.imf *.mdl *.med *.mtm *.okt *.ptm "
        "*.stm *.ult *.umx *.wow *.mo3 *.oxm);;"
        "All Files (*)");
    if (!path.isEmpty()) openFile(path);
}

void MainWindow::onPlayPause() {
    if (!m_engine) return;
    bool nowPaused = !m_engine->isPaused();
    m_engine->setPaused(nowPaused);
    m_actPlayPause->setText(nowPaused ? "▶ Play" : "⏸ Pause");
}

void MainWindow::onStop() {
    if (!m_engine) return;
    m_engine->stop(); m_engine->wait(); m_engine.reset();
    m_trackerView->clearModule();
    m_state.reset();
    clearTree();
    m_actPlayPause->setText("▶ Play");
    m_actStop->setEnabled(false);
    setWindowTitle("OpenMPT");
    statusBar()->showMessage("Drag & drop a module file, or use File > Open");
}

void MainWindow::onSongFinished() {
    m_actPlayPause->setText("▶ Play");
}

// ── Toolbar info update ───────────────────────────────────────────────────────

void MainWindow::updateToolbarInfo() {
    if (!m_seeker->isSliderDown() && m_state.duration_seconds > 0.0) {
        double t = m_state.position_seconds.load();
        double d = m_state.duration_seconds.load();
        m_seeker->setValue(int(t / d * 1000.0));
    }

    if (m_state.is_playing.load()) {
        m_lblTempo->setText(QString::number(m_state.bpm.load(), 'f', 1));
        m_lblSpeed->setText(QString::number(m_state.current_speed.load()));
        m_lblRPB  ->setText(QString::number(m_state.rows_per_beat.load()));
        m_vuMeter->setLevels(m_state.master_vu_left.load(),
                             m_state.master_vu_right.load());
    } else {
        m_lblTempo->setText("---");
        m_lblSpeed->setText("---");
        m_lblRPB  ->setText("---");
        m_vuMeter->setLevels(0.f, 0.f);
    }
}

// ── Tree panel ────────────────────────────────────────────────────────────────

void MainWindow::populateTree() {
    m_tree->clear();

    std::string title, typeStr;
    int nPat, nSamp, nInstr;
    std::vector<std::string> instrNames, sampleNames, patNames;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        title      = m_state.title;
        typeStr    = m_state.type_long;
        nPat       = m_state.num_patterns;
        nSamp      = m_state.num_samples;
        nInstr     = m_state.num_instruments;
        instrNames = m_state.instrument_names;
        sampleNames= m_state.sample_names;
        patNames   = m_state.pattern_names;
    }

    // Song header
    auto* songRoot = new QTreeWidgetItem(m_tree,
        QStringList{QString::fromStdString(title.empty() ? "(untitled)" : title)});
    songRoot->setExpanded(true);

    // Patterns
    auto* pats = new QTreeWidgetItem(songRoot,
        QStringList{QString("Patterns (%1)").arg(nPat)});
    pats->setExpanded(false);
    for (int i = 0; i < nPat; i++) {
        std::string name = (i < (int)patNames.size() && !patNames[i].empty())
            ? patNames[i] : std::to_string(i);
        new QTreeWidgetItem(pats, QStringList{
            QString("%1: %2").arg(i, 2, 10, QChar('0'))
                             .arg(QString::fromStdString(name))});
    }

    // Instruments (if any)
    if (nInstr > 0) {
        auto* instrs = new QTreeWidgetItem(songRoot,
            QStringList{QString("Instruments (%1)").arg(nInstr)});
        instrs->setExpanded(true);
        for (int i = 0; i < nInstr; i++) {
            std::string name = (i < (int)instrNames.size()) ? instrNames[i] : "";
            new QTreeWidgetItem(instrs, QStringList{
                QString("%1: %2").arg(i+1, 2, 10, QChar('0'))
                                 .arg(QString::fromStdString(name))});
        }
    }

    // Samples
    if (nSamp > 0) {
        auto* samps = new QTreeWidgetItem(songRoot,
            QStringList{QString("Samples (%1)").arg(nSamp)});
        samps->setExpanded(nInstr == 0); // expand if no instruments
        for (int i = 0; i < nSamp; i++) {
            std::string name = (i < (int)sampleNames.size()) ? sampleNames[i] : "";
            new QTreeWidgetItem(samps, QStringList{
                QString("%1: %2").arg(i+1, 2, 10, QChar('0'))
                                 .arg(QString::fromStdString(name))});
        }
    }

    m_tree->expandItem(songRoot);
}

void MainWindow::clearTree() {
    m_tree->clear();
}

// ── Drag & drop / window events ───────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    for (const QUrl& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) { openFile(url.toLocalFile()); break; }
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    onStop();
    e->accept();
}
