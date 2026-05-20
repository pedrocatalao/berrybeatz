#pragma once
#include <functional>
#include <cstdint>
#include <deque>
#include <vector>
#include "ModuleState.h"
#include "PatternEditor.h"

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

// Single cell edit record for undo
struct CellEdit {
    int32_t     pat, row, ch;
    PatternCell before, after;
};
using UndoEntry = std::vector<CellEdit>;

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
    void clearModule() { m_mod = nullptr; m_undoStack.clear(); m_redoStack.clear(); m_sel.active = false; }
    void setFonts(ImFont* mono, ImFont* ui) { m_fontMono = mono; m_fontUI = ui; }

    int  baseOctave() const   { return m_baseOctave; }
    void setBaseOctave(int o) { m_baseOctave = std::clamp(o, 0, 8); }

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

private:
    void processInput();
    void drawPatternToolbar1(float x, float y, float w);
    void drawPatternToolbar2(float x, float y, float w);
    void drawOrderList(float x, float y, float w);
    void drawChannelStrip(float x, float y, float w);
    void drawPatternPanel(float x, float y, float w, float h);
    void drawStereoVU(ImDrawList* dl, float x, float y, float w, float h, float lvlL, float lvlR);

    void enterNote(uint8_t note);
    void advanceCursor();
    void clampCursor();

    // Undo / redo
    void pushEdit(UndoEntry e);
    void doUndo();
    void doRedo();

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
    int32_t    m_editStep      {1};
    bool       m_editMode      {false};
    int32_t    m_pluginPopupCh {-1};

    // Block selection
    PatSel m_sel;

    // Undo / redo stacks
    std::deque<UndoEntry> m_undoStack;
    std::deque<UndoEntry> m_redoStack;
    static constexpr int  kMaxUndo = 64;

    // Clipboard
    ClipData m_clip;

    // Dynamic layout
    float   m_chanH  {45.0f};
    float   m_patTop {0.0f};

    // Pattern toolbar state
    bool    m_showVU        {true};
    bool    m_showPluginRow {true};
    bool    m_loopPattern   {false};
    bool    m_followSong    {true};
    int32_t m_curInstrument {1};
    bool    m_overflowPaste {false};
    bool    m_detailView    {false};
};
