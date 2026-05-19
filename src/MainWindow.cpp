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
#include <QSlider>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QRadialGradient>
#include <cmath>

// ── ToolbarVU ─────────────────────────────────────────────────────────────────

ToolbarVU::ToolbarVU(QWidget* parent) : QWidget(parent) {
    setFixedSize(140, 22);
    setToolTip("Master L/R output level");
}

void ToolbarVU::setLevels(float l, float r) {
    if (qAbs(m_l - l) > 0.002f || qAbs(m_r - r) > 0.002f) {
        m_l = l; m_r = r; update();
    }
}

void ToolbarVU::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(0x06, 0x06, 0x0e));

    // Thin bar geometry: 2px bar, 1px gap = 3px stride
    constexpr float barW   = 2.0f;
    constexpr float stride = 3.0f;
    const int N = int(float(width()) / stride);
    const int bH = height() / 2 - 1;

    auto drawBar = [&](float v, int y) {
        float level = qMin(v / 1.2f, 1.0f);
        for (int i = 0; i < N; i++) {
            float frac = float(i) / float(N - 1);
            bool  lit  = level > frac;
            QColor col;
            if      (frac >= 0.80f) col = lit ? QColor(230,  20,  20) : QColor(28,  3,  3);
            else if (frac >= 0.60f) col = lit ? QColor(240, 180,   0) : QColor(32, 22,  0);
            else                    col = lit ? QColor( 30, 210,  30) : QColor( 2, 26,  2);
            p.fillRect(QRectF(float(i) * stride, float(y), barW, float(bH)), col);
        }
    };

    drawBar(m_l, 0);
    drawBar(m_r, bH + 2);
}

// ── RotaryKnob ────────────────────────────────────────────────────────────────

RotaryKnob::RotaryKnob(QWidget* parent) : QWidget(parent) {
    setFixedSize(36, 36);
    setCursor(Qt::SizeVerCursor);
}

void RotaryKnob::setValue(int v) {
    int c = qBound(m_min, v, m_max);
    if (c != m_value) { m_value = c; update(); }
}

void RotaryKnob::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int    W = width(), H = height();
    const int    d = qMin(W, H) - 2;
    const QRectF knob((W - d) / 2.0, (H - d) / 2.0, d, d);
    const QPointF c  = knob.center();
    const float   r  = d / 2.0f;

    // Body
    QRadialGradient grad(c, r);
    grad.setColorAt(0.0, QColor(0x2c, 0x2c, 0x58));
    grad.setColorAt(1.0, QColor(0x0e, 0x0e, 0x22));
    p.setPen(QPen(QColor(0x40, 0x40, 0x72), 1.0));
    p.setBrush(grad);
    p.drawEllipse(knob);

    // Arc track (270° sweep, 7 o'clock → 5 o'clock)
    constexpr float inset = 4.5f;
    const QRectF arc = knob.adjusted(inset, inset, -inset, -inset);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0x2e, 0x2e, 0x5a), 2.5f, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(arc, 225 * 16, -270 * 16);

    // Active arc
    const float norm = (m_max > m_min) ? float(m_value - m_min) / float(m_max - m_min) : 0.f;
    if (norm > 0.001f) {
        p.setPen(QPen(QColor(0x50, 0x50, 0xc0), 2.5f, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, 225 * 16, int(-norm * 270.f * 16.f));
    }

    // Value text centred in knob body
    p.setPen(QColor(0xd0, 0xd0, 0x50));
    QFont f("Menlo", 1);
    f.setPixelSize(8);
    p.setFont(f);
    p.drawText(knob, Qt::AlignCenter, QString::number(m_value));
}

void RotaryKnob::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging    = true;
        m_dragStartY   = e->pos().y();
        m_dragStartVal = m_value;
    }
}

void RotaryKnob::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    const int   dy    = m_dragStartY - e->pos().y();       // up = positive = increase
    const float scale = qMax(0.15f, float(m_max - m_min) / 100.f)
                        / (e->modifiers() & Qt::ControlModifier ? 5.f : 1.f);
    const int newVal = qBound(m_min, m_dragStartVal + int(dy * scale), m_max);
    if (newVal != m_value) {
        m_value = newVal;
        update();
        if (!signalsBlocked()) emit valueChanged(m_value);
    }
}

void RotaryKnob::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    bool wasDrag = std::abs(e->pos().y() - m_dragStartY) > 3;
    m_dragging = false;
    if (!wasDrag)
        emit editRequested(m_label, m_value, m_min, m_max);
}

void RotaryKnob::wheelEvent(QWheelEvent* e) {
    const int newVal = qBound(m_min, m_value + (e->angleDelta().y() > 0 ? 1 : -1), m_max);
    if (newVal != m_value) {
        m_value = newVal;
        update();
        if (!signalsBlocked()) emit valueChanged(m_value);
    }
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
        // 4 white keys + 3 black keys
        const int   nW  = 4;
        const float kw  = float(s) / nW;
        const float kh  = float(s);
        const float bw  = kw * 0.58f;
        const float bh  = kh * 0.62f;

        const QColor wCol(0xa0, 0xa0, 0x60);
        const QColor sep (0x1a, 0x1a, 0x28);
        const QColor bCol(0x18, 0x18, 0x28);

        // White keys
        for (int i = 0; i < nW; i++) {
            p.setPen(QPen(sep, 0.8f));
            p.setBrush(wCol);
            p.drawRect(QRectF(i * kw + 0.3f, 0.3f, kw - 0.3f, kh - 0.6f));
        }

        // Black keys: 2 then gap then 1  (C# D# · F#)
        const float cx[] = { kw, 2*kw, 3*kw };
        p.setPen(Qt::NoPen);
        p.setBrush(bCol);
        for (float x : cx)
            p.drawRect(QRectF(x - bw*0.5f, 0.3f, bw, bh));
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
    m_updateTimer->setInterval(33);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateToolbarInfo);
    m_updateTimer->start();

    // TrackerView transport signals
    connect(m_trackerView, &TrackerView::orderSeekRequested, this, [this](int32_t order){
        if (m_engine) m_engine->seekToOrder(order);
    });
    connect(m_trackerView, &TrackerView::patternPlayFromCursor,
            this, [this](int32_t order, int32_t row, bool loop) {
        if (!m_engine) return;
        m_engine->playPatternFrom(order, row, loop);
        m_actPlayPause->setText("⏸");
        m_actStop->setEnabled(true);
    });
    connect(m_trackerView, &TrackerView::patternPlayFromStart,
            this, [this](int32_t order, bool loop) {
        if (!m_engine) return;
        m_engine->playPatternFrom(order, 0, loop);
        m_actPlayPause->setText("⏸");
        m_actStop->setEnabled(true);
    });
    connect(m_trackerView, &TrackerView::patternPlayRow,
            this, [this](int32_t order, int32_t row) {
        if (!m_engine) return;
        m_engine->playRow(order, row);
        m_actPlayPause->setText("⏸");
        m_actStop->setEnabled(true);
    });
    connect(m_trackerView, &TrackerView::stopRequested, this, &MainWindow::onStop);
}

MainWindow::~MainWindow() {
    if (m_engine) { m_engine->stop(); m_engine->wait(); }
}

void MainWindow::setupMenu() {
    QMenu* file = menuBar()->addMenu("&File");
    m_actNew = new QAction("&New",   this); m_actNew->setShortcut(QKeySequence::New);
    m_actSave= new QAction("&Save", this); m_actSave->setShortcut(QKeySequence::Save);
    auto* open = new QAction("&Open", this); open->setShortcut(QKeySequence::Open);
    connect(m_actNew,  &QAction::triggered, this, &MainWindow::onNew);
    connect(open,      &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::onSave);
    file->addAction(m_actNew); file->addAction(open); file->addAction(m_actSave);
    file->addSeparator();
    auto* quit = new QAction("&Quit", this); quit->setShortcut(QKeySequence::Quit);
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
        "QToolBar { background:#0a0a14; border-bottom:1px solid #2e2e5a; spacing:2px; padding:2px 4px; }"
        "QToolButton { color:#e0e040; background:#141428; border:1px solid #2e2e5a; padding:2px 8px; }"
        "QToolButton:hover   { background:#2e2e5a; }"
        "QToolButton:pressed { background:#4040a0; }"
        "QToolButton:disabled { color:#404040; border-color:#1e1e3a; }"
        "QLabel { color:#606040; font-size:9px; padding:0; background:transparent; }");

    const QString knobLblStyle =
        "color:#606040; font-size:9px; padding:0; background:transparent;";

    auto mkKnob = [&](const QString& lbl, int lo, int hi, int val, RotaryKnob*& out) -> QWidget* {
        auto* w   = new QWidget; w->setStyleSheet("background:transparent;");
        auto* lay = new QHBoxLayout(w); lay->setContentsMargins(2,0,2,0); lay->setSpacing(3);
        auto* l   = new QLabel(lbl); l->setStyleSheet(knobLblStyle);
        l->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
        out = new RotaryKnob; out->setRange(lo, hi); out->setValue(val); out->setLabel(lbl);
        lay->addWidget(l); lay->addWidget(out);
        return w;
    };

    // ── File ──────────────────────────────────────────────────────────────────
    tb->addAction(m_actNew);
    auto* openAct = new QAction("Open", this);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);
    tb->addAction(openAct);
    tb->addAction(m_actSave);
    tb->addSeparator();

    // ── MIDI ──────────────────────────────────────────────────────────────────
    m_actMidi = new QAction("MIDI", this);
    tb->addAction(m_actMidi);
    tb->addSeparator();

    // ── Transport ─────────────────────────────────────────────────────────────
    m_actStop = new QAction("■", this);
    m_actStop->setToolTip("Stop");
    connect(m_actStop, &QAction::triggered, this, &MainWindow::onStop);
    m_actStop->setEnabled(false);
    tb->addAction(m_actStop);

    m_actPlayPause = new QAction("▶", this);
    m_actPlayPause->setToolTip("Play / Pause");
    connect(m_actPlayPause, &QAction::triggered, this, &MainWindow::onPlayPause);
    tb->addAction(m_actPlayPause);

    m_actPlayFromStart = new QAction("|▶", this);
    m_actPlayFromStart->setToolTip("Play from beginning");
    connect(m_actPlayFromStart, &QAction::triggered, this, &MainWindow::onPlayFromStart);
    tb->addAction(m_actPlayFromStart);
    tb->addSeparator();

    // ── Position / seek ───────────────────────────────────────────────────────
    m_seeker = new QSlider(Qt::Horizontal, this);
    m_seeker->setRange(0, 1000);
    m_seeker->setValue(0);
    m_seeker->setEnabled(false);
    m_seeker->setMinimumWidth(120);
    m_seeker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_seeker->setStyleSheet(
        "QSlider::groove:horizontal { background:#1a1a2e; height:4px; border-radius:2px; }"
        "QSlider::sub-page:horizontal { background:#5050c0; border-radius:2px; }"
        "QSlider::handle:horizontal { background:#a0a060; border:none;"
        "                             width:8px; margin:-4px 0; border-radius:4px; }"
        "QSlider::handle:horizontal:disabled { background:#303050; }");
    connect(m_seeker, &QSlider::sliderReleased, this, [this]{
        if (m_engine) {
            double d = m_state.duration_seconds.load();
            m_engine->seekSeconds(m_seeker->value() / 1000.0 * d);
        }
    });
    tb->addWidget(m_seeker);
    tb->addSeparator();

    // ── Knob controls ─────────────────────────────────────────────────────────
    tb->addWidget(mkKnob("Octave",     0,   8,   4, m_knobOctave));
    tb->addWidget(mkKnob("Tempo",     32, 512, 125, m_knobTempo));
    tb->addWidget(mkKnob("Ticks/Row",  1,  31,   6, m_knobSpeed));
    tb->addWidget(mkKnob("Row/Beat",   1,  32,   4, m_knobRPB));
    tb->addWidget(mkKnob("Global Vol", 0, 128, 128, m_knobGlobalVol));

    connect(m_knobOctave, &RotaryKnob::valueChanged,
            this, [this](int v){ m_trackerView->setBaseOctave(v); });

    auto wireKnobEdit = [&](RotaryKnob* k) {
        connect(k, &RotaryKnob::editRequested,
                this, [this, k](const QString& lbl, int val, int lo, int hi) {
                    m_trackerView->requestKnobEdit(lbl, val, lo, hi,
                        [k](int v){ k->applyEdit(v); });
                });
    };
    wireKnobEdit(m_knobOctave);
    wireKnobEdit(m_knobTempo);
    wireKnobEdit(m_knobSpeed);
    wireKnobEdit(m_knobRPB);
    wireKnobEdit(m_knobGlobalVol);

    tb->addSeparator();

    // ── Setup ─────────────────────────────────────────────────────────────────
    m_actSetup = new QAction("⚙ Setup", this);
    tb->addAction(m_actSetup);
    tb->addSeparator();

    // ── VU ────────────────────────────────────────────────────────────────────
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
    m_actPlayPause->setText("⏸");
    m_actStop->setEnabled(true);
    m_seeker->setValue(0);
    m_seeker->setEnabled(true);
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
        m_actPlayPause->setText(nowPaused ? "▶" : "⏸");
    } else {
        // Thread finished (song ended or was stopped) — restart from current position
        m_engine->start();
        m_actPlayPause->setText("⏸");
    }
}

void MainWindow::onStop() {
    if (!m_engine) return;
    m_engine->rewind(); // stops audio thread, rewinds to 0, keeps module loaded
    m_actPlayPause->setText("▶");
    m_seeker->setValue(0);
}

void MainWindow::onSongFinished() {
    m_actPlayPause->setText("▶");
    // Thread has exited but module stays loaded; Play will restart it
}

void MainWindow::onNew() {
    // TODO: create new empty module
}

void MainWindow::onSave() {
    // TODO: export / save module
}

void MainWindow::onPlayFromStart() {
    if (!m_engine) return;
    m_engine->rewind();
    m_engine->start();
    m_actPlayPause->setText("⏸");
}

// ── Toolbar info update ───────────────────────────────────────────────────────

void MainWindow::updateToolbarInfo() {
    // Seek bar (don't update while user is dragging)
    if (!m_seeker->isSliderDown()) {
        double d = m_state.duration_seconds.load();
        if (d > 0.0)
            m_seeker->setValue(int(m_state.position_seconds.load() / d * 1000.0));
    }

    if (m_state.is_playing.load()) {
        m_knobTempo->blockSignals(true);
        m_knobSpeed->blockSignals(true);
        m_knobRPB  ->blockSignals(true);
        m_knobTempo->setValue(int(m_state.bpm.load() + 0.5));
        m_knobSpeed->setValue(m_state.current_speed.load());
        m_knobRPB  ->setValue(m_state.rows_per_beat.load());
        m_knobTempo->blockSignals(false);
        m_knobSpeed->blockSignals(false);
        m_knobRPB  ->blockSignals(false);
        m_vuMeter->setLevels(m_state.master_vu_left.load(),
                             m_state.master_vu_right.load());
    } else {
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
