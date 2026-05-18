#pragma once
#include <QOpenGLWidget>
#include <QTimer>
#include "ModuleState.h"

struct ImGuiContext;
struct ImDrawList;
namespace openmpt { class module; class module_ext; }

class TrackerView : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit TrackerView(ModuleState& state, QWidget* parent = nullptr);
    ~TrackerView() override;

    void setModule(openmpt::module_ext* mod) { m_mod = mod; }
    void clearModule() { m_mod = nullptr; }

signals:
    void orderSeekRequested(int32_t order);

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
    void drawOrderList();
    void drawChannelStrip();
    void drawPatternPanel();

    void drawLEDs(ImDrawList* dl, float x, float y, float w, float h, float level);

    ModuleState&      m_state;
    openmpt::module_ext*  m_mod   {nullptr};
    ImGuiContext*     m_imgui     {nullptr};
    QTimer            m_timer;
    double            m_lastTime  {0.0};

    // Layout cache (computed each frame from font metrics)
    float m_charW    {8.0f};
    float m_rowNumW  {40.0f};
    float m_cellW    {130.0f};

    // Horizontal scroll shared between channel strip and pattern table
    float    m_hscroll   {0.0f};
    // Smooth vertical scroll for pattern panel
    float    m_scrollY      {0.0f};
    int32_t  m_lastPattern  {-1};
    int32_t  m_lastRow      {-1};
    // Per-channel mute state (bit N = channel N muted, visual only for now)
    uint64_t m_muteState {0};
};
