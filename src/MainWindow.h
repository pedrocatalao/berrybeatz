#pragma once
#include <QMainWindow>
#include <QWidget>
#include <memory>
#include "ModuleState.h"

class AudioEngine;
class TrackerView;
class QLabel;
class QSlider;
class QAction;
class QTreeWidget;
class QSplitter;
class QTimer;

// ── Toolbar master VU widget ──────────────────────────────────────────────────
class ToolbarVU : public QWidget {
    Q_OBJECT
public:
    explicit ToolbarVU(QWidget* parent = nullptr);
    void setLevels(float l, float r);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    float m_l{0.f}, m_r{0.f};
};

// ── Main window ───────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onOpenFile();
    void onPlayPause();
    void onStop();
    void onSongFinished();

private:
    void setupMenu();
    void setupToolbar();
    void startPlayback(const QString& path);
    void populateTree();
    void clearTree();
    void updateToolbarInfo();

    ModuleState                  m_state;
    std::unique_ptr<AudioEngine> m_engine;

    // Central layout
    QSplitter*   m_splitter     {nullptr};
    QTreeWidget* m_tree         {nullptr};
    TrackerView* m_trackerView  {nullptr};

    // Toolbar widgets
    QAction*   m_actPlayPause   {nullptr};
    QAction*   m_actStop        {nullptr};
    QSlider*   m_seeker         {nullptr};
    QLabel*    m_lblTempo       {nullptr};
    QLabel*    m_lblSpeed       {nullptr};
    QLabel*    m_lblRPB         {nullptr};
    QLabel*    m_lblVol         {nullptr};
    ToolbarVU* m_vuMeter        {nullptr};

    QTimer*    m_updateTimer    {nullptr};
};
