#pragma once
#include <QOpenGLWidget>
#include <QTimer>
#include "ModuleState.h"

struct ImGuiContext;
namespace openmpt { class module; }

class TrackerView : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit TrackerView(ModuleState& state, QWidget* parent = nullptr);
    ~TrackerView() override;

    // Called by MainWindow so TrackerView can query pattern data
    void setModule(openmpt::module* mod) { m_mod = mod; }
    void clearModule() { m_mod = nullptr; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;

private:
    void renderUI();
    void drawPatternPanel();
    void drawVUMeters();
    void drawInfoBar();

    ModuleState&      m_state;
    openmpt::module*  m_mod     {nullptr};
    ImGuiContext*     m_imgui   {nullptr};
    QTimer            m_timer;
    double            m_lastTime{0.0};
};
