#pragma once
#include <QOpenGLWidget>
#include <QTimer>
#include <QString>
#include <functional>
#include "ModuleState.h"

struct ImGuiContext;
struct ImDrawList;
struct ImFont;
namespace openmpt { class module; class module_ext; }

struct KnobEditState {
    bool                      pendingOpen {false};
    QString                   title;
    int                       value {0};
    int                       vmin  {0};
    int                       vmax  {100};
    std::function<void(int)>  onConfirm;
};

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

    void requestKnobEdit(const QString& title, int value, int vmin, int vmax,
                         std::function<void(int)> onConfirm);

signals:
    void orderSeekRequested(int32_t order);
    void patternPlayFromCursor(int32_t order, int32_t row, bool loop);
    void patternPlayFromStart(int32_t order, bool loop);
    void patternPlayRow(int32_t order, int32_t row);
    void stopRequested();

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
    void drawPatternToolbar1();
    void drawPatternToolbar2();
    void drawOrderList();
    void drawChannelStrip();
    void drawPatternPanel();
    void drawStereoVU(ImDrawList* dl, float x, float y, float w, float h, float lvlL, float lvlR);

    // Pattern editing
    void handleEditKey(int qtKey, Qt::KeyboardModifiers mods, const QString& text);
    void enterNote(uint8_t note);
    void advanceCursor();
    void clampCursor();

    ModuleState&          m_state;
    openmpt::module_ext*  m_mod        {nullptr};
    ImGuiContext*         m_imgui      {nullptr};
    ImFont*               m_fontUI     {nullptr}; // proportional font for toolbar buttons
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

    // Dynamic layout (recomputed each frame from toggle state)
    float   m_chanH  {45.0f}; // CHANNAME_H+VU_H+PLUGIN_H, updated each frame
    float   m_patTop {0.0f};

    // Knob edit modal state
    KnobEditState m_knobEdit;

    // Pattern toolbar state
    bool    m_showVU        {true};
    bool    m_showPluginRow {true};
    bool    m_loopPattern   {false};
    bool    m_followSong    {true};
    int32_t m_curInstrument {1};
    bool    m_overflowPaste {false};
    bool    m_detailView    {false};
};
