#pragma once
#include <QOpenGLWidget>
#include <QTimer>
#include "ModuleState.h"

struct ImGuiContext;
struct ImDrawList;
namespace openmpt { class module; class module_ext; }

// Sub-column positions within one channel cell
enum class EditCol : uint8_t { Note=0, Instr=1, Vol=2, FxCmd=3, FxParam=4, _Count=5 };

struct EditCursor {
    int32_t row     {0};
    int32_t channel {0};
    EditCol  col    {EditCol::Note};
};

class TrackerView : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit TrackerView(ModuleState& state, QWidget* parent = nullptr);
    ~TrackerView() override;

    void setModule(openmpt::module_ext* mod) { m_mod = mod; }
    void clearModule() { m_mod = nullptr; }

    int  baseOctave() const   { return m_baseOctave; }
    void setBaseOctave(int o) { m_baseOctave = std::clamp(o, 0, 8); }

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
    void drawStereoVU(ImDrawList* dl, float x, float y, float w, float h, float lvlL, float lvlR);

    // Pattern editing
    void handleEditKey(int qtKey, Qt::KeyboardModifiers mods, const QString& text);
    void enterNote(uint8_t note); // note = 1..120, 0=nothing, 0xFE=cut, 0xFF=off
    void advanceCursor();
    void clampCursor();

    ModuleState&          m_state;
    openmpt::module_ext*  m_mod        {nullptr};
    ImGuiContext*         m_imgui      {nullptr};
    QTimer                m_timer;
    double                m_lastTime   {0.0};

    // Layout cache
    float    m_charW       {8.0f};
    float    m_rowNumW     {40.0f};
    float    m_cellW       {130.0f};
    float    m_rowH        {16.0f};
    float    m_hscroll     {0.0f};
    float    m_scrollY     {0.0f};
    int32_t  m_lastPattern {-1};
    int32_t  m_lastRow     {-1};
    uint64_t m_muteState   {0};

    // Edit cursor
    EditCursor m_cur;
    int32_t    m_baseOctave    {4};
    int32_t    m_editStep      {1};
    bool       m_editMode      {false};
    int32_t    m_pluginPopupCh {-1};
};
