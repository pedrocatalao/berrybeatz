#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include "Theme.h"
#include "RenderSettings.h"
#include "TrackerView.h"
#include "ModuleController.h"
#include "PatternEditor.h"
#include "SoundModule.h"
#include <imgui.h>
#include "SwitchButton.h"
#include "LcdSpinner.h"
#include "LcdDropdown.h"
#include "LcdInput.h"
#include "OrderList.h"
#include "ControlGroup.h"
#include <algorithm>

// ── OpenMPT "Blue (FT2)" colour scheme — sourced from Theme.h ─────────────────
static constexpr ImVec4 kBgNormal   = kCellBgNormal;
static constexpr ImVec4 kTextNormal = kCellTextNormal;
static constexpr ImVec4 kTextCurRow = kCellTextCurRow;
static constexpr ImVec4 kBgHilight  = kCellBgBeat;
static constexpr ImVec4 kBgMeasure  = kCellBgMeasure;
static constexpr ImVec4 kColNote    = kCellNote;
static constexpr ImVec4 kColInstr   = kCellInstr;
static constexpr ImVec4 kColVolume  = kCellVolume;
static constexpr ImVec4 kColGlobal  = kCellEffect;
static constexpr ImVec4 kDim        = kCellDim;
static constexpr ImVec4 kChHeader   = kCellChanHdr;
static constexpr ImVec4 kOrderBg    = kCellOrderBg;
static constexpr ImVec4 kOrderCur   = kCellOrderCur;

static constexpr ImU32 kVuLoLit    = kVuCyanLit;
static constexpr ImU32 kVuLoUnlit  = kVuCyanUnlit;
static constexpr ImU32 kVuMedLit   = kVuPinkLit;
static constexpr ImU32 kVuMedUnlit = kVuPinkUnlit;
static constexpr ImU32 kVuHiLit    = kVuVioletLit;
static constexpr ImU32 kVuHiUnlit  = kVuVioletUnlit;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr float PTBAR1_H   = 120.0f;
static constexpr float ORDER_H    = 20.0f;
static constexpr float CHANNAME_H = 18.0f;
static constexpr float VU_H       = 13.0f;
static constexpr float WAVE_H     = 28.0f;
static constexpr float PLUGIN_H   = 14.0f;

// ── TrackerView ───────────────────────────────────────────────────────────────

TrackerView::TrackerView(ModuleState& state) : m_state(state) {}


// ── Toolbar helpers ───────────────────────────────────────────────────────────

static void tip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
        ImGui::SetTooltip("%s", text);
}


static void tbSep() {
    ImGui::SameLine(0, 5);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  h = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(
        {p.x, p.y + 2.f}, {p.x, p.y + h - 2.f},
        IM_COL32(46, 46, 90, 220), 1.0f);
    ImGui::Dummy({1.0f, h});
    ImGui::SameLine(0, 5);
}

static void pushToolbarStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32( 45,  45,  68, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32( 65,  65,  98, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32( 80,  80, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(208, 208, 224, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   IM_COL32( 80,  80,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32( 80,  80, 112, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32( 13,  13,  30, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32( 26,  26,  46, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      IM_COL32(208, 208, 224, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {8.0f, 3.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     {3.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
}

static void popToolbarStyle() {
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(10);
}

// ── Main render entry point ───────────────────────────────────────────────────

void TrackerView::render(float x, float y, float w, float h, float boxTopExt) {
    if (m_fontMono) ImGui::PushFont(m_fontMono);
    m_charW   = ImGui::CalcTextSize("A").x;
    m_rowNumW = m_charW * 3.0f + 10.0f;
    static constexpr float kCellChars[4] = {11.f, 8.f, 5.f, 3.f};
    static constexpr float kCellPad[4]  = {26.f, 20.f, 14.f, 8.f};
    m_cellW = m_charW * kCellChars[m_detailView] + kCellPad[m_detailView];
    m_rowH    = ImGui::GetTextLineHeightWithSpacing();
    if (m_fontMono) ImGui::PopFont();

    m_chanH  = CHANNAME_H
             + (m_showVU        ? VU_H     : 0.f)
             + (m_showWave      ? WAVE_H   : 0.f)
             + (m_showPluginRow ? PLUGIN_H : 0.f);
    constexpr float ORDER_GAP  = 6.f;
    const bool      hasModule  = m_ctrl != nullptr;
    const float     orderSlotH = hasModule ? (ORDER_H + ORDER_GAP) : 0.f;
    m_patTop = y + m_toolbar1H + orderSlotH + m_chanH;

    ImGuiWindowFlags noDecoFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    const ImVec4 kBarBg{0.f, 0.f, 0.f, 0.f};

    // Pattern toolbar — height adapts to group content (one-frame lag via m_toolbar1H)
    ImGui::SetNextWindowPos ({x, y});
    ImGui::SetNextWindowSize({w, m_toolbar1H});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
    ImGui::Begin("##ptbar1", nullptr, noDecoFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    drawPatternToolbar1(x, y, w);
    ImGui::End();

    // Box behind order list + pattern panel (extends up behind tab bar)
    {
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        ImVec2       p0 = {x + 4.f, y - boxTopExt};
        ImVec2       p1 = {x + w - 2.f, y + h};
        bg->AddRectFilled(p0, p1, kClrTabBodyBg);
    }

    // Pattern toolbar row 4 — order list (only when a module is loaded)
    if (hasModule) {
        float rowY = y + m_toolbar1H;
        ImGui::SetNextWindowPos ({x, rowY});
        ImGui::SetNextWindowSize({w, ORDER_H});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kBarBg);
        ImGui::Begin("##ptbar4", nullptr,
                     noDecoFlags | ImGuiWindowFlags_NoScrollbar
                                 | ImGuiWindowFlags_NoScrollWithMouse
                                 | ImGuiWindowFlags_NoMouseInputs);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        constexpr float RIGHT_MARGIN = 16.f;
        float cursorX = x + 2.f + m_rowNumW;
        float cursorY = ImGui::GetCursorScreenPos().y;
        float maxW    = (x + w) - cursorX - RIGHT_MARGIN;

        int32_t norders  = m_state.num_orders.load();
        int32_t curOrder = m_state.current_order.load();
        OrderList("##orders", "Order", m_fontUI, g_fontSidebar,
                  norders, curOrder,
                  [this](int32_t i) { return m_ctrl->module()->getOrderPattern(i); },
                  [this](int32_t i) { if (onOrderSeekRequested) onOrderSeekRequested(i); },
                  cursorX, cursorY, ORDER_H, maxW, noDecoFlags);

        ImGui::End();
    }

    // Pattern panel — channel headers are the frozen first row of the table
    float chanY = y + m_toolbar1H + orderSlotH;
    float patH  = h - (chanY - y);
    m_patAreaMin = {x + 2.f, chanY};
    m_patAreaMax = {x + w,   y + h};
    ImGui::SetNextWindowPos ({x + 2.f, chanY});
    ImGui::SetNextWindowSize({w - 2.f, patH});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          kClrScrollBg);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        kClrScrollGrab);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, kClrScrollHover);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  kClrScrollActive);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0)); // pushed last → popped first below
    ImGui::Begin("##pattern", nullptr, noDecoFlags);
    ImGui::PopStyleVar(1);   // WindowPadding — no longer needed after Begin
    ImGui::PopStyleColor(1); // WindowBg — no longer needed after Begin

    if (!ImGui::GetIO().WantTextInput) {
        auto prevCur = m_cur;
        processInput();
        m_editCursorMoved = (m_cur.row != prevCur.row || m_cur.channel != prevCur.channel);
    } else {
        m_editCursorMoved = false;
    }

    drawPatternPanel(x, chanY, w, patH);
    ImGui::End();
    ImGui::PopStyleColor(4); // scrollbar colors — must outlive End()

    // Phosphor ghost rendering — screen-space, on top of everything
    if (!m_phosphorGhosts.empty() && g_glowPattern > 0.f) {
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        fdl->PushClipRect(m_patAreaMin, m_patAreaMax, true);
        fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        for (auto& g : m_phosphorGhosts) {
            uint8_t a1 = (uint8_t)(g.alpha * 60.f * g_glowPattern);
            uint8_t a2 = (uint8_t)(g.alpha * 28.f * g_glowPattern);
            ImU32 c1 = (g.baseCol & 0x00FFFFFFu) | (uint32_t(a1) << 24);
            ImU32 c2 = (g.baseCol & 0x00FFFFFFu) | (uint32_t(a2) << 24);
            fdl->AddText(g.font, g.fsize, {g.x - 1, g.y - 1}, c1, g.text);
            fdl->AddText(g.font, g.fsize, {g.x + 1, g.y - 1}, c1, g.text);
            fdl->AddText(g.font, g.fsize, {g.x - 1, g.y + 1}, c1, g.text);
            fdl->AddText(g.font, g.fsize, {g.x + 1, g.y + 1}, c1, g.text);
            fdl->AddText(g.font, g.fsize, {g.x - 2, g.y - 2}, c2, g.text);
            fdl->AddText(g.font, g.fsize, {g.x + 2, g.y + 2}, c2, g.text);
        }
        fdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
        fdl->PopClipRect();
    }


    // Plugin settings popup (anchored to chanstrip window for ID stability)
    ImGui::SetNextWindowPos({x, chanY});
    ImGui::SetNextWindowSize({1, 1}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##plughost", nullptr, noDecoFlags | ImGuiWindowFlags_NoNav);
    if (m_pluginPopupCh >= 0) {
        m_pluginPopup.open();
        m_pluginPopupCh = -1;
    }
    if (m_pluginPopup.begin("Plugin Settings", 300.f, 76.f)) {
        ImGui::Spacing();
        ImGui::TextColored(kDim, "No plugin assigned.");
        m_pluginPopup.end();
    }
    ImGui::End();
}

// ── Pattern toolbar row 1 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar1(float /*x*/, float /*y*/, float /*w*/) {
    bool has = m_ctrl != nullptr;
    if (m_fontUI) ImGui::PushFont(m_fontUI);
    pushToolbarStyle();

    auto sw = [](const char* lbl, bool led = false) -> float {
        float tx = ImGui::CalcTextSize(lbl).x;
        float px = ImGui::GetStyle().FramePadding.x;
        return led ? (px + tx + 6.f + 4.f + 6.f) : (px * 2.f + tx);
    };

    auto spinW = [](const char* lbl, int vmax) -> float {
        float px  = ImGui::GetStyle().FramePadding.x;
        float lw  = ImGui::CalcTextSize(lbl).x;
        char  buf[12]; snprintf(buf, sizeof(buf), "%d", vmax);
        float lcdW = ImGui::CalcTextSize(buf).x + 8.f;
        return px + lw + 6.f + lcdW + 6.f + 18.f + 18.f + 4.f;
    };
    float stepW  = spinW("Step", 16);
    float instrW = ImGui::GetStyle().FramePadding.x + ImGui::CalcTextSize("Instr").x + 6.f + 130.f + 22.f + 2.f;
    float patW   = ImGui::GetStyle().FramePadding.x + ImGui::CalcTextSize("Pattern").x + 6.f + 120.f + 6.f;

    std::vector<ControlGroupDef> groups = {
        {"Edit", 270.f, 200.f, m_fontUI, {
            {sw("Undo"), [&] {
                bool dis = !(has && m_undo.canUndo());
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Undo##undo", "Undo")) m_undo.undo(*m_ctrl);
                tip("Undo");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Redo"), [&] {
                bool dis = !(has && m_undo.canRedo());
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Redo##redo", "Redo")) m_undo.redo(*m_ctrl);
                tip("Redo");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Insert"), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Insert##ins", "Insert"))
                    if (onInsertPatternRequested) onInsertPatternRequested();
                tip("Insert pattern");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Record", true), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                bool recLit = m_editMode && ((int)(ImGui::GetTime() * 2.0) % 2 == 0);
                if (SwitchButton("Record##rec", "Record", (int)recLit, 1, {0.f, 0.f}, IM_COL32(255, 60, 60, 255)))
                    setEditMode(!m_editMode);
                tip("Toggle record mode (Enter)");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Expand"), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Expand##exp", "Expand")) {}
                tip("Expand pattern");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Shrink"), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Shrink##shr", "Shrink")) {}
                tip("Shrink pattern");
                if (dis) ImGui::EndDisabled();
            }},
            {stepW, [&] {
                if (int d = LcdSpinner("Step##step", "Step", m_editStep, 0, 16))
                    m_editStep = std::clamp(m_editStep + d, 0, 16);
                tip("Edit step (- / + keys)");
            }},
            {instrW, [&] {
                std::vector<std::string> names;
                int total = 0;
                {
                    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
                    if (m_state.num_instruments > 0) { names = m_state.instrument_names; total = m_state.num_instruments; }
                    else                             { names = m_state.sample_names;     total = m_state.num_samples;     }
                }
                std::vector<std::string> fmtItems;
                fmtItems.reserve(total);
                for (int i = 1; i <= total; i++) {
                    char lbl[80]; int idx = i - 1;
                    if (idx < (int)names.size() && !names[idx].empty())
                        snprintf(lbl, sizeof(lbl), "%02d: %s", i, names[idx].c_str());
                    else
                        snprintf(lbl, sizeof(lbl), "%02d", i);
                    fmtItems.push_back(lbl);
                }
                std::vector<const char*> ptrs;
                ptrs.reserve(fmtItems.size());
                for (auto& s : fmtItems) ptrs.push_back(s.c_str());
                int sel = m_selectedInstrument - 1;
                if (!has) ImGui::BeginDisabled();
                if (LcdDropdown("##instr", "Instr", &sel, ptrs.data(), (int)ptrs.size(), "No instrument", 130.f)) {
                    m_selectedInstrument = sel + 1;
                    if (onInstrumentChanged) onInstrumentChanged(sel + 1);
                }
                if (!has) ImGui::EndDisabled();
                tip("Current instrument — click to pick from list");
            }},
            {sw("Plugin"), [&] {
                ImGui::BeginDisabled();
                SwitchButton("Plugin##plugin2", "Plugin");
                ImGui::EndDisabled();
                tip("Plugin selector (not yet available)");
            }},
        }},
        {"Playback", 180.f, 200.f, m_fontUI, {
            {sw("Play", true), [&] {
                bool patPlaying = m_state.is_pattern_playing.load();
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Play##play", "Play", patPlaying, 1,
                                 {0.f, 0.f}, kClrLedGreen)) {
                    if (patPlaying) {
                        if (onPauseRequested) onPauseRequested();
                    } else {
                        int32_t row = m_editMode ? m_cur.row : m_state.current_row.load();
                        if (onPatternPlayFromCursor)
                            onPatternPlayFromCursor(m_state.current_order.load(), row, m_loopPattern);
                    }
                }
                tip(patPlaying ? "Pause" : "Play from cursor");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Restart"), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Restart##patrestart", "Restart"))
                    if (onPatternPlayFromStart) onPatternPlayFromStart(m_state.current_order.load(), m_loopPattern);
                tip("Play pattern from start");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Play Row"), [&] {
                bool dis = !has;
                if (dis) ImGui::BeginDisabled();
                if (SwitchButton("Play Row##playrow", "Play Row")) {
                    int32_t ord = m_state.current_order.load();
                    int32_t row = m_cur.row;
                    advanceCursor();
                    if (onPatternPlayRow) onPatternPlayRow(ord, row);
                }
                tip("Play current row and advance cursor");
                if (dis) ImGui::EndDisabled();
            }},
            {sw("Loop", true), [&] {
                bool prev = m_loopPattern;
                if (SwitchButton("Loop##lp", "Loop", m_loopPattern, 1)) m_loopPattern = !m_loopPattern;
                if (m_loopPattern != prev && onLoopToggled) onLoopToggled(m_loopPattern);
                tip("Loop current pattern");
            }},
        }},
        {"View", 280.f, 200.f, m_fontUI, {
            {sw("VU", true), [&] {
                if (SwitchButton("VU##vu", "VU", m_showVU, 1)) m_showVU = !m_showVU;
                tip("Show/hide channel VU meters");
            }},
            {sw("WF", true), [&] {
                if (SwitchButton("WF##wf", "WF", m_showWave, 1)) m_showWave = !m_showWave;
                tip("Show/hide channel waveform");
            }},
            {sw("FX", true), [&] {
                if (SwitchButton("FX##fx", "FX", m_showPluginRow, 1)) m_showPluginRow = !m_showPluginRow;
                tip("Show/hide plugin row");
            }},
            {sw("Keyjazz", true), [&] {
                if (SwitchButton("Keyjazz##kj", "Keyjazz", m_showKbd, 1)) m_showKbd = !m_showKbd;
                tip("Show piano keyboard");
            }},
            {patW, [&] {
                int32_t curPat = m_ctrl ? m_state.current_pattern.load() : -1;
                std::string patName;
                if (curPat >= 0) {
                    std::lock_guard<std::mutex> lk(m_state.meta_mutex);
                    if (curPat < (int32_t)m_state.pattern_names.size())
                        patName = m_state.pattern_names[curPat];
                }
                char patBuf[64] = {};
                strncpy(patBuf, patName.c_str(), 63);
                if (!has) ImGui::BeginDisabled();
                LcdInput("##patname", "Pattern", patBuf, 64, 120.f);
                if (!has) ImGui::EndDisabled();
                tip("Pattern name");
            }},
            {sw("Detail", true), [&] {
                if (SwitchButton("Detail##detail", "Detail", 3 - m_detailView, 3))
                    m_detailView = (m_detailView + 1) % 4;
                static const char* detailTips[] = {
                    "Note + Instrument + Volume + Effect",
                    "Note + Instrument + Volume",
                    "Note + Instrument",
                    "Note only",
                };
                tip(detailTips[m_detailView]);
            }},
        }},
        {"Navigate", 120.f, 200.f, m_fontUI, {
            {sw("Follow", true), [&] {
                bool suspended = m_editMode && m_followSong;
                bool blink     = suspended && ((int)(ImGui::GetTime() * 2.0) % 2 == 0);
                ImU32 ledCol   = suspended ? kClrAmber : kClrLedGreen;
                int   ledVal   = suspended ? (int)blink : (int)m_followSong;
                if (SwitchButton("Follow##fw", "Follow", ledVal, 1, {0.f, 0.f}, ledCol) && !m_editMode)
                    m_followSong = !m_followSong;
                tip(suspended ? "Follow suspended while recording" : "Follow playback cursor");
            }},
            {sw("Seek", true), [&] {
                if (SwitchButton("Seek##sk", "Seek", m_seekMode, 1)) m_seekMode = !m_seekMode;
                tip("Scroll seeks play cursor (also: Option+Scroll)");
            }},
            {sw("Overflow", true), [&] {
                if (SwitchButton("Overflow##ovf", "Overflow", m_overflowPaste, 1)) m_overflowPaste = !m_overflowPaste;
                tip("Overflow paste");
            }},
        }},
    };

    float groupsH = DrawControlGroups(29.f, 6.f, ImGui::GetContentRegionMax().x - 29.f, groups);
    m_toolbar1H = 6.f + groupsH + 15.f;

    popToolbarStyle();
    if (m_fontUI) ImGui::PopFont();
}



// ── Stereo VU ─────────────────────────────────────────────────────────────────

void TrackerView::drawStereoVU(ImDrawList* dl, float x, float y, float w, float h,
                                float lvlL, float lvlR) {
    constexpr float barW   = 2.0f;
    constexpr float stride = 3.0f;
    float cx    = x + w * 0.5f;
    int   N     = std::max(1, int((w * 0.5f - 1.0f) / stride));
    float bH    = h - 1.0f;
    float normL = std::min(lvlL / 1.2f, 1.0f);
    float normR = std::min(lvlR / 1.2f, 1.0f);

    // Glow pass — additive blending, only lit segments
    if (g_glowVU > 0.f) {
        const float opa = g_glowVU * 0.5f;
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }, nullptr);
        for (int i = 0; i < N; i++) {
            float frac = float(i) / float(N);
            int gr, gg, gb;
            if      (frac >= 0.80f) { gr = 200; gg =   0; gb = 255; }
            else if (frac >= 0.60f) { gr = 255; gg =  30; gb = 180; }
            else                    { gr =   0; gg = 220; gb = 255; }
            float rx = cx + 1.0f + float(i) * stride;
            float lx = cx - 1.0f - float(i) * stride - barW;
            if (normR > frac) {
                dl->AddRectFilled({rx - 16, y - 6}, {rx + barW + 16, y + bH + 6}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({rx - 11, y - 5}, {rx + barW + 11, y + bH + 5}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({rx -  7, y - 3}, {rx + barW +  7, y + bH + 3}, IM_COL32(gr, gg, gb, (uint8_t)( 5 * opa)));
                dl->AddRectFilled({rx -  4, y - 2}, {rx + barW +  4, y + bH + 2}, IM_COL32(gr, gg, gb, (uint8_t)( 7 * opa)));
                dl->AddRectFilled({rx -  2, y - 1}, {rx + barW +  2, y + bH + 1}, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
                dl->AddRectFilled({rx -  1, y    }, {rx + barW +  1, y + bH    }, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
            }
            if (normL > frac) {
                dl->AddRectFilled({lx - 16, y - 6}, {lx + barW + 16, y + bH + 6}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({lx - 11, y - 5}, {lx + barW + 11, y + bH + 5}, IM_COL32(gr, gg, gb, (uint8_t)( 3 * opa)));
                dl->AddRectFilled({lx -  7, y - 3}, {lx + barW +  7, y + bH + 3}, IM_COL32(gr, gg, gb, (uint8_t)( 5 * opa)));
                dl->AddRectFilled({lx -  4, y - 2}, {lx + barW +  4, y + bH + 2}, IM_COL32(gr, gg, gb, (uint8_t)( 7 * opa)));
                dl->AddRectFilled({lx -  2, y - 1}, {lx + barW +  2, y + bH + 1}, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
                dl->AddRectFilled({lx -  1, y    }, {lx + barW +  1, y + bH    }, IM_COL32(gr, gg, gb, (uint8_t)(10 * opa)));
            }
        }
        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }, nullptr);
    }

    // Normal bars
    for (int i = 0; i < N; i++) {
        float frac = float(i) / float(N);
        ImU32 litCol, unlitCol;
        if      (frac >= 0.80f) { litCol = kVuHiLit;  unlitCol = kVuHiUnlit;  }
        else if (frac >= 0.60f) { litCol = kVuMedLit; unlitCol = kVuMedUnlit; }
        else                    { litCol = kVuLoLit;  unlitCol = kVuLoUnlit;  }
        float rx = cx + 1.0f + float(i) * stride;
        dl->AddRectFilled({rx, y}, {rx + barW, y + bH}, normR > frac ? litCol : unlitCol);
        float lx = cx - 1.0f - float(i) * stride - barW;
        dl->AddRectFilled({lx, y}, {lx + barW, y + bH}, normL > frac ? litCol : unlitCol);
    }
}


// ── Pattern panel ─────────────────────────────────────────────────────────────

void TrackerView::drawPatternPanel(float /*x*/, float /*y*/, float /*w*/, float /*h*/) {
    // Decay phosphor ghosts
    {
        const float dt    = ImGui::GetIO().DeltaTime;
        const float decay = powf(0.04f, dt);  // decays to ~4% over 1 second
        for (auto& g : m_phosphorGhosts) g.alpha *= decay;
        m_phosphorGhosts.erase(
            std::remove_if(m_phosphorGhosts.begin(), m_phosphorGhosts.end(),
                [](const PhosphorGhost& g){ return g.alpha < 0.02f; }),
            m_phosphorGhosts.end());
    }

    if (!m_ctrl || m_state.num_channels.load() <= 0) {
        ImGui::TextColored(kDim, " No module loaded.");
        return;
    }
    auto* mod = m_ctrl->module();

    int32_t curPat  = m_state.current_pattern.load();
    int32_t curRow  = m_state.current_row.load();
    int32_t nch     = m_state.num_channels.load();
    int32_t nrows   = mod->getPatternNumRows(curPat);

    int rpb = mod->getPatternRowsPerBeat(curPat);
    if (rpb <= 0) rpb = 4;
    int rpm = mod->getPatternRowsPerMeasure(curPat);
    if (rpm <= 0) rpm = 16;

    int32_t curOrder = m_state.current_order.load();

    struct Slot { int32_t pat, nrows, startAbs, orderIdx; int rpb, rpm; };
    std::vector<Slot> slots;
    int32_t totalRows = 0;
    int32_t curAbsRow = curRow;

    if (m_continuousScroll) {
        int32_t numOrders = m_state.num_orders.load();
        slots.reserve(numOrders);
        for (int32_t o = 0; o < numOrders; ++o) {
            int32_t p  = (int32_t)mod->getOrderPattern(o);
            int32_t pr = (p >= 0) ? (int32_t)mod->getPatternNumRows(p) : 0;
            if (pr <= 0) continue;
            int prpb = mod->getPatternRowsPerBeat(p);    if (prpb <= 0) prpb = 4;
            int prpm = mod->getPatternRowsPerMeasure(p); if (prpm <= 0) prpm = 16;
            if (o == curOrder) curAbsRow = totalRows + curRow;
            slots.push_back({p, pr, totalRows, o, prpb, prpm});
            totalRows += pr;
        }
    }
    if (slots.empty()) {
        slots.push_back({curPat, nrows, 0, curOrder, rpb, rpm});
        totalRows = nrows;
        curAbsRow = curRow;
    }

    // Click to move edit cursor — works in both modes.
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && m_rowH > 0.f && m_tableOriginY > 0.f) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        const float sb = ImGui::GetStyle().ScrollbarSize;
        bool inGrid = ImGui::IsMouseHoveringRect(
            {wp.x, wp.y + m_chanH}, {wp.x + ws.x - sb, wp.y + ws.y - sb}, false);
        if (inGrid) {
            float relY  = mp.y - m_tableOriginY + m_scrollY;
            float step  = m_colStep > 0.f ? m_colStep : (m_cellW + 2.f * ImGui::GetStyle().CellPadding.x);
            float relX  = mp.x - (m_tableOriginX - m_hscroll);
            int   absRow = int(relY / m_rowH);
            int   ch     = int(relX / step);
            if (ch >= 0 && ch < nch && absRow >= 0 && absRow < totalRows) {
                int32_t clickLocalR   = absRow;
                if (m_continuousScroll) {
                    for (auto& s : slots) {
                        if (absRow >= s.startAbs && absRow < s.startAbs + s.nrows) {
                            clickLocalR = absRow - s.startAbs;
                            if (s.orderIdx != curOrder && onOrderSeekRequested)
                                onOrderSeekRequested(s.orderIdx);
                            m_cur.row = clickLocalR; m_cur.channel = ch; m_cur.col = EditCol::Note;
                            setEditMode(true);
                            break;
                        }
                    }
                } else if (clickLocalR < nrows) {
                    m_cur.row = clickLocalR; m_cur.channel = ch; m_cur.col = EditCol::Note;
                    setEditMode(true);
                }
            }
        }
    }

    int totalCols = 1 + nch;
    ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;

    float rowH  = m_rowH;  // pre-computed with mono font in render()
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float kSbSize = ImGui::GetStyle().ScrollbarSize;
    float visH  = avail.y - m_chanH - kSbSize;  // exclude frozen header and horizontal scrollbar

    // Fetch channel data for the frozen header row
    const VuSnapshot& snap = m_state.vuBuf[1 - m_state.vuWriteIdx.load(std::memory_order_acquire)];
    const auto& vu          = snap.vu;
    const auto& channelWave = snap.channelWave;
    std::vector<std::string> chNames(nch);
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        for (int ch = 0; ch < nch; ch++) {
            if (ch < (int)m_state.channel_names.size() && !m_state.channel_names[ch].empty())
                chNames[ch] = std::to_string(ch + 1) + ": " + m_state.channel_names[ch];
            else
                chNames[ch] = "Channel " + std::to_string(ch + 1);
        }
    }

    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight,  IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.f, ImGui::GetStyle().CellPadding.y));
    bool tableOpen = ImGui::BeginTable("##pat", totalCols, tFlags, avail);
    if (!tableOpen) { ImGui::PopStyleVar(); ImGui::PopStyleColor(2); return; }

    ImGui::TableSetupScrollFreeze(1, 1);  // freeze col 0 + row 0 (channel headers)
    ImGui::TableSetupColumn("##row", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, m_rowNumW);
    for (int ch = 0; ch < nch; ch++) {
        char id[12]; snprintf(id, sizeof(id), "##c%d", ch);
        ImGui::TableSetupColumn(id, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, m_cellW);
    }

    m_hscroll = ImGui::GetScrollX();

    // ── Frozen channel header row ──────────────────────────────────────────────
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        uint64_t newMuteState = m_muteState;
        int32_t plugClickCh = -1;

        ImGui::TableNextRow(ImGuiTableRowFlags_None, m_chanH);
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 0, 0, 0), 0);

        ImGui::TableSetColumnIndex(0);
        {
            ImVec2      cp  = ImGui::GetCursorScreenPos();
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImGui::Dummy({m_rowNumW, m_chanH});

            if (m_rowH > 0.f && g_fontRowNum) {
                float scrollY = ImGui::GetScrollY();
                int   topAbs  = (int)(scrollY / m_rowH);
                float frac    = fmodf(scrollY, m_rowH);
                int   nFit    = std::min(4, (int)(m_chanH / m_rowH));
                // Resolve absolute row → local row + slot rpb/rpm
                struct GhostInfo { int32_t local; int rpb, rpm; int orderIdx; };
                auto ghostInfo = [&](int32_t abs) -> GhostInfo {
                    for (auto& s : slots)
                        if (abs >= s.startAbs && abs < s.startAbs + s.nrows)
                            return {abs - s.startAbs, s.rpb, s.rpm, s.orderIdx};
                    return {abs, rpb, rpm, curOrder};
                };
                cdl->PushClipRect({cp.x, cp.y}, {cp.x + m_rowNumW, cp.y + m_chanH}, true);
                for (int i = 0; i < nFit; i++) {
                    int32_t absRow   = topAbs - 1 - i;
                    if (absRow < 0) break;
                    auto    gi       = ghostInfo(absRow);
                    bool    isMeasure = (gi.local % gi.rpm == 0);
                    bool    isBeat    = (gi.local % gi.rpb == 0) && !isMeasure;
                    float   yPos     = floorf(cp.y + m_chanH - m_rowH * (i + 1) + frac);
                    bool    isActive = (gi.orderIdx == curOrder);
                    float   maxA     = (isMeasure || isBeat) ? 160.f : 70.f;
                    if (!isActive) maxA *= 0.35f;
                    uint8_t a        = (uint8_t)(maxA * float(nFit - i) / float(nFit));
                    ImVec4  base     = (isMeasure || isBeat) ? kTextNormal
                                                              : ImVec4{180/255.f, 180/255.f, 210/255.f, 1.f};
                    ImU32   col      = IM_COL32((uint8_t)(base.x*255.f), (uint8_t)(base.y*255.f),
                                               (uint8_t)(base.z*255.f), a);
                    char buf[8]; snprintf(buf, sizeof(buf), "%3d", gi.local);
                    cdl->AddText(g_fontRowNum, g_fontRowNum->LegacySize,
                                 {cp.x + m_rowNumW - 22.f - 7.f, yPos + 3.f},
                                 col, buf);
                }
                cdl->PopClipRect();
            }
        }

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableSetColumnIndex(ch + 1);
            ImVec2 p  = ImGui::GetCursorScreenPos();
            float  cw = m_cellW;
            bool muted = (m_muteState >> ch) & 1;

            // Structural colours — used for full-height column separators below
            ImU32 hiCol  = muted ? IM_COL32(45,45,60,255) : IM_COL32(68,68,115,255);
            ImU32 loCol  = muted ? IM_COL32(8, 8,14, 255) : IM_COL32(10,10, 20, 255);
            float padX   = ImGui::GetStyle().CellPadding.x;
            float hdrTop = p.y;
            float hdrBot = hdrTop + m_chanH + 4.f;

            ImGui::SetCursorScreenPos(p);
            ImGui::PushID(0x10000 + ch);
            ImGui::InvisibleButton("##mute", {cw, CHANNAME_H});
            bool namePressed = ImGui::IsItemActive();
            bool nameHov     = ImGui::IsItemHovered();

            // ── SwitchButton-style body ───────────────────────────────────
            constexpr float R = 1.5f;
            ImU32 bodyCol = namePressed ? IM_COL32(50,50,70,255)
                          : nameHov     ? IM_COL32(38,38,55,255)
                                        : IM_COL32(25,25,38,255);
            dl->AddRectFilled(p, {p.x + cw - 1, p.y + CHANNAME_H}, bodyCol,
                              R, ImDrawFlags_RoundCornersTop);

            if (!namePressed)
                dl->AddRectFilled(p, {p.x + cw - 1, p.y + CHANNAME_H * 0.5f},
                                  IM_COL32(255,255,255,14), R, ImDrawFlags_RoundCornersTop);

            float bvR = p.x + cw - 2.f;
            float bvB = p.y + CHANNAME_H - 1.f;
            if (!namePressed) {
                dl->AddLine({p.x + R,    p.y + 0.5f}, {bvR - R, p.y + 0.5f}, IM_COL32(255,255,255,55));
                dl->AddLine({p.x + 0.5f, p.y + R   }, {p.x + 0.5f,   bvB  }, IM_COL32(255,255,255,40));
                dl->AddLine({p.x,        bvB        }, {bvR,     bvB        }, IM_COL32(0,0,0,120), 2.f);
                dl->AddLine({bvR,        p.y        }, {bvR,     bvB        }, IM_COL32(0,0,0,100));
            } else {
                dl->AddLine({p.x + R,    p.y + 0.5f}, {bvR - R, p.y + 0.5f}, IM_COL32(0,0,0,100));
                dl->AddLine({p.x + 0.5f, p.y + R   }, {p.x + 0.5f,   bvB  }, IM_COL32(0,0,0,80));
                dl->AddLine({p.x,        bvB        }, {bvR,     bvB        }, IM_COL32(255,255,255,35), 2.f);
                dl->AddLine({bvR,        p.y        }, {bvR,     bvB        }, IM_COL32(255,255,255,25));
            }

            dl->AddRect(p, {p.x + cw - 1, p.y + CHANNAME_H},
                        IM_COL32(30,30,45,255), R, ImDrawFlags_RoundCornersTop, 1.f);

            // Gutter fills + full-height column separators (structural)
            dl->AddRectFilled({p.x - padX, hdrTop}, {p.x,             hdrBot}, IM_COL32(46,46,92,200));
            dl->AddRectFilled({p.x + cw,   hdrTop}, {p.x + cw + padX, hdrBot}, IM_COL32(46,46,92,200));

            // ── Channel name + LED ────────────────────────────────────────
            constexpr float LED_W     = 4.f;
            constexpr float LED_H     = 10.f;
            constexpr float LED_PAD_R = 6.f;
            float off  = namePressed ? 1.f : 0.f;
            float ledX = p.x + cw - 1 - LED_PAD_R - LED_W + off;
            float ledY = p.y + (CHANNAME_H - LED_H) * 0.5f + off;

            float  textH  = ImGui::GetTextLineHeight();
            ImVec2 ts     = ImGui::CalcTextSize(chNames[ch].c_str());
            float  availW = ledX - off - p.x;
            dl->AddText({p.x + (availW - ts.x) * 0.5f + off,
                         p.y + (CHANNAME_H - textH) * 0.5f + off},
                        kClrTextLabel, chNames[ch].c_str());

            // LED housing
            dl->AddRectFilled({ledX - 1.f, ledY - 1.f},
                              {ledX + LED_W + 1.f, ledY + LED_H + 1.f},
                              IM_COL32(0,0,0,150), 2.f);
            if (!muted) {
                if (g_glowKnobs > 0.f) {
                    auto ga = [](int a) { return (uint8_t)(a * g_glowKnobs); };
                    dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    }, nullptr);
                    constexpr float E1 = 5.f, E2 = 2.5f;
                    dl->AddRectFilled({ledX - E1, ledY - E1},
                                      {ledX + LED_W + E1, ledY + LED_H + E1},
                                      IM_COL32(100,220,160, ga(10)), R + E1);
                    dl->AddRectFilled({ledX - E2, ledY - E2},
                                      {ledX + LED_W + E2, ledY + LED_H + E2},
                                      IM_COL32(100,220,160, ga(28)), 2.f + E2);
                    dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }, nullptr);
                }
                dl->AddRectFilled({ledX, ledY}, {ledX + LED_W, ledY + LED_H}, kClrLedGreen, 1.5f);
            } else {
                dl->AddRectFilled({ledX, ledY}, {ledX + LED_W, ledY + LED_H},
                                  IM_COL32(10,30,40,255), 1.5f);
            }

            // Mute / solo click target
            {
                ImVec2 pMax = {p.x + cw, p.y + float(CHANNAME_H)};
                bool hov = ImGui::IsMouseHoveringRect(p, pMax);
                bool dbl = hov && ImGui::IsMouseDoubleClicked(0);
                bool sng = hov && ImGui::IsMouseClicked(0) && !dbl;
                uint64_t allOthersMask = ((uint64_t(1) << nch) - 1) & ~(uint64_t(1) << ch);
                if (dbl) {
                    bool wasPlaying = !((m_preClickMuteState >> ch) & 1);
                    bool wasSoloed  = wasPlaying && (m_preClickMuteState == allOthersMask);
                    if (wasSoloed || !wasPlaying)
                        newMuteState = 0;
                    else
                        newMuteState = allOthersMask;
                } else if (sng) {
                    m_preClickMuteState = m_muteState;
                    bool isSoloed = m_muteState == allOthersMask;
                    if (isSoloed)
                        newMuteState = 0;
                    else
                        newMuteState ^= (uint64_t(1) << ch);
                }
            }

            // VU
            float vuTop = p.y + CHANNAME_H;
            if (m_showVU) {
                drawStereoVU(dl, p.x + 2, vuTop, cw - 3, VU_H, vu[ch].left, vu[ch].right);
                dl->AddLine({p.x + 0.5f, vuTop}, {p.x + 0.5f, vuTop + VU_H}, IM_COL32(255,255,255,40));
            }

            // Oscilloscope waveform
            float vuBot = vuTop + (m_showVU ? VU_H : 0.f);
            if (m_showWave) {
                float wy    = vuBot;
                float wx0 = p.x + 1.f, wy0 = wy, wx1 = p.x + cw - 1.f, wy1 = wy + WAVE_H;

                // Spherical gradient background — bright top-left (CRT bulge highlight)
                dl->AddRectFilled({wx0, wy0}, {wx1, wy1}, kClrWaveGradBase);
                dl->AddRectFilledMultiColor({wx0, wy0}, {wx1, wy1},
                    kClrWaveGradTL, kClrWaveGradTR, kClrWaveGradBR, kClrWaveGradBL);
                dl->AddLine({p.x + 0.5f, wy0}, {p.x + 0.5f, wy1}, IM_COL32(255,255,255,40));

                // Black 1px grid — masks gradient down to phosphor dot array (3px dots, 1px gap)
                {
                    constexpr float kP = 3.f;
                    ImU32 blk = IM_COL32(0, 0, 0, 255);
                    for (float gy = wy0 + (kP - 1.f); gy < wy1; gy += kP)
                        dl->AddRectFilled({wx0, gy}, {wx1, gy + 1.f}, blk);
                    for (float gx = wx0 + (kP - 1.f); gx < wx1; gx += kP)
                        dl->AddRectFilled({gx, wy0}, {gx + 1.f, wy1}, blk);
                }
            }
            if (m_showWave && channelWave[ch].count > 1) {
                float wy    = vuBot;
                float wx0 = p.x + 1.f;
                float cy    = wy + WAVE_H * 0.5f;
                float halfH = WAVE_H * 0.5f - 2.f;

                // Show only a portion of the buffer so individual waves are visible
                static constexpr int kDisplaySamples = 160;
                int total = channelWave[ch].count;
                int displayN = std::min(total, kDisplaySamples);

                // Oscilloscope trigger: find first upward zero crossing in the
                // lead-in window so the waveform appears stable (no flickering)
                int start = 0;
                int searchEnd = total - displayN;
                for (int i = 1; i < searchEnd; ++i) {
                    if (channelWave[ch].samples[i - 1] <= 0.f && channelWave[ch].samples[i] > 0.f) {
                        start = i;
                        break;
                    }
                }

                // Normalize by peak, partial floor so quiet signals stay readable
                float peak = 0.f;
                for (int i = start; i < start + displayN; ++i)
                    peak = std::max(peak, std::abs(channelWave[ch].samples[i]));
                float norm = (peak > 0.0001f) ? halfH / std::max(peak, 0.15f) : 0.f;
                // Fade when channel is silent
                float chAmp = (vu[ch].left + vu[ch].right) * 0.5f;
                norm *= std::min(chAmp * 50.0f, 1.0f);

                ImU32 wavCol = muted ? IM_COL32(80, 80, 80, 120) : IM_COL32(190, 190, 200, 220);
                float xscale = (cw - 2.f) / float(displayN);
                ImVec2 wpts[kDisplaySamples];
                wpts[0] = {p.x + 1.f, cy - channelWave[ch].samples[start] * norm};
                for (int i = 1; i < displayN; ++i)
                    wpts[i] = {p.x + float(i) * xscale, cy - channelWave[ch].samples[start + i] * norm};
                dl->AddPolyline(wpts, displayN, wavCol, ImDrawFlags_None, 1.0f);
            }

            // Plugin row
            float plugTop = vuBot + (m_showWave ? WAVE_H : 0.f);
            if (m_showPluginRow) {
                ImGui::SetCursorScreenPos({p.x, plugTop});
                if (ImGui::InvisibleButton("##plug", {cw, PLUGIN_H}))
                    plugClickCh = ch;
                bool fxPressed = ImGui::IsItemActive();
                bool fxHov     = ImGui::IsItemHovered();

                constexpr float fxR = 1.5f;
                ImVec2 fxTL = {p.x,          plugTop};
                ImVec2 fxBR = {p.x + cw - 1, plugTop + PLUGIN_H};

                ImU32 fxBody = fxPressed ? IM_COL32(50,50,70,255)
                             : fxHov     ? IM_COL32(38,38,55,255)
                                         : IM_COL32(25,25,38,255);
                dl->AddRectFilled(fxTL, fxBR, fxBody, fxR, ImDrawFlags_RoundCornersBottom);

                if (!fxPressed)
                    dl->AddRectFilled(fxTL, {fxBR.x, plugTop + PLUGIN_H * 0.5f},
                                      IM_COL32(255,255,255,14), 0.f);

                float fbvR = fxBR.x - 1.f;
                float fbvB = plugTop + PLUGIN_H - 1.f;
                if (!fxPressed) {
                    dl->AddLine({fxTL.x,        plugTop + 0.5f}, {fbvR,        plugTop + 0.5f}, IM_COL32(255,255,255,55));
                    dl->AddLine({fxTL.x + 0.5f, plugTop       }, {fxTL.x+0.5f, fbvB - fxR    }, IM_COL32(255,255,255,40));
                    dl->AddLine({fxTL.x + fxR,  fbvB          }, {fbvR - fxR,  fbvB           }, IM_COL32(0,0,0,120), 2.f);
                    dl->AddLine({fbvR,           plugTop       }, {fbvR,        fbvB - fxR     }, IM_COL32(0,0,0,100));
                } else {
                    dl->AddLine({fxTL.x,        plugTop + 0.5f}, {fbvR,        plugTop + 0.5f}, IM_COL32(0,0,0,100));
                    dl->AddLine({fxTL.x + 0.5f, plugTop       }, {fxTL.x+0.5f, fbvB - fxR    }, IM_COL32(0,0,0,80));
                    dl->AddLine({fxTL.x + fxR,  fbvB          }, {fbvR - fxR,  fbvB           }, IM_COL32(255,255,255,35), 2.f);
                    dl->AddLine({fbvR,           plugTop       }, {fbvR,        fbvB - fxR     }, IM_COL32(255,255,255,25));
                }

                dl->AddRect(fxTL, fxBR, IM_COL32(30,30,45,255), fxR, ImDrawFlags_RoundCornersBottom, 1.f);

                const char* lbl = "---";
                ImVec2 ts  = ImGui::CalcTextSize(lbl);
                float  off = fxPressed ? 1.f : 0.f;
                ImU32  fxTextCol = muted ? IM_COL32(80,80,90,255) : kClrTextLabel;
                dl->AddText({p.x + (cw - ts.x) * 0.5f + off,
                             plugTop + (PLUGIN_H - ts.y) * 0.5f + off},
                            fxTextCol, lbl);
            }

            // Fill the CellPadding.y gap between channel header bottom and first pattern row
            {
                float padY   = ImGui::GetStyle().CellPadding.y;
                float gapTop = p.y + m_chanH;
                float gapBot = gapTop + padY + 1.f;
                dl->AddRectFilled({p.x, gapTop}, {p.x + cw - 1, gapBot}, IM_COL32(20, 20, 33, 255));
            }
            ImGui::PopID();

            // Claim the full header height so the row is tall enough
            ImGui::SetCursorScreenPos({p.x, p.y + m_chanH});
            ImGui::Dummy({0.f, 0.f});
        }

        if (newMuteState != m_muteState) {
            m_muteState = newMuteState;
            m_state.channel_mute_mask.store(newMuteState);
        }
        if (plugClickCh >= 0)
            m_pluginPopupCh = plugClickCh;
    }

    float currentScrollY = ImGui::GetScrollY();

    // Option+Scroll — seek play cursor row by row (consumes wheel before free-scroll check).
    const float dt = ImGui::GetIO().DeltaTime;
    if ((m_seekMode != ImGui::GetIO().KeyAlt) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        const float wy = ImGui::GetIO().MouseWheel;
        if (std::abs(wy) > 0.f && onRowSeekRequested && m_ctrl) {
            m_wheelAcc += wy;
            int steps = (int)m_wheelAcc;
            m_wheelAcc -= (float)steps;
            if (steps != 0) {
                int32_t nOrds  = m_state.num_orders.load();
                int32_t newOrd = m_state.current_order.load();
                int32_t newRow = m_state.current_row.load() - steps; // up = wy>0 = earlier row
                // navigate across pattern boundaries
                while (newRow < 0 && newOrd > 0) {
                    --newOrd;
                    int prevPat  = m_ctrl->module()->getOrderPattern(newOrd);
                    int prevRows = pe_num_rows(m_ctrl->module(), prevPat);
                    newRow += prevRows;
                }
                newRow = std::max(newRow, 0);
                int curPatRows = pe_num_rows(m_ctrl->module(), curPat);
                while (newRow >= curPatRows && newOrd < nOrds - 1) {
                    newRow -= curPatRows;
                    ++newOrd;
                    int nextPat  = m_ctrl->module()->getOrderPattern(newOrd);
                    curPatRows   = pe_num_rows(m_ctrl->module(), nextPat);
                }
                newRow = std::min(newRow, curPatRows - 1);
                onRowSeekRequested(newOrd, newRow);
            }
            ImGui::GetIO().MouseWheel = 0.f;
        }
    } else {
        m_wheelAcc = 0.f;
    }

    // Manual vertical scroll while hovered → suppress follow for 1 second.
    // Ignore when horizontal component dominates (trackpad horizontal swipe bleed).
    {
        const float wy = ImGui::GetIO().MouseWheel;
        const float wx = ImGui::GetIO().MouseWheelH;
        if (std::abs(wy) > std::abs(wx) &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            m_scrollSuppressTimer = 1.0f;
        }
    }
    if (m_scrollSuppressTimer > 0.f)
        m_scrollSuppressTimer = std::max(0.f, m_scrollSuppressTimer - dt);

    bool  playing    = m_state.is_playing.load();
    bool  patChanged = (curPat != m_lastPattern);
    bool  rowRewound = (playing && curAbsRow < m_lastAbsRow - 2);
    if (curOrder != m_lastCurOrder)
        m_glowLastPos.clear();
    bool  navSnap    = m_navSnapPending; m_navSnapPending = false;
    // In continuous mode pattern boundaries are seamless — only snap on actual
    // backward jumps (loop restart / seek), not on natural forward progression.
    bool  needsSnap  = m_continuousScroll ? (rowRewound || navSnap) : (patChanged || rowRewound || navSnap);

    bool  forceScroll  = false;
    float newScrollY   = currentScrollY;
    bool  forceScrollX = false;
    float newScrollX   = m_hscroll;

    if (m_editMode) {
        int32_t editAbs = 0;
        for (auto& s : slots)
            if (s.orderIdx == curOrder) { editAbs = s.startAbs + m_cur.row; break; }
        if (m_editCursorMoved) {
            int32_t rowDelta = editAbs - m_lastEditAbsRow;
            if (rowDelta != 0) {
                const float kZone     = 3.f * rowH;
                float       cursorTop = float(editAbs) * rowH;
                float       cursorBot = cursorTop + rowH;
                bool nearBottom = cursorBot  > currentScrollY + visH - kZone;
                bool nearTop    = cursorTop  < currentScrollY + kZone;
                if ((rowDelta > 0 && nearBottom) || (rowDelta < 0 && nearTop)) {
                    newScrollY  = currentScrollY + float(rowDelta) * rowH;
                    forceScroll = true;
                }
            }
            int32_t chDelta = m_cur.channel - m_lastEditChannel;
            if (chDelta != 0 && m_colStep > 0.f) {
                float visW      = avail.x - m_rowNumW - kSbSize;
                float kZoneX    = 2.f * m_colStep;
                float cursorL   = float(m_cur.channel) * m_colStep;
                float cursorR   = cursorL + m_colStep;
                bool nearRight  = cursorR > m_hscroll + visW - kZoneX;
                bool nearLeft   = cursorL < m_hscroll + kZoneX;
                if ((chDelta > 0 && nearRight) || (chDelta < 0 && nearLeft)) {
                    newScrollX   = m_hscroll + float(chDelta) * m_colStep;
                    forceScrollX = true;
                }
            }
        }
        m_lastEditAbsRow  = editAbs;
        m_lastEditChannel = m_cur.channel;
    } else if (needsSnap) {
        float padY  = ImGui::GetStyle().CellPadding.y;
        newScrollY  = float(curAbsRow) * rowH - visH * 0.5f + rowH * 0.5f + padY;
        m_visualRow = float(curAbsRow);
        forceScroll = true;
    } else if (playing && m_followSong && m_scrollSuppressTimer == 0.f) {
        float padY      = ImGui::GetStyle().CellPadding.y;
        float target    = float(curAbsRow) * rowH - visH * 0.5f + rowH * 0.5f + padY;
        float maxScroll = ImGui::GetScrollMaxY();
        if (target > 0.f) {
            // Clamp to max: beyond last center-lockable row the scroll stays put and
            // the cursor walks down naturally (symmetric with walk-from-top).
            newScrollY  = std::min(target, maxScroll);
            forceScroll = true;
        }
        // target <= 0: walk from top — don't touch scroll
    }

    if (forceScroll) {
        newScrollY = roundf(newScrollY / rowH) * rowH;
        newScrollY = std::max(0.f, std::min(newScrollY, float(totalRows) * rowH));
        ImGui::SetScrollY(newScrollY);
        m_scrollY = newScrollY;
    } else if (m_rowH > 0.f) {
        float snapped = roundf(currentScrollY / m_rowH) * m_rowH;
        snapped = std::max(0.f, std::min(snapped, float(totalRows) * rowH));
        if (fabsf(snapped - currentScrollY) > 0.5f) {
            ImGui::SetScrollY(snapped);
            m_scrollY = snapped;
        } else {
            m_scrollY = currentScrollY;
        }
    }
    if (forceScrollX) {
        float maxScrollX = std::max(0.f, float(nch) * (m_colStep > 0.f ? m_colStep : m_cellW) - (avail.x - m_rowNumW - kSbSize));
        newScrollX = std::max(0.f, std::min(newScrollX, maxScrollX));
        ImGui::SetScrollX(newScrollX);
        m_hscroll = newScrollX;
    }

    m_lastPattern  = curPat;
    m_lastRow      = curRow;
    m_lastAbsRow   = curAbsRow;
    m_lastCurOrder = curOrder;

    if (m_fontMono) ImGui::PushFont(m_fontMono);

    // Pre-compute monospace field pixel widths (fixed per font size, avoids per-cell CalcTextSize)
    float fw[5] = {};
    {
        float monoCW = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "X").x;
        fw[0] = monoCW * 3;  // note  (3 chars)
        fw[1] = monoCW * 2;  // inst  (2 chars)
        fw[2] = monoCW * 3;  // vol   (3 chars)
        fw[3] = monoCW;      // fx    (1 char)
        fw[4] = monoCW * 2;  // fxp   (2 chars)
    }

    // Animate per-slot dim factors (1.0 = active, 0.5 = fully dim, 900ms linear fade)
    {
        const float dt       = ImGui::GetIO().DeltaTime;
        const float fadeStep = 0.5f * std::min(dt / 0.3f, 1.0f);
        for (auto& s : slots) {
            if (s.orderIdx == curOrder) {
                m_slotDim[s.orderIdx] = 1.0f;
            } else {
                auto [it, inserted] = m_slotDim.emplace(s.orderIdx, 0.5f);
                if (!inserted)
                    it->second = std::max(it->second - fadeStep, 0.5f);
            }
        }
    }

    // For edit cursor: absolute row within the continuous scroll space
    int32_t curOrderSlotStart = 0;
    for (auto& s : slots)
        if (s.orderIdx == curOrder) { curOrderSlotStart = s.startAbs; break; }
    int32_t editAbsRow = curOrderSlotStart + m_cur.row;

    // In center-lock phase: rederive highlight from currentScrollY so it stays in sync
    // with what's actually rendered (SetScrollY lags 1 frame). Skip during walk-from-top
    // (forceScroll false) and walk-from-bottom (target > maxScroll) — both let the real
    // curAbsRow from audio drive the highlight while scroll is pinned at 0 / maxScroll.
    if (playing && m_followSong && m_scrollSuppressTimer == 0.f && forceScroll) {
        float padY      = ImGui::GetStyle().CellPadding.y;
        float target    = float(curAbsRow) * rowH - visH * 0.5f + rowH * 0.5f + padY;
        float maxScroll = ImGui::GetScrollMaxY();
        if (target <= maxScroll) {
            int32_t h = (int32_t)roundf((currentScrollY + visH * 0.5f - rowH * 0.5f - padY) / rowH);
            curAbsRow = std::clamp(h, 0, totalRows - 1);
        }
        // else walk-from-bottom: use real curAbsRow from audio
    }

    ImGuiListClipper clipper;
    clipper.Begin(totalRows);
    int slotIdx = 0;
    while (clipper.Step()) {
        // Walk slot pointer to the first slot covering DisplayStart
        while (slotIdx + 1 < (int)slots.size() && slots[slotIdx + 1].startAbs <= clipper.DisplayStart)
            ++slotIdx;

    for (int absR = clipper.DisplayStart; absR < clipper.DisplayEnd; absR++) {
        // Advance slot pointer as needed
        while (slotIdx + 1 < (int)slots.size() && slots[slotIdx + 1].startAbs <= absR)
            ++slotIdx;

        const Slot&   slot     = slots[slotIdx];
        const int32_t localR   = absR - slot.startAbs;
        const bool    isActive = (slot.orderIdx == curOrder);
        auto          dimIt    = m_slotDim.find(slot.orderIdx);
        const float   dimFactor = dimIt != m_slotDim.end() ? dimIt->second : (isActive ? 1.0f : 0.5f);

        bool isCur     = isActive && (slot.startAbs + localR) == curAbsRow;
        bool isMeasure = (localR % slot.rpm == 0);
        bool isBeat    = (localR % slot.rpb == 0) && !isMeasure;

        ImGui::TableNextRow();

        ImU32 rowBg = isCur     ? kClrPatCurRowBg
                    : isMeasure ? ImGui::ColorConvertFloat4ToU32(kBgMeasure)
                    : isBeat    ? ImGui::ColorConvertFloat4ToU32(kBgHilight)
                    :             kClrPatRowBg;

        ImGui::TableNextColumn();
        {
            char rnBuf[8]; snprintf(rnBuf, sizeof(rnBuf), "%3d", localR);
            if (g_fontRowNum) ImGui::PushFont(g_fontRowNum);
            ImVec2 rnPos = ImGui::GetCursorScreenPos();
            float  rnW   = ImGui::CalcTextSize(rnBuf).x;
            rnPos.x = floorf(rnPos.x + m_rowNumW - rnW - 2.f);
            rnPos.y = floorf(rnPos.y) - 1.f;

            // Pattern boundary separator line
            if (m_continuousScroll && localR == 0 && absR > 0) {
                float winLeft  = ImGui::GetWindowPos().x + m_rowNumW;
                float winRight = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x
                                 - ImGui::GetStyle().ScrollbarSize;
                float step     = m_colStep > 0.f ? m_colStep : m_cellW;
                float lineRight = std::min(winRight, winLeft + nch * step);
                ImGui::GetWindowDrawList()->AddLine(
                    {winLeft, rnPos.y - 2.f}, {lineRight, rnPos.y - 2.f},
                    IM_COL32(70, 70, 130, 220));
            }

            ImVec4 rnColV = isBeat||isMeasure ? kTextNormal : kDim;
            ImU32 rnCol = ImGui::ColorConvertFloat4ToU32(
                {rnColV.x * dimFactor, rnColV.y * dimFactor, rnColV.z * dimFactor, rnColV.w});
            ImGui::GetWindowDrawList()->AddText(rnPos, rnCol, rnBuf);
            float lineH = ImGui::GetTextLineHeight();
            if (g_fontRowNum) ImGui::PopFont();
            ImGui::Dummy({m_rowNumW, lineH});
        }

        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableNextColumn();

            // Measure actual cell origins — must happen before culling so click detection works
            if (absR == clipper.DisplayStart) {
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ch == 0) {
                    m_tableOriginX = cp.x + m_hscroll;
                    m_tableOriginY = cp.y - float(absR) * m_rowH + m_scrollY
                                   - ImGui::GetStyle().CellPadding.y;
                } else if (ch == 1) {
                    m_colStep = (cp.x + m_hscroll) - m_tableOriginX;
                }
            }

            // Row background — drawn per channel cell so it excludes the row-number
            // column and any empty space to the right of the last channel.
            {
                ImVec2 cp   = ImGui::GetCursorScreenPos();
                float  padY = ImGui::GetStyle().CellPadding.y;
                ImVec2 rMin = {cp.x, cp.y - padY};
                ImVec2 rMax = {cp.x + m_cellW, cp.y - padY + m_rowH};
                ImDrawList* wdl = ImGui::GetWindowDrawList();
                wdl->AddRectFilled(rMin, rMax, rowBg);
                if (isCur) {
                    wdl->AddRectFilled(rMin, {rMax.x, rMin.y + m_rowH * 0.5f}, IM_COL32(255,255,255,18));
                    wdl->AddLine({rMin.x, rMin.y + 0.5f}, {rMax.x, rMin.y + 0.5f}, IM_COL32(255,255,255,70));
                    wdl->AddLine({rMin.x, rMax.y - 0.5f}, {rMax.x, rMax.y - 0.5f}, IM_COL32(0,0,0,110));
                }
            }

            // Channel divider lines
            {
                ImVec2 cp    = ImGui::GetCursorScreenPos();
                float  padX  = ImGui::GetStyle().CellPadding.x;
                float  padY  = ImGui::GetStyle().CellPadding.y;
                float  rowH  = ImGui::GetTextLineHeight() + 2.f * padY;
                float  top   = cp.y - padY;
                bool   chMut = (m_muteState >> ch) & 1;
                ImU32  hiCol = chMut ? IM_COL32(45,45,60,255) : IM_COL32(68,68,115,255);
                ImU32  loCol = chMut ? IM_COL32(8, 8,14, 255) : IM_COL32(10,10, 20, 255);
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                sdl->AddRectFilled({cp.x - padX, top}, {cp.x,               top + rowH}, IM_COL32(46, 46, 92, 200));
                sdl->AddRectFilled({cp.x + m_cellW, top}, {cp.x + m_cellW + padX, top + rowH}, IM_COL32(46, 46, 92, 200));
                sdl->AddLine({cp.x + 0.5f,               top - 0.5f}, {cp.x + 0.5f,               top + rowH}, IM_COL32(255,255,255,40));
                sdl->AddLine({cp.x + m_cellW - 1, top - 1.f}, {cp.x + m_cellW - 1, top + rowH}, loCol);
            }

            bool isEditCell = m_editMode && (absR == editAbsRow) && (ch == m_cur.channel);

            // Selection highlight — only within active pattern
            bool isSelected = isActive && m_sel.active
                && localR >= m_sel.rowMin() && localR <= m_sel.rowMax()
                && ch >= m_sel.chMin() && ch <= m_sel.chMax();
            if (isSelected) {
                ImVec2 rMin = ImGui::GetCursorScreenPos();
                ImVec2 rMax = {rMin.x + m_cellW, rMin.y + m_rowH};
                ImGui::GetWindowDrawList()->AddRectFilled(
                    rMin, rMax, IM_COL32(80, 80, 200, 60));
            }

            const bool showInst = m_detailView <= 2;
            const bool showVol  = m_detailView <= 1;
            const bool showFx   = m_detailView == 0;

            float subX[5] = {}, subW[5] = { fw[0], fw[1], fw[2], fw[3], fw[4] };
            float cellY = 0.f;

            std::string note  = mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_NOTE);
            std::string inst  = showInst ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_INSTRUMENT) : "";
            std::string fx    = showFx   ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_EFFECT)     : "";
            std::string fxp   = showFx   ? mod->formatCellCommand(slot.pat, localR, ch, SoundModule::COMMAND_PARAMETER)  : "";

            // libopenmpt formats vol as hex; read raw cell and format as decimal to match OpenMPT
            PatternCell rawCell{};
            if (showVol) pe_get_cell(mod, slot.pat, localR, ch, rawCell);
            bool hasVol = showVol && rawCell.volcmd != VOLCMD_NONE;
            char volBuf[8] = {};
            if (hasVol) {
                static constexpr char kVolCmdLetters[] = " vpcdabuhlrgfe:o";
                char letter = (rawCell.volcmd < 16) ? kVolCmdLetters[rawCell.volcmd] : '?';
                snprintf(volBuf, sizeof(volBuf), "%c%02d", letter, (int)rawCell.vol);
            }
            std::string vol = hasVol ? volBuf : "";  // kept for glow

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = showInst && !inst.empty() && inst != ".." && inst != "00";
            bool hasFx   = showFx && !fx.empty() && fx != ".";

            float cellDim = isActive ? ((m_muteState >> ch) & 1 ? 0.5f : 1.0f) : dimFactor;
            auto dimColor = [cellDim](const ImVec4& c) -> ImVec4 {
                return {c.x * cellDim, c.y * cellDim, c.z * cellDim, c.w};
            };

            // Direct draw — bypasses per-cell vsnprintf, CalcTextSize, and ItemAdd/hover overhead
            {
                ImDrawList* cdl  = ImGui::GetWindowDrawList();
                ImVec2      cpos = ImGui::GetCursorScreenPos();
                cellY            = cpos.y;
                float cx         = cpos.x + 4.f;

                auto dimU32 = [&](const ImVec4& c) -> ImU32 {
                    return IM_COL32((uint8_t)(c.x * cellDim * 255.f + 0.5f),
                                    (uint8_t)(c.y * cellDim * 255.f + 0.5f),
                                    (uint8_t)(c.z * cellDim * 255.f + 0.5f),
                                    (uint8_t)(c.w * 255.f + 0.5f));
                };

                subX[0] = cx + 0.5f;
                cdl->AddText({cx, cpos.y}, dimU32(hasNote ? kColNote : kDim), hasNote ? note.c_str() : "...");
                cx += fw[0];
                if (showInst) {
                    cx += 6.f; subX[1] = cx + 0.5f;
                    cdl->AddText({cx, cpos.y}, dimU32(hasInst ? kColInstr : kDim), hasInst ? inst.c_str() : "..");
                    cx += fw[1];
                }
                if (showVol) {
                    cx += 6.f; subX[2] = cx + 0.5f;
                    cdl->AddText({cx, cpos.y}, dimU32(hasVol ? kColVolume : kDim), hasVol ? volBuf : "...");
                    cx += fw[2];
                }
                if (showFx) {
                    cx += 6.f; subX[3] = cx + 0.5f;
                    cdl->AddText({cx, cpos.y}, dimU32(hasFx ? kColGlobal : kDim), hasFx ? fx.c_str() : ".");
                    cx += fw[3]; subX[4] = cx + 0.5f;
                    cdl->AddText({cx, cpos.y}, dimU32(hasFx ? kColGlobal : kDim), hasFx ? fxp.c_str() : "..");
                }
                ImGui::Dummy({m_cellW - 8.f, ImGui::GetTextLineHeight()});
            }

            if (isEditCell) {
                int lastCol = showFx ? 4 : (showVol ? 2 : (showInst ? 1 : 0));
                int col = std::min(int(m_cur.col), lastCol);
                float y0 = ImGui::GetItemRectMin().y;
                float y1 = ImGui::GetItemRectMax().y;
                ImU32 hilite = IM_COL32(255, 220, 80, 80);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {subX[col]-1, y0}, {subX[col]+subW[col], y1+1}, hilite);
                float x0cell = subX[0] - 1;
                float x1cell = subX[lastCol] + subW[lastCol] + 1;
                ImGui::GetWindowDrawList()->AddRect(
                    {x0cell, y0-1}, {x1cell, y1+1}, IM_COL32(255,255,255,220));
            }

            // Glow on non-empty cells — active pattern, unmuted channels only
            bool chMuted = (m_muteState >> ch) & 1;
            if (isActive && !chMuted && g_glowPattern > 0.f && (hasNote || hasInst || hasVol || hasFx)) {
                ImDrawList* gdl  = ImGui::GetWindowDrawList();
                ImFont*     font = ImGui::GetFont();
                float       fsz  = ImGui::GetFontSize();
                gdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                }, nullptr);
                auto addGlowText = [&](float gx, const ImVec4& c, const char* txt, int subCol) {
                    // Phosphor: track last screen pos; spawn ghost if it moved
                    int64_t pkey = (int64_t(absR) << 20) | (int64_t(ch) << 4) | int64_t(subCol);
                    ImVec2  cur  = {gx, cellY};
                    auto    pit  = m_glowLastPos.find(pkey);
                    if (pit != m_glowLastPos.end() && fabsf(pit->second.y - cellY) > 0.5f) {
                        ImU32 bcol = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), 0);
                        PhosphorGhost g;
                        g.x       = pit->second.x;
                        g.y       = pit->second.y;
                        g.font    = font;
                        g.fsize   = fsz;
                        g.baseCol = bcol;
                        g.alpha   = 1.0f;
                        strncpy(g.text, txt, 7); g.text[7] = '\0';
                        m_phosphorGhosts.push_back(g);
                        // Top-row pre-exit ghost: also stamp current position so it
                        // fades in place after the row scrolls off the top
                        if (absR == clipper.DisplayStart) {
                            PhosphorGhost ge = g;
                            ge.x = gx; ge.y = cellY;
                            m_phosphorGhosts.push_back(ge);
                        }
                    }
                    m_glowLastPos[pkey] = cur;

                    uint8_t a1 = (uint8_t)((isCur ? 124 : 24) * g_glowPattern); // wide halo (large blur radius)
                    uint8_t a2 = (uint8_t)((isCur ? 30 :  8) * g_glowPattern); // tight core glow (bright center)
                    uint8_t a3 = (uint8_t)((isCur ? 54 :  2) * g_glowPattern); // outermost soft bloom

                    ImU32 gc1 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a1);
                    ImU32 gc2 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a2);
                    ImU32 gc3 = IM_COL32((uint8_t)(c.x*255.f),(uint8_t)(c.y*255.f),(uint8_t)(c.z*255.f), a3);
                    gdl->AddText(font, fsz, {gx-1, cellY-1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx+1, cellY-1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx-1, cellY+1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx+1, cellY+1}, gc1, txt);
                    gdl->AddText(font, fsz, {gx-2, cellY-2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx+2, cellY-2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx-2, cellY+2}, gc2, txt);
                    gdl->AddText(font, fsz, {gx+2, cellY+2}, gc2, txt);
                    if (a3) {
                        gdl->AddText(font, fsz, {gx-3, cellY-3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx+3, cellY-3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx-3, cellY+3}, gc3, txt);
                        gdl->AddText(font, fsz, {gx+3, cellY+3}, gc3, txt);
                    }
                };
                if (hasNote)              addGlowText(subX[0], kColNote,   note.c_str(), 0);
                if (hasInst)              addGlowText(subX[1], kColInstr,  inst.c_str(), 1);
                if (hasVol)               addGlowText(subX[2], kColVolume, vol.c_str(),  2);
                if (hasFx) {
                    addGlowText(subX[3], kColGlobal, fx.c_str(),  3);
                    addGlowText(subX[4], kColGlobal, fxp.c_str(), 4);
                }
                gdl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }, nullptr);
            }
        }
    }
    }
    clipper.End();
    if (m_fontMono) ImGui::PopFont();
    ImGui::EndTable();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

}

