#include "MainWindow.h"
#include "AudioEngine.h"
#include "TrackerView.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenMPT");
    resize(1200, 720);
    setAcceptDrops(true);

    m_trackerView = new TrackerView(m_state, this);
    setCentralWidget(m_trackerView);

    // Timer must be created before setupToolbar() which calls m_seekTimer->start()
    m_seekTimer = new QTimer(this);
    m_seekTimer->setInterval(200);
    connect(m_seekTimer, &QTimer::timeout, this, [this]{
        if (!m_seeker->isSliderDown() && m_state.duration_seconds > 0.0) {
            double t = m_state.position_seconds.load();
            double d = m_state.duration_seconds.load();
            m_seeker->setValue(int(t / d * 1000.0));
        }
    });

    setupMenu();
    setupToolbar();

    statusBar()->addWidget(new QLabel("Drag & drop a module file to open"));
}

MainWindow::~MainWindow() {
    if (m_engine) {
        m_engine->stop();
        m_engine->wait();
    }
}

void MainWindow::setupMenu() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    auto* openAct = new QAction("&Open…", this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpenFile);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    auto* quitAct = new QAction("&Quit", this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(quitAct);

    QMenu* playMenu = menuBar()->addMenu("&Playback");
    m_actPlayPause = playMenu->addAction("&Play / Pause", this, &MainWindow::onPlayPause);
    m_actPlayPause->setShortcut(Qt::Key_Space);
    m_actStop = playMenu->addAction("&Stop", this, &MainWindow::onStop);
    m_actStop->setShortcut(Qt::Key_Escape);
}

void MainWindow::setupToolbar() {
    QToolBar* tb = addToolBar("Playback");
    tb->setMovable(false);

    m_actPlayPause = tb->addAction("▶ Play");
    connect(m_actPlayPause, &QAction::triggered, this, &MainWindow::onPlayPause);

    m_actStop = tb->addAction("■ Stop");
    connect(m_actStop, &QAction::triggered, this, &MainWindow::onStop);

    tb->addSeparator();

    m_seeker = new QSlider(Qt::Horizontal, this);
    m_seeker->setRange(0, 1000);
    m_seeker->setFixedWidth(300);
    connect(m_seeker, &QSlider::sliderReleased, this, [this]{
        if (m_engine && m_state.duration_seconds > 0.0) {
            double t = m_seeker->value() / 1000.0 * m_state.duration_seconds.load();
            m_engine->seekSeconds(t);
        }
    });
    tb->addWidget(m_seeker);

    m_seekTimer->start();
}

void MainWindow::openFile(const QString& path) {
    startPlayback(path);
}

void MainWindow::startPlayback(const QString& path) {
    if (m_engine) {
        m_engine->stop();
        m_engine->wait();
    }

    m_state.reset();
    m_trackerView->clearModule();

    m_engine = std::make_unique<AudioEngine>(m_state, this);
    connect(m_engine.get(), &AudioEngine::loadFailed, this, [this](const QString& msg){
        QMessageBox::critical(this, "Load Error", msg);
    });
    connect(m_engine.get(), &AudioEngine::songFinished, this, &MainWindow::onSongFinished);

    if (!m_engine->loadFile(path)) return;

    // Hand the module pointer to TrackerView before starting the audio thread.
    // Pattern data (notes/effects) is read-only after load so calling
    // format_pattern_row_channel_command() from the UI thread while the audio
    // thread calls read() is safe per libopenmpt's thread-safety guarantees.
    m_trackerView->setModule(m_engine->getModule());
    m_engine->start();

    updateToolbarState();

    QFileInfo fi(path);
    setWindowTitle("OpenMPT — " + fi.fileName());
}

void MainWindow::updateToolbarState() {
    bool playing = m_engine != nullptr;
    m_actStop->setEnabled(playing);
}

void MainWindow::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(this,
        "Open Module File", {},
        "Module Files (*.mod *.xm *.s3m *.it *.mptm *.669 *.amf *.ams *.dbm "
        "*.digi *.dmf *.dsm *.far *.gdm *.imf *.mdl *.med *.mtm *.okt *.ptm "
        "*.stm *.ult *.umx *.wow *.mo3 *.oxm);;"
        "All Files (*)"
    );
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
    m_engine->stop();
    m_engine->wait();
    m_engine.reset();
    m_trackerView->clearModule();
    m_state.reset();
    updateToolbarState();
    setWindowTitle("OpenMPT");
}

void MainWindow::onSongFinished() {
    m_actPlayPause->setText("▶ Play");
}

void MainWindow::onSeek(int value) {
    if (m_engine && m_state.duration_seconds > 0.0) {
        double t = value / 1000.0 * m_state.duration_seconds.load();
        m_engine->seekSeconds(t);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    for (const QUrl& url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            openFile(url.toLocalFile());
            break;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    onStop();
    e->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Space:  onPlayPause(); break;
    case Qt::Key_Escape: onStop();      break;
    default: QMainWindow::keyPressEvent(e); break;
    }
}
