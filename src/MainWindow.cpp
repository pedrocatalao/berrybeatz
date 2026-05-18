#include "MainWindow.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include <libopenmpt/libopenmpt_ext.hpp>
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
#include <QTabWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

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

// ── Tab icons ─────────────────────────────────────────────────────────────────

static QIcon makeTabIcon(std::function<void(QPainter&, int)> paint) {
    constexpr int sz = 16;
    qreal dpr = qApp->devicePixelRatio();
    int px = int(sz * dpr);
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    paint(p, sz);
    return QIcon(pm);
}

static QIcon iconGeneral() {
    return makeTabIcon([](QPainter& p, int s) {
        QColor c(0xa0, 0xa0, 0x60);
        int cx = s/2, cy = s/2, r = s/2 - 2;
        int teeth = 8;
        QPainterPath path;
        for (int i = 0; i < teeth * 2; i++) {
            float a = float(i) * M_PI / teeth;
            float ri = (i % 2 == 0) ? r : r - 2.5f;
            float x = cx + ri * std::cos(a - M_PI/2);
            float y = cy + ri * std::sin(a - M_PI/2);
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        path.closeSubpath();
        p.fillPath(path, c);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x0a, 0x0a, 0x14));
        p.drawEllipse(QPoint(cx, cy), s/5, s/5);
    });
}

static QIcon iconPatterns() {
    return makeTabIcon([](QPainter& p, int s) {
        QColor c(0xa0, 0xa0, 0x60);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.save();
        p.translate(s*0.28f, s*0.72f);
        p.rotate(-25);
        p.drawEllipse(QRectF(-s*0.18f, -s*0.13f, s*0.36f, s*0.26f));
        p.restore();
        p.setPen(QPen(c, 1.5f));
        p.drawLine(QPointF(s*0.44f, s*0.67f), QPointF(s*0.44f, s*0.15f));
        QPainterPath flag;
        flag.moveTo(s*0.44f, s*0.15f);
        flag.cubicTo(s*0.85f, s*0.22f, s*0.75f, s*0.40f, s*0.44f, s*0.48f);
        p.setPen(Qt::NoPen);
        p.strokePath(flag, QPen(c, 1.5f));
    });
}

static QIcon iconSamples() {
    return makeTabIcon([](QPainter& p, int s) {
        QColor c(0xa0, 0xa0, 0x60);
        QPainterPath wave;
        float y0 = s * 0.5f, amp = s * 0.3f;
        wave.moveTo(0, y0);
        wave.cubicTo(s*0.25f, y0 - amp, s*0.25f, y0 - amp, s*0.5f, y0);
        wave.cubicTo(s*0.75f, y0 + amp, s*0.75f, y0 + amp, s*1.0f, y0);
        p.strokePath(wave, QPen(c, 1.8f, Qt::SolidLine, Qt::RoundCap));
    });
}

static QIcon iconInstruments() {
    return makeTabIcon([](QPainter& p, int s) {
        QColor white(0xa0, 0xa0, 0x60), black(0x50, 0x50, 0x30);
        float kw = s / 5.0f, kh = s - 2.0f, bw = kw * 0.6f, bh = kh * 0.58f;
        p.setPen(QPen(QColor(0x40,0x40,0x40), 0.5f));
        for (int i = 0; i < 5; i++) {
            p.setBrush(white);
            p.drawRect(QRectF(i * kw + 0.5f, 1, kw - 1, kh - 1));
        }
        static const int blacks[] = {0, 1, 3, 4};
        for (int pos : blacks) {
            p.setBrush(black);
            p.setPen(Qt::NoPen);
            p.drawRect(QRectF((pos + 1) * kw - bw/2, 1, bw, bh));
        }
    });
}

static QIcon iconComments() {
    return makeTabIcon([](QPainter& p, int s) {
        QColor c(0xa0, 0xa0, 0x60), dim(0x60, 0x60, 0x40);
        float m = 2, w = s - m*2, h = s - m*2;
        p.setPen(QPen(c, 1.0f));
        p.setBrush(QColor(0x0a,0x0a,0x14));
        p.drawRect(QRectF(m, m, w, h));
        p.setPen(QPen(dim, 1.2f));
        float lx = m + 2, lw = w - 4;
        for (int i = 0; i < 4; i++) {
            float ly = m + 3 + i * (h - 4) / 4.0f;
            if (i == 3) lw *= 0.55f;
            p.drawLine(QPointF(lx, ly), QPointF(lx + lw, ly));
        }
    });
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

    // ── Tabs ──────────────────────────────────────────────────────────────────
    const QString tabStyle =
        // pin the tab bar to the left edge of the widget
        "QTabWidget::tab-bar { left: 0px; }"
        // pane: sits flush under the tab bar line
        "QTabWidget::pane { border:1px solid #2e2e5a; border-top:none; background:#0a0a14; }"
        // individual tabs: top+left+right border, no bottom, rounded top corners
        "QTabBar::tab {"
        "  background:#0d0d1e; color:#707050;"
        "  padding:5px 14px 5px 10px;"
        "  border:1px solid #2e2e5a; border-bottom:none;"
        "  border-top-left-radius:4px; border-top-right-radius:4px;"
        "  margin-right:2px; margin-top:3px;"
        "  font-size:12px; }"
        "QTabBar::tab:selected {"
        "  background:#0a0a14; color:#e0e040;"
        "  border-bottom-color:#0a0a14;"
        "  margin-top:0px; }"
        "QTabBar::tab:hover:!selected { background:#151528; color:#c0c070; }";
    const QString listStyle =
        "QTreeWidget { background:#0a0a14; color:#e0e040; border:none; font-size:12px; }"
        "QTreeWidget::item:selected { background:#4040a0; }"
        "QTreeWidget::item:hover    { background:#2e2e5a; }"
        "QHeaderView::section { background:#141428; color:#a0a060; border:none;"
        "                       border-right:1px solid #2e2e5a; padding:2px 4px; font-size:11px; }";
    const QString labelStyle = "color:#e0e040; font-size:12px;";
    const QString dimStyle   = "color:#707070; font-size:11px;";

    // General tab
    m_tabGeneral = new QWidget;
    m_tabGeneral->setStyleSheet("background:#0a0a14;");
    {
        auto* form = new QFormLayout(m_tabGeneral);
        form->setContentsMargins(16, 12, 16, 12);
        form->setSpacing(6);
        auto mkLabel = [&](QLabel*& lbl, const QString& placeholder) {
            lbl = new QLabel(placeholder);
            lbl->setStyleSheet(labelStyle);
            return lbl;
        };
        auto mkKey = [&](const QString& text) {
            auto* l = new QLabel(text);
            l->setStyleSheet(dimStyle);
            return l;
        };
        form->addRow(mkKey("Title:"),    mkLabel(m_genTitle,    "—"));
        form->addRow(mkKey("Artist:"),   mkLabel(m_genArtist,   "—"));
        form->addRow(mkKey("Type:"),     mkLabel(m_genType,     "—"));
        form->addRow(mkKey("Duration:"), mkLabel(m_genDuration, "—"));
        form->addRow(mkKey("Contents:"), mkLabel(m_genCounts,   "—"));
    }

    // Samples tab
    m_tabSamples = new QWidget;
    m_tabSamples->setStyleSheet("background:#0a0a14;");
    {
        auto* lay = new QVBoxLayout(m_tabSamples);
        lay->setContentsMargins(0, 0, 0, 0);
        m_sampleList = new QTreeWidget;
        m_sampleList->setStyleSheet(listStyle);
        m_sampleList->setHeaderLabels({"#", "Name"});
        m_sampleList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_sampleList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_sampleList->setRootIsDecorated(false);
        lay->addWidget(m_sampleList);
    }

    // Instruments tab
    m_tabInstrs = new QWidget;
    m_tabInstrs->setStyleSheet("background:#0a0a14;");
    {
        auto* lay = new QVBoxLayout(m_tabInstrs);
        lay->setContentsMargins(0, 0, 0, 0);
        m_instrList = new QTreeWidget;
        m_instrList->setStyleSheet(listStyle);
        m_instrList->setHeaderLabels({"#", "Name"});
        m_instrList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_instrList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_instrList->setRootIsDecorated(false);
        lay->addWidget(m_instrList);
    }

    // Comments tab
    m_tabComments = new QTextEdit;
    m_tabComments->setReadOnly(true);
    m_tabComments->setStyleSheet(
        "QTextEdit { background:#0a0a14; color:#e0e040; border:none;"
        "            font-family:Menlo,monospace; font-size:12px; }");

    m_tabs = new QTabWidget;
    m_tabs->setStyleSheet(tabStyle);
    m_tabs->setIconSize({16, 16});
    m_tabs->addTab(m_tabGeneral,  iconGeneral(),     "General");
    m_tabs->addTab(m_trackerView, iconPatterns(),    "Patterns");
    m_tabs->addTab(m_tabSamples,  iconSamples(),     "Samples");
    m_tabs->addTab(m_tabInstrs,   iconInstruments(), "Instruments");
    m_tabs->addTab(m_tabComments, iconComments(),    "Comments");
    m_tabs->setCurrentIndex(1); // start on Patterns
    m_tabs->tabBar()->setExpanding(false);          // tabs size to content
    m_tabs->tabBar()->setDocumentMode(false);        // keep border framing

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_tabs);
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
    clearAll();

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
    populateTabs();

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
    if (m_engine->isRunning()) {
        bool nowPaused = !m_engine->isPaused();
        m_engine->setPaused(nowPaused);
        m_actPlayPause->setText(nowPaused ? "▶ Play" : "⏸ Pause");
    } else {
        // Thread finished (song ended or was stopped) — restart from current position
        m_engine->start();
        m_actPlayPause->setText("⏸ Pause");
    }
}

void MainWindow::onStop() {
    if (!m_engine) return;
    m_engine->rewind(); // stops audio thread, rewinds to 0, keeps module loaded
    m_actPlayPause->setText("▶ Play");
}

void MainWindow::onSongFinished() {
    m_actPlayPause->setText("▶ Play");
    // Thread has exited but module stays loaded; Play will restart it
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

void MainWindow::clearAll() {
    m_tree->clear();
    m_genTitle->setText("—");
    m_genArtist->setText("—");
    m_genType->setText("—");
    m_genDuration->setText("—");
    m_genCounts->setText("—");
    m_sampleList->clear();
    m_instrList->clear();
    m_tabComments->clear();
}

void MainWindow::populateTabs() {
    std::string title, artist, typeStr, tracker, message;
    int nPat, nSamp, nInstr, nCh;
    std::vector<std::string> instrNames, sampleNames;
    double duration;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        title      = m_state.title;
        artist     = m_state.artist;
        typeStr    = m_state.type_long;
        tracker    = m_state.tracker;
        nPat       = m_state.num_patterns;
        nSamp      = m_state.num_samples;
        nInstr     = m_state.num_instruments;
        instrNames = m_state.instrument_names;
        sampleNames= m_state.sample_names;
        duration   = m_state.duration_seconds.load();
    }
    nCh = m_state.num_channels.load();

    // General
    m_genTitle->setText(QString::fromStdString(title.empty() ? "(untitled)" : title));
    m_genArtist->setText(QString::fromStdString(artist.empty() ? "—" : artist));
    m_genType->setText(QString::fromStdString(
        typeStr + (tracker.empty() ? "" : "  ·  " + tracker)));
    int mins = int(duration) / 60, secs = int(duration) % 60;
    m_genDuration->setText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
    m_genCounts->setText(QString("%1 patterns  ·  %2 channels  ·  %3 samples  ·  %4 instruments")
        .arg(nPat).arg(nCh).arg(nSamp).arg(nInstr));

    // Samples
    m_sampleList->clear();
    for (int i = 0; i < nSamp; i++) {
        std::string name = (i < (int)sampleNames.size()) ? sampleNames[i] : "";
        auto* item = new QTreeWidgetItem(m_sampleList,
            QStringList{QString("%1").arg(i+1, 2, 10, QChar('0')),
                        QString::fromStdString(name)});
        item->setForeground(0, QColor(0x70, 0x70, 0x70));
    }

    // Instruments
    m_instrList->clear();
    for (int i = 0; i < nInstr; i++) {
        std::string name = (i < (int)instrNames.size()) ? instrNames[i] : "";
        auto* item = new QTreeWidgetItem(m_instrList,
            QStringList{QString("%1").arg(i+1, 2, 10, QChar('0')),
                        QString::fromStdString(name)});
        item->setForeground(0, QColor(0x70, 0x70, 0x70));
    }

    // Comments — fetch live from module
    if (m_engine && m_engine->getModule()) {
        std::string msg = m_engine->getModule()->get_metadata("message");
        m_tabComments->setPlainText(QString::fromStdString(msg));
    }
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
