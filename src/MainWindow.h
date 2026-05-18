#pragma once
#include <QMainWindow>
#include <QWidget>
#include <memory>
#include "ModuleState.h"

class AudioEngine;
class TrackerView;
class QLabel;
class QAction;
class QSpinBox;
class QSlider;
class QTreeWidget;
class QSplitter;
class QTabWidget;
class QTextEdit;
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
    void onNew();
    void onSave();
    void onPlayPause();
    void onStop();
    void onPlayFromStart();
    void onSongFinished();

private:
    void setupMenu();
    void setupToolbar();
    void startPlayback(const QString& path);
    void populateTree();
    void populateTabs();
    void clearAll();
    void updateToolbarInfo();

    ModuleState                  m_state;
    std::unique_ptr<AudioEngine> m_engine;

    // Central layout
    QSplitter*   m_splitter    {nullptr};
    QTreeWidget* m_tree        {nullptr};
    QTabWidget*  m_tabs        {nullptr};
    TrackerView* m_trackerView {nullptr};

    // Tab content
    QWidget*   m_tabGeneral  {nullptr};
    QWidget*   m_tabSamples  {nullptr};
    QWidget*   m_tabInstrs   {nullptr};
    QTextEdit* m_tabComments {nullptr};

    // General tab labels
    QLabel* m_genTitle    {nullptr};
    QLabel* m_genArtist   {nullptr};
    QLabel* m_genType     {nullptr};
    QLabel* m_genDuration {nullptr};
    QLabel* m_genCounts   {nullptr};

    // Sample / instrument lists
    QTreeWidget* m_sampleList {nullptr};
    QTreeWidget* m_instrList  {nullptr};

    // Toolbar — actions
    QAction*  m_actNew           {nullptr};
    QAction*  m_actSave          {nullptr};
    QAction*  m_actMidi          {nullptr};
    QAction*  m_actStop          {nullptr};
    QAction*  m_actPlayPause     {nullptr};
    QAction*  m_actPlayFromStart {nullptr};
    QAction*  m_actSetup         {nullptr};

    // Toolbar — spin controls
    QSpinBox* m_spinOctave    {nullptr};
    QSpinBox* m_spinTempo     {nullptr};
    QSpinBox* m_spinSpeed     {nullptr};
    QSpinBox* m_spinRPB       {nullptr};
    QSpinBox* m_spinGlobalVol {nullptr};

    // Toolbar — seeker
    QSlider*   m_seeker  {nullptr};

    // Toolbar — VU
    ToolbarVU* m_vuMeter {nullptr};

    QTimer* m_updateTimer {nullptr};
};
