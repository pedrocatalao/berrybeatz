#pragma once
#include <QMainWindow>
#include <memory>
#include "ModuleState.h"

class AudioEngine;
class TrackerView;
class QLabel;
class QSlider;
class QAction;

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
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onOpenFile();
    void onPlayPause();
    void onStop();
    void onSongFinished();
    void onSeek(int value);

private:
    void setupMenu();
    void setupToolbar();
    void updateToolbarState();
    void startPlayback(const QString& path);

    ModuleState               m_state;
    std::unique_ptr<AudioEngine> m_engine;
    TrackerView*              m_trackerView  {nullptr};

    QAction*   m_actPlayPause {nullptr};
    QAction*   m_actStop      {nullptr};
    QSlider*   m_seeker       {nullptr};
    QLabel*    m_statusLabel  {nullptr};
    QTimer*    m_seekTimer    {nullptr};
};
