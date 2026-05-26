#pragma once
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ModuleState.h"
#include "PatternEditor.h"
#include "UndoManager.h"
#include "PopupModal.h"

struct ImFont;
struct ImDrawList;
namespace openmpt { class module; class module_ext; }

struct KnobEditState {
    bool                      pendingOpen {false};
    std::string               title;
    int                       value {0};
    int                       vmin  {0};
    int                       vmax  {100};
    float                     popupX {0.f};
    float                     popupY {0.f};
    std::function<void(int)>  onConfirm;
};

enum class EditCol : uint8_t { Note=0, Instr=1, Vol=2, FxCmd=3, FxParam=4, _Count=5 };

struct EditCursor {
    int32_t row     {0};
    int32_t channel {0};
    EditCol  col    {EditCol::Note};
};

// Block selection state
struct PatSel {
    bool    active    {false};
    int32_t rowAnchor {0};
    int32_t chAnchor  {0};
    int32_t rowCursor {0};
    int32_t chCursor  {0};
    int32_t rowMin() const { return std::min(rowAnchor, rowCursor); }
    int32_t rowMax() const { return std::max(rowAnchor, rowCursor); }
    int32_t chMin()  const { return std::min(chAnchor,  chCursor);  }
    int32_t chMax()  const { return std::max(chAnchor,  chCursor);  }
};

// Clipboard data
struct ClipData {
    std::vector<std::vector<PatternCell>> cells; // [row][ch]
    int32_t rows  {0};
    int32_t chans {0};
};

class TrackerView {
public:
    explicit TrackerView(ModuleState& state);
    ~TrackerView() = default;

    void setModule(openmpt::module_ext* mod) { m_mod = mod; }
    void clearModule() { m_mod = nullptr; m_undo.clear(); m_sel.active = false; m_muteState = 0; }
    void setFonts(ImFont* mono, ImFont* ui) { m_fontMono = mono; m_fontUI = ui; }
    void setContinuousScroll(bool v) { m_continuousScroll = v; }

    int  baseOctave() const    { return m_baseOctave; }
    void setBaseOctave(int o)  { m_baseOctave = std::clamp(o, 0, 8); }
    bool getLoopPattern() const { return m_loopPattern; }

    // Call each frame when the patterns tab is visible.
    // (x,y,w,h) is the area allocated by the App for the pattern editor.
    void render(float x, float y, float w, float h);

    // Callbacks wired up by App
    std::function<void(int32_t)>                onOrderSeekRequested;
    std::function<void(int32_t, int32_t, bool)> onPatternPlayFromCursor;
    std::function<void(int32_t, bool)>          onPatternPlayFromStart;
    std::function<void(int32_t, int32_t)>       onPatternPlayRow;
    std::function<void()>                       onStopRequested;
    std::function<void()>                       onInsertPatternRequested;
    std::function<void(bool)>                   onLoopToggled;

private:
    void processInput();
    void drawPatternToolbar1(float x, float y, float w);
    void drawPatternToolbar2(float x, float y, float w);
    void drawPatternToolbar3(float x, float y, float w);
    void drawOrderList(float x, float y, float w);
    void drawPatternPanel(float x, float y, float w, float h);
    void drawStereoVU(ImDrawList* dl, float x, float y, float w, float h, float lvlL, float lvlR);

    void enterNote(uint8_t note);
    void advanceCursor();
    void clampCursor();


    // Block operations
    void selCopy();
    void selPaste();
    void selDelete();
    void insertRow();
    void deleteRow();

    // Helper: set a cell with undo tracking
    void setCell(int32_t pat, int32_t row, int32_t ch, const PatternCell& cell);

    ModuleState&          m_state;
    openmpt::module_ext*  m_mod      {nullptr};
    ImFont*               m_fontMono {nullptr};
    ImFont*               m_fontUI   {nullptr};

    // Layout cache
    float    m_charW        {8.0f};
    float    m_rowNumW      {40.0f};
    float    m_cellW        {130.0f};
    float    m_rowH         {16.0f};
    float    m_hscroll      {0.0f};
    float    m_scrollY      {0.0f};
    // Table origins: screen positions at scroll=0, measured inside the table loop.
    float    m_tableOriginY {0.0f};
    float    m_tableOriginX {0.0f};  // screen X of channel 0 content start, scroll=0
    float    m_colStep      {0.0f};  // screen pitch per channel column (content + padding)
    int32_t  m_lastPattern  {-1};
    int32_t  m_lastRow      {-1};
    uint64_t m_muteState    {0};

    // Edit cursor
    EditCursor m_cur;
    float      m_visualRow     {0.f};
    int32_t    m_baseOctave    {4};
    int        m_editStep      {1};
    bool       m_editMode      {false};
    int32_t    m_pluginPopupCh {-1};
    PopupModal m_pluginPopup;

    // Block selection
    PatSel m_sel;

    UndoManager m_undo;

    // Clipboard
    ClipData m_clip;

    // Dynamic layout
    float   m_chanH  {45.0f};
    float   m_patTop {0.0f};

    // Pattern toolbar state
    bool    m_showVU        {true};
    bool    m_showWave      {true};
    bool    m_showPluginRow {true};
    bool    m_loopPattern   {false};
    bool    m_continuousScroll   {false};
    bool    m_followSong         {true};
    float   m_scrollSuppressTimer {0.f};
    int32_t m_lastAbsRow          {0};
    std::unordered_map<int32_t, float> m_slotDim;   // orderIdx → animated dim factor
    int32_t m_curInstrument {0};
    bool    m_overflowPaste {false};
    int     m_detailView    {0};     // 0=note+inst+vol+fx, 1=+vol, 2=+inst, 3=note
};
