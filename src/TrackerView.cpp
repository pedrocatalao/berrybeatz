#include "TrackerView.h"
#include "PatternEditor.h"
#include <libopenmpt/libopenmpt_ext.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDateTime>

// ── Note / key helpers ────────────────────────────────────────────────────────

// FT2-style piano layout → semitone offset (0-11 = base octave, 12-23 = base+1).
// Returns -1 if the key is not a note key.
static int noteFromQtKey(int k) {
    switch (k) {
    // Lower row → base octave
    case Qt::Key_Z: return  0; // C
    case Qt::Key_S: return  1; // C#
    case Qt::Key_X: return  2; // D
    case Qt::Key_D: return  3; // D#
    case Qt::Key_C: return  4; // E
    case Qt::Key_V: return  5; // F
    case Qt::Key_G: return  6; // F#
    case Qt::Key_B: return  7; // G
    case Qt::Key_H: return  8; // G#
    case Qt::Key_N: return  9; // A
    case Qt::Key_J: return 10; // A#
    case Qt::Key_M: return 11; // B
    // Upper row → base+1 octave
    case Qt::Key_Q: return 12; // C
    case Qt::Key_2: return 13; // C#
    case Qt::Key_W: return 14; // D
    case Qt::Key_3: return 15; // D#
    case Qt::Key_E: return 16; // E
    case Qt::Key_R: return 17; // F
    case Qt::Key_5: return 18; // F#
    case Qt::Key_T: return 19; // G
    case Qt::Key_6: return 20; // G#
    case Qt::Key_Y: return 21; // A
    case Qt::Key_7: return 22; // A#
    case Qt::Key_U: return 23; // B
    default: return -1;
    }
}

// Returns 0-15 for hex digit characters, -1 otherwise.
static int hexDigitFromKey(int k) {
    if (k >= Qt::Key_0 && k <= Qt::Key_9) return k - Qt::Key_0;
    if (k >= Qt::Key_A && k <= Qt::Key_F) return k - Qt::Key_A + 10;
    return -1;
}

// ── OpenMPT "Blue (FT2)" colour scheme ────────────────────────────────────────
// Exact values from mptrack/ColorSchemes.h
static const ImVec4 kBgNormal    {  0/255.f,   0/255.f,   0/255.f, 1.f};
static const ImVec4 kTextNormal  {224/255.f, 224/255.f,  64/255.f, 1.f}; // #E0E040
static const ImVec4 kBgCurRow    {112/255.f, 112/255.f, 112/255.f, 1.f}; // #707070
static const ImVec4 kTextCurRow  {240/255.f, 240/255.f,  80/255.f, 1.f}; // #F0F050
static const ImVec4 kBgHilight   { 64/255.f,  64/255.f, 128/255.f, 1.f}; // #404080 beat
static const ImVec4 kBgMeasure   { 46/255.f,  46/255.f,  90/255.f, 1.f}; // #2E2E5A measure
static const ImVec4 kColNote     {224/255.f, 224/255.f,  64/255.f, 1.f}; // #E0E040
static const ImVec4 kColInstr    {255/255.f, 255/255.f,   0/255.f, 1.f}; // #FFFF00
static const ImVec4 kColVolume   {  0/255.f, 255/255.f,   0/255.f, 1.f}; // #00FF00
static const ImVec4 kColPanning  {  0/255.f, 255/255.f, 255/255.f, 1.f}; // #00FFFF
static const ImVec4 kColPitch    {255/255.f, 255/255.f,   0/255.f, 1.f}; // #FFFF00
static const ImVec4 kColGlobal   {255/255.f,  64/255.f,  64/255.f, 1.f}; // #FF4040
static const ImVec4 kDim         { 80/255.f,  80/255.f,  80/255.f, 1.f};
static const ImVec4 kChHeader    { 30/255.f,  30/255.f,  60/255.f, 1.f};
static const ImVec4 kChBorder    { 46/255.f,  46/255.f,  92/255.f, 1.f}; // #2E2E5C sep
static const ImVec4 kOrderBg     {  0/255.f,   0/255.f,   0/255.f, 0.f}; // transparent
static const ImVec4 kOrderCur    { 64/255.f,  64/255.f, 160/255.f, 1.f}; // #4040A0

// VU LED colours (exact from ColorSchemes.h)
static const ImU32 kVuLoLit    = IM_COL32(  0, 200,   0, 255); // #00C800
static const ImU32 kVuLoUnlit  = IM_COL32(  0,  28,   0, 255);
static const ImU32 kVuMedLit   = IM_COL32(255, 200,   0, 255); // #FFC800
static const ImU32 kVuMedUnlit = IM_COL32( 40,  30,   0, 255);
static const ImU32 kVuHiLit    = IM_COL32(225,   0,   0, 255); // #E10000
static const ImU32 kVuHiUnlit  = IM_COL32( 35,   0,   0, 255);

// ── Layout constants (match OpenMPT Draw_pat.cpp) ─────────────────────────────
static constexpr float PTBAR1_H  = 32.0f;
static constexpr float PTBAR2_H  = 30.0f;
static constexpr float ORDER_H   = 26.0f;
static constexpr float CHANNAME_H= 18.0f;
static constexpr float VU_H      = 13.0f;
static constexpr float PLUGIN_H  = 14.0f;
static constexpr float CHAN_H    = CHANNAME_H + VU_H + PLUGIN_H;
static constexpr float PAT_TOP   = PTBAR1_H + PTBAR2_H + ORDER_H + CHAN_H;

// ── TrackerView ───────────────────────────────────────────────────────────────

TrackerView::TrackerView(ModuleState& state, QWidget* parent)
    : QOpenGLWidget(parent), m_state(state) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // needed so ImGui knows hover position before first click
    connect(&m_timer, &QTimer::timeout, this, [this]{ update(); });
    m_timer.start(16);
}

TrackerView::~TrackerView() {
    makeCurrent();
    ImGui_ImplOpenGL3_Shutdown();
    if (m_imgui) ImGui::DestroyContext(m_imgui);
    doneCurrent();
}

void TrackerView::initializeGL() {
    m_imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imgui);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    float dpr      = float(devicePixelRatio());
    float fontSize = 13.0f;
    ImFontConfig cfg;
    cfg.OversampleH = 2; cfg.OversampleV = 2;
    if (!io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Menlo.ttc", fontSize * dpr, &cfg))
        io.Fonts->AddFontDefault();
    io.FontGlobalScale = 1.0f / dpr;

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.HoverDelayNormal = 3.0f;
    s.WindowRounding = s.FrameRounding = s.ScrollbarRounding = 0.0f;
    s.WindowBorderSize = s.ChildBorderSize = 0.0f;
    s.WindowPadding = {4, 2};
    s.CellPadding   = {3, 1};
    s.ItemSpacing   = {2, 1};

    ImGui_ImplOpenGL3_Init("#version 150");
}

void TrackerView::resizeGL(int w, int h) {
    ImGui::SetCurrentContext(m_imgui);
    float dpr = float(devicePixelRatio());
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize             = ImVec2(float(w), float(h));
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);
}

void TrackerView::paintGL() {
    ImGui::SetCurrentContext(m_imgui);

    double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    float  dt  = float(now - m_lastTime);
    if (dt > 0.1f) dt = 0.1f;
    m_lastTime = now;

    float dpr = float(devicePixelRatio());
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize             = ImVec2(float(width()), float(height()));
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);
    io.DeltaTime               = (dt > 0.0f) ? dt : 1.0f/60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    renderUI();

    ImGui::Render();
    glViewport(0, 0, int(width()*dpr), int(height()*dpr));
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ── Main layout ───────────────────────────────────────────────────────────────

void TrackerView::renderUI() {
    const float W = float(width());
    const float H = float(height());

    // Cache font metrics for column alignment
    m_charW   = ImGui::CalcTextSize("A").x;
    m_rowNumW = m_charW * 5.0f;
    m_cellW   = m_charW * 16.0f;

    // Dynamic channel-strip height based on visibility toggles
    m_chanH  = CHANNAME_H
             + (m_showVU        ? VU_H     : 0.f)
             + (m_showPluginRow ? PLUGIN_H : 0.f);
    m_patTop = PTBAR1_H + PTBAR2_H + ORDER_H + m_chanH;

    ImGuiWindowFlags noDecoFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    const ImVec4 kBar1Bg{10/255.f, 10/255.f, 20/255.f, 1.f};  // #0a0a14 — matches Qt toolbar
    const ImVec4 kBar2Bg{10/255.f, 10/255.f, 20/255.f, 1.f};

    // ── Pattern toolbar row 1 ────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, 0});
    ImGui::SetNextWindowSize({W, PTBAR1_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBar1Bg);
    ImGui::Begin("##ptbar1", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar1();
    ImGui::End();

    // ── Pattern toolbar row 2 ────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, PTBAR1_H});
    ImGui::SetNextWindowSize({W, PTBAR2_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBar2Bg);
    ImGui::Begin("##ptbar2", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawPatternToolbar2();
    ImGui::End();

    // ── Order list ──────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, PTBAR1_H + PTBAR2_H});
    ImGui::SetNextWindowSize({W, ORDER_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kOrderBg);
    ImGui::Begin("##orders", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawOrderList();
    ImGui::End();

    // ── Channel strip ────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, PTBAR1_H + PTBAR2_H + ORDER_H});
    ImGui::SetNextWindowSize({W, m_chanH});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kChHeader);
    ImGui::Begin("##chanstrip", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawChannelStrip();
    ImGui::End();

    // ── Pattern panel ────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, m_patTop});
    ImGui::SetNextWindowSize({W, H - m_patTop});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBgNormal);
    ImGui::Begin("##pattern", nullptr, noDecoFlags);
    ImGui::PopStyleColor();
    drawPatternPanel();
    ImGui::End();
}

// ── Pattern toolbar helpers ───────────────────────────────────────────────────

static void tip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
        ImGui::SetTooltip("%s", text);
}

static void togBtn(const char* lbl, bool& state, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    bool wasActive = state;
    if (wasActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(64, 64, 160, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 190, 255));
    }
    if (ImGui::Button(lbl)) state = !state;
    if (wasActive) ImGui::PopStyleColor(2);
    if (!enabled) ImGui::EndDisabled();
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

// ── Pattern toolbar row 1 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar1() {
    bool has = m_mod != nullptr;

    // Qt toolbar color scheme
    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32( 20,  20,  40, 255)); // #141428
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32( 46,  46,  90, 255)); // #2e2e5a
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32( 64,  64, 160, 255)); // #4040a0
    ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(224, 224,  64, 255)); // #e0e040
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   IM_COL32( 64,  64,  64, 255)); // #404040
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32( 46,  46,  90, 255)); // #2e2e5a
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32( 13,  13,  30, 255)); // #0d0d1e
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32( 26,  26,  46, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      IM_COL32(224, 224,  64, 255));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   {5.0f, 4.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    {3.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,  2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetCursorPosY(5.0f);

    auto actBtn = [&](const char* lbl, bool enabled = true) -> bool {
        if (!enabled) ImGui::BeginDisabled();
        bool r = ImGui::Button(lbl);
        if (!enabled) ImGui::EndDisabled();
        return r;
    };

    // Transport
    if (actBtn("+##ins", has))        { /* TODO: insert pattern */ }
    tip("Insert pattern");
    ImGui::SameLine();
    if (actBtn("▶|##playcur", has)) {
        int32_t row = m_editMode ? m_cur.row : m_state.current_row.load();
        emit patternPlayFromCursor(m_state.current_order.load(), row, m_loopPattern);
    }
    tip("Play from cursor (stops at pattern end, loops if Loop is on)");
    ImGui::SameLine();
    if (actBtn("|▶##playstart", has)) emit patternPlayFromStart(
                                          m_state.current_order.load(), m_loopPattern);
    tip("Play pattern from start (loops if Loop is on)");
    ImGui::SameLine();
    if (actBtn("■##stop2", has))      emit stopRequested();
    tip("Stop playback");
    ImGui::SameLine();
    if (actBtn("→##playrow", has)) {
        int32_t ord = m_state.current_order.load();
        int32_t row = m_cur.row;
        advanceCursor();
        emit patternPlayRow(ord, row);
    }
    tip("Play current row and advance cursor");
    ImGui::SameLine();
    // Record button — red when active
    if (m_editMode) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(160, 30, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(190, 50, 50, 255));
    }
    if (actBtn("●##rec", has)) m_editMode = !m_editMode;
    if (m_editMode) ImGui::PopStyleColor(2);
    tip("Toggle record mode (Enter)");

    tbSep();

    // View toggles
    togBtn("VU##vu",  m_showVU);
    tip("Show/hide channel VU meters");
    ImGui::SameLine();
    togBtn("FX##fx",  m_showPluginRow);
    tip("Show/hide plugin row");

    tbSep();

    // Pattern edit
    if (actBtn("↩##undo",   has)) { /* TODO: undo */ }
    tip("Undo");
    ImGui::SameLine();
    if (actBtn("≡##props",  has)) { /* TODO: properties */ }
    tip("Pattern properties");
    ImGui::SameLine();
    if (actBtn("↕+##exp",   has)) { /* TODO: expand pattern */ }
    tip("Expand pattern");
    ImGui::SameLine();
    if (actBtn("↕-##shr",   has)) { /* TODO: shrink pattern */ }
    tip("Shrink pattern");

    tbSep();

    togBtn("≣##detail",  m_detailView);
    tip("Detail view");

    tbSep();

    togBtn("⊕##ovf", m_overflowPaste);
    tip("Overflow paste");

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(10);
}

// ── Pattern toolbar row 2 ─────────────────────────────────────────────────────

void TrackerView::drawPatternToolbar2() {
    bool has = m_mod != nullptr;

    // Qt toolbar color scheme
    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32( 20,  20,  40, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32( 64,  64, 160, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,           IM_COL32(224, 224,  64, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   IM_COL32( 64,  64,  64, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32( 13,  13,  30, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32( 26,  26,  46, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32( 46,  46,  90, 255));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      IM_COL32(224, 224,  64, 255));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    {4.0f, 3.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     {4.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetCursorPosY(5.0f);

    // Instrument
    ImGui::TextColored(kDim, "Instr");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(48.0f);
    if (ImGui::InputInt("##instr", &m_curInstrument, 1, 1))
        m_curInstrument = std::clamp(m_curInstrument, 1, 255);
    tip("Current instrument number");

    tbSep();

    // Plugin (stub — no write API yet)
    ImGui::TextColored(kDim, "Plugin");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(70.0f);
    ImGui::BeginDisabled();
    int plugDummy = 0;
    ImGui::InputInt("##plugin2", &plugDummy, 0, 0);
    ImGui::EndDisabled();
    tip("Plugin selector (not yet available)");

    tbSep();

    // Edit step
    ImGui::TextColored(kDim, "Step");
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(44.0f);
    if (ImGui::InputInt("##step2", &m_editStep, 1, 1))
        m_editStep = std::clamp(m_editStep, 0, 16);
    tip("Edit step: rows to advance after entering a note (- / + keys)");

    tbSep();

    togBtn("Loop##lp",   m_loopPattern);
    tip("Loop current pattern during pattern playback");
    ImGui::SameLine();
    togBtn("Follow##fw", m_followSong);
    tip("Follow playback cursor in pattern view");

    tbSep();

    // Pattern name (display only for now)
    ImGui::TextColored(kDim, "Pattern");
    ImGui::SameLine(0, 4);
    int32_t curPat = m_mod ? m_state.current_pattern.load() : -1;
    std::string patName;
    if (curPat >= 0) {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        if (curPat < (int32_t)m_state.pattern_names.size())
            patName = m_state.pattern_names[curPat];
    }
    char patBuf[64] = {};
    strncpy(patBuf, patName.c_str(), 63);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::BeginDisabled(!has);
    ImGui::InputText("##patname", patBuf, 64);
    ImGui::EndDisabled();

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(10);
}

// ── Order list ────────────────────────────────────────────────────────────────

void TrackerView::drawOrderList() {
    int32_t norders  = m_state.num_orders.load();
    int32_t curOrder = m_state.current_order.load();

    if (norders <= 0) {
        ImGui::SetCursorPosY(4);
        ImGui::TextColored(kDim, "  Open a module file  (File > Open or drag & drop)");
        return;
    }

    float boxW = m_charW * 4.2f;
    float boxH = ORDER_H - 4.0f;

    ImGui::SetCursorPosY(2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2.0f, 1.0f});

    for (int32_t i = 0; i < norders; i++) {
        bool isCur = (i == curOrder);

        ImVec4 btnBg = isCur ? kOrderCur : kOrderBg;
        ImGui::PushStyleColor(ImGuiCol_Button,        btnBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{btnBg.x+.1f, btnBg.y+.1f, btnBg.z+.2f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{btnBg.x+.2f, btnBg.y+.2f, btnBg.z+.3f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          isCur ? kTextCurRow : kTextNormal);

        int32_t pat = m_mod ? m_mod->get_order_pattern(i) : i;
        char label[12];
        snprintf(label, sizeof(label), "%02d##o%d", pat, i);
        if (ImGui::Button(label, {boxW, boxH}))
            emit orderSeekRequested(i);

        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 2);
    }

    ImGui::PopStyleVar(3);
}

// ── Stereo VU (thin bars, center-out) ────────────────────────────────────────

void TrackerView::drawStereoVU(ImDrawList* dl, float x, float y, float w, float h,
                                float lvlL, float lvlR)
{
    constexpr float barW   = 2.0f;
    constexpr float stride = 3.0f;  // barW + 1px gap

    float cx  = x + w * 0.5f;
    int   N   = std::max(1, int((w * 0.5f - 1.0f) / stride));
    float bH  = h - 1.0f;

    float normL = std::min(lvlL / 1.2f, 1.0f);
    float normR = std::min(lvlR / 1.2f, 1.0f);

    for (int i = 0; i < N; i++) {
        float frac = float(i) / float(N);

        ImU32 litCol, unlitCol;
        if      (frac >= 0.80f) { litCol = kVuHiLit;  unlitCol = kVuHiUnlit;  }
        else if (frac >= 0.60f) { litCol = kVuMedLit; unlitCol = kVuMedUnlit; }
        else                    { litCol = kVuLoLit;  unlitCol = kVuLoUnlit;  }

        // Right channel — bars spread rightward from centre
        float rx = cx + 1.0f + float(i) * stride;
        dl->AddRectFilled({rx, y}, {rx + barW, y + bH},
                          normR > frac ? litCol : unlitCol);

        // Left channel — bars spread leftward from centre (mirrored)
        float lx = cx - 1.0f - float(i) * stride - barW;
        dl->AddRectFilled({lx, y}, {lx + barW, y + bH},
                          normL > frac ? litCol : unlitCol);
    }
}

// ── Channel strip ─────────────────────────────────────────────────────────────

void TrackerView::drawChannelStrip() {
    int32_t nch = m_state.num_channels.load();
    if (nch <= 0) return;

    // Snapshot VU and channel names
    std::array<ChannelVU, MAX_CHANNELS> vu{};
    {
        std::lock_guard<std::mutex> lk(m_state.vu_mutex);
        vu = m_state.vu;
    }
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

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      wPos = ImGui::GetWindowPos();
    float       wW   = ImGui::GetWindowWidth();

    // "Row" label — vertically centred in the full header height
    {
        float textH = ImGui::GetTextLineHeight();
        float rowLabelY = wPos.y + (m_chanH - textH) * 0.5f;
        dl->AddText({wPos.x + 6, rowLabelY},
                    ImGui::ColorConvertFloat4ToU32(kDim), "Row");
    }

    int32_t plugClickCh = -1; // deferred outside PushID scope

    for (int ch = 0; ch < nch && ch < MAX_CHANNELS; ch++) {
        float chX    = wPos.x + m_rowNumW + float(ch) * m_cellW - m_hscroll;
        float cellW2 = m_cellW - 2.0f;
        if (chX + m_cellW < wPos.x || chX > wPos.x + wW) continue;

        bool muted = (m_muteState >> ch) & 1;

        // Background covers dynamic header height
        ImU32 hdrCol = muted ? IM_COL32(25, 15, 15, 255)
                             : ImGui::ColorConvertFloat4ToU32(kChHeader);
        dl->AddRectFilled({chX, wPos.y}, {chX + cellW2, wPos.y + m_chanH}, hdrCol);

        // Vertical separator
        dl->AddLine({chX + cellW2, wPos.y}, {chX + cellW2, wPos.y + m_chanH},
                    ImGui::ColorConvertFloat4ToU32(kChBorder));

        // Channel name — horizontally centred
        std::string name = chNames[ch];
        if (name.size() > 12) name = name.substr(0, 12);
        ImU32 nameCol = muted ? ImGui::ColorConvertFloat4ToU32(kDim)
                              : ImGui::ColorConvertFloat4ToU32(kTextNormal);
        float nameW = ImGui::CalcTextSize(name.c_str()).x;
        float nameX = chX + (cellW2 - nameW) * 0.5f;
        dl->AddText({nameX, wPos.y + 2}, nameCol, name.c_str());

        float vuBottom = wPos.y + CHANNAME_H; // bottom of name row = VU start

        // VU meters (optional)
        if (m_showVU) {
            drawStereoVU(dl, chX + 2, vuBottom + 1, cellW2 - 2, VU_H - 2,
                         vu[ch].left, vu[ch].right);
            vuBottom += VU_H;
        }

        // Plugin row (optional)
        bool hasPlugin = false;
        if (m_showPluginRow) {
            dl->AddLine({chX, vuBottom}, {chX + cellW2, vuBottom},
                        IM_COL32(46, 46, 92, 255));
            const char* plugLabel = "-";
            float lblW = ImGui::CalcTextSize(plugLabel).x;
            dl->AddText({chX + (cellW2 - lblW) * 0.5f, vuBottom + 2},
                        ImGui::ColorConvertFloat4ToU32(kDim), plugLabel);
        }

        // Hit areas
        ImGui::PushID(ch);

        // Mute: name row + VU row (everything above plugin row)
        float muteH = CHANNAME_H + (m_showVU ? VU_H : 0.f);
        ImGui::SetCursorScreenPos({chX, wPos.y});
        if (ImGui::InvisibleButton("##mute", {cellW2, muteH})) {
            m_muteState ^= (1ULL << ch);
            m_state.channel_mute_mask.store(m_muteState);
        }

        // Plugin row — only clickable when a plugin is assigned
        if (m_showPluginRow && hasPlugin) {
            ImGui::SetCursorScreenPos({chX, vuBottom});
            if (ImGui::InvisibleButton("##plug", {cellW2, PLUGIN_H}))
                plugClickCh = ch;
        }

        ImGui::PopID();
    }

    // Open popup outside per-channel PushID so the ID is stable
    if (plugClickCh >= 0) {
        m_pluginPopupCh = plugClickCh;
        ImGui::OpenPopup("##plugsettings");
    }

    ImGui::SetNextWindowSize({300, 0}, ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##plugsettings", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextColored(kTextNormal, "Channel %d — Plugin Settings",
                           m_pluginPopupCh + 1);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(kDim, "No plugin assigned.");
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Close", {80, 0})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ── Pattern panel ─────────────────────────────────────────────────────────────

void TrackerView::drawPatternPanel() {
    if (!m_mod || m_state.num_channels.load() <= 0) {
        ImGui::TextColored(kDim, " No module loaded.");
        return;
    }

    int32_t curPat  = m_state.current_pattern.load();
    int32_t curRow  = m_state.current_row.load();
    int32_t nch     = m_state.num_channels.load();
    int32_t nrows   = m_mod->get_pattern_num_rows(curPat);

    int rpb = m_mod->get_pattern_rows_per_beat(curPat);
    if (rpb <= 0) rpb = 4;
    int rpm = rpb * 4; // rows per measure

    // One column for row numbers + one per channel, no header row (drawn in channel strip)
    int totalCols = 1 + nch;
    ImGuiTableFlags tFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;

    float rowH  = ImGui::GetTextLineHeightWithSpacing();
    m_rowH = rowH;
    ImVec2 avail = ImGui::GetContentRegionAvail(); // capture before BeginTable
    float visH  = avail.y;

    if (!ImGui::BeginTable("##pat", totalCols, tFlags, avail))
        return;

    ImGui::TableSetupScrollFreeze(1, 0);
    ImGui::TableSetupColumn("##row", ImGuiTableColumnFlags_WidthFixed, m_rowNumW);
    for (int ch = 0; ch < nch; ch++) {
        char id[12]; snprintf(id, sizeof(id), "##c%d", ch);
        ImGui::TableSetupColumn(id, ImGuiTableColumnFlags_WidthFixed, m_cellW);
    }

    // Capture horizontal scroll for channel strip sync (1-frame lag, imperceptible)
    m_hscroll = ImGui::GetScrollX();

    float currentScrollY = ImGui::GetScrollY();
    bool  playing     = m_state.is_playing.load();
    bool  patChanged  = (curPat != m_lastPattern);
    bool  rowRewound  = (playing && !m_editMode && curRow < m_lastRow - 2);

    bool  forceScroll = false;
    float newScrollY  = currentScrollY;

    if (patChanged || rowRewound) {
        // Snap on pattern change or loop rewind
        int32_t snapRow = m_editMode ? m_cur.row : curRow;
        newScrollY  = float(snapRow) * rowH - visH * 0.5f + rowH * 0.5f;
        forceScroll = true;
    } else if (playing && !m_editMode && m_followSong) {
        // Smooth-follow playback cursor
        float targetY = float(curRow) * rowH - visH * 0.5f + rowH * 0.5f;
        float dt = ImGui::GetIO().DeltaTime;
        newScrollY  = currentScrollY + (targetY - currentScrollY) * std::min(1.0f, 8.0f * dt);
        forceScroll = true;
    } else if (m_editMode) {
        // Scroll only when edit cursor leaves the visible area; mouse wheel is free otherwise
        float curTop = float(m_cur.row) * rowH;
        float curBot = curTop + rowH;
        if (curTop < currentScrollY + rowH * 2) {
            newScrollY = curTop - rowH * 2;
            forceScroll = true;
        } else if (curBot > currentScrollY + visH - rowH * 2) {
            newScrollY = curBot - visH + rowH * 2;
            forceScroll = true;
        }
    }

    if (forceScroll) {
        newScrollY = std::max(0.f, std::min(newScrollY, float(nrows) * rowH));
        ImGui::SetScrollY(newScrollY);
        m_scrollY = newScrollY;
    } else {
        m_scrollY = currentScrollY; // track actual scroll for click hit-testing
    }

    m_lastPattern = curPat;
    m_lastRow     = curRow;

    ImGuiListClipper clipper;
    clipper.Begin(nrows);
    while (clipper.Step()) {
    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
        ImGui::TableNextRow();
        bool isCur     = (r == curRow);
        bool isMeasure = (r % rpm == 0);
        bool isBeat    = (r % rpb == 0) && !isMeasure;

        if (isCur) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::ColorConvertFloat4ToU32(kBgCurRow));
        } else if (isMeasure) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::ColorConvertFloat4ToU32(kBgMeasure));
        } else if (isBeat) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::ColorConvertFloat4ToU32(kBgHilight));
        }

        // Row number
        ImGui::TableNextColumn();
        ImGui::TextColored(isCur ? kTextCurRow : (isBeat||isMeasure ? kTextNormal : kDim),
                           "%3d", r);

        // Channel cells
        for (int ch = 0; ch < nch; ch++) {
            ImGui::TableNextColumn();

            bool isEditCell = m_editMode && (r == m_cur.row) && (ch == m_cur.channel);

            // Sub-column widths (chars): note=3, instr=2, vol=3, fxcmd=1, fxparam=2
            // We record screen-x of each sub-col start to draw the cursor highlight.
            float subX[5]; // screen X of each sub-column text start
            float subW[5]; // approximate pixel width
            bool  colFirst = true;

            auto recordPos = [&](int col) {
                subX[col] = ImGui::GetCursorScreenPos().x;
            };

            std::string note = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_note);
            std::string inst = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_instrument);
            std::string vol  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_volumeffect);
            std::string fx   = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_effect);
            std::string fxp  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_parameter);

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = !inst.empty() && inst != ".." && inst != "00";
            bool hasVol  = !vol.empty()  && vol.find_first_not_of("v.") != std::string::npos;
            bool hasFx   = !fx.empty()   && fx  != ".";

            recordPos(0);
            ImGui::TextColored(hasNote ? kColNote : kDim,
                               "%-3s", note.empty() ? "..." : note.c_str());
            subW[0] = ImGui::GetItemRectSize().x + 3;
            ImGui::SameLine(0, 3);
            recordPos(1);
            ImGui::TextColored(hasInst ? kColInstr : kDim,
                               "%2s", inst.empty() ? ".." : inst.c_str());
            subW[1] = ImGui::GetItemRectSize().x + 3;
            ImGui::SameLine(0, 3);
            recordPos(2);
            ImGui::TextColored(hasVol ? kColVolume : kDim,
                               "%-3s", vol.empty() ? "..." : vol.c_str());
            subW[2] = ImGui::GetItemRectSize().x + 2;
            ImGui::SameLine(0, 2);
            recordPos(3);
            ImGui::TextColored(hasFx ? kColGlobal : kDim,
                               "%s", fx.empty() ? "." : fx.c_str());
            subW[3] = ImGui::GetItemRectSize().x;
            ImGui::SameLine(0, 0);
            recordPos(4);
            ImGui::TextColored(hasFx ? kColGlobal : kDim,
                               "%-2s", fxp.empty() ? ".." : fxp.c_str());
            subW[4] = ImGui::GetItemRectSize().x;

            if (isEditCell) {
                int col = int(m_cur.col);
                float y0 = ImGui::GetItemRectMin().y;
                float y1 = ImGui::GetItemRectMax().y;
                ImU32 hilite = m_editMode ? IM_COL32(255,220,80,80) : IM_COL32(255,255,255,40);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    {subX[col]-1, y0}, {subX[col]+subW[col], y1+1}, hilite);
                // bright border around whole cell
                float x0cell = subX[0] - 1;
                float x1cell = subX[4] + subW[4] + 1;
                ImGui::GetWindowDrawList()->AddRect(
                    {x0cell, y0-1}, {x1cell, y1+1}, IM_COL32(255,255,255,220));
            }
        }
    }
    } // clipper.Step()
    clipper.End();

    ImGui::EndTable();
}

// ── Qt input → ImGui ──────────────────────────────────────────────────────────

static ImGuiKey qtKeyToImGui(int k) {
    switch(k) {
    case Qt::Key_Tab:       return ImGuiKey_Tab;
    case Qt::Key_Left:      return ImGuiKey_LeftArrow;
    case Qt::Key_Right:     return ImGuiKey_RightArrow;
    case Qt::Key_Up:        return ImGuiKey_UpArrow;
    case Qt::Key_Down:      return ImGuiKey_DownArrow;
    case Qt::Key_PageUp:    return ImGuiKey_PageUp;
    case Qt::Key_PageDown:  return ImGuiKey_PageDown;
    case Qt::Key_Home:      return ImGuiKey_Home;
    case Qt::Key_End:       return ImGuiKey_End;
    case Qt::Key_Delete:    return ImGuiKey_Delete;
    case Qt::Key_Backspace: return ImGuiKey_Backspace;
    case Qt::Key_Return:    return ImGuiKey_Enter;
    case Qt::Key_Escape:    return ImGuiKey_Escape;
    case Qt::Key_Space:     return ImGuiKey_Space;
    default:                return ImGuiKey_None;
    }
}

void TrackerView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    ImGui::SetCurrentContext(m_imgui);
    ImGui::GetIO().AddMousePosEvent(float(e->pos().x()), float(e->pos().y()));
    if (e->button() == Qt::LeftButton)  ImGui::GetIO().AddMouseButtonEvent(0, true);
    if (e->button() == Qt::RightButton) ImGui::GetIO().AddMouseButtonEvent(1, true);

    // Click inside pattern panel → move edit cursor
    if (m_mod && e->button() == Qt::LeftButton && m_rowH > 0.f) {
        float patY = m_patTop;
        float qx = float(e->pos().x());
        float qy = float(e->pos().y());
        if (qy >= patY) {
            float relY = qy - patY + m_scrollY;
            float relX = qx - m_rowNumW + m_hscroll;
            int row = int(relY / m_rowH);
            int ch  = int(relX / m_cellW);
            int32_t nch   = m_state.num_channels.load();
            int32_t nrows = pe_num_rows(m_mod, m_state.current_pattern.load());
            if (row >= 0 && row < nrows && ch >= 0 && ch < nch) {
                m_cur.row     = row;
                m_cur.channel = ch;
                m_cur.col     = EditCol::Note;
            }
        }
    }
}
void TrackerView::mouseReleaseEvent(QMouseEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    if (e->button() == Qt::LeftButton)  ImGui::GetIO().AddMouseButtonEvent(0, false);
    if (e->button() == Qt::RightButton) ImGui::GetIO().AddMouseButtonEvent(1, false);
}
void TrackerView::mouseMoveEvent(QMouseEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGui::GetIO().AddMousePosEvent(float(e->pos().x()), float(e->pos().y()));
}
void TrackerView::wheelEvent(QWheelEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    float dx = e->angleDelta().x() / 120.0f;
    float dy = e->angleDelta().y() / 120.0f;
    // Shift+scroll → horizontal (for mice without h-wheel)
    if ((e->modifiers() & Qt::ShiftModifier) && dx == 0.f) {
        ImGui::GetIO().AddMouseWheelEvent(dy, 0.f);
        return;
    }
    // Use dominant axis only — trackpad always leaks a small x during vertical swipes
    if (std::abs(dx) > std::abs(dy))
        ImGui::GetIO().AddMouseWheelEvent(dx, 0.f);
    else
        ImGui::GetIO().AddMouseWheelEvent(0.f, dy);
}
void TrackerView::keyPressEvent(QKeyEvent* e) {
    ImGui::SetCurrentContext(m_imgui);

    // Intercept tracker edit / navigation keys before ImGui sees them
    if (m_mod) {
        handleEditKey(e->key(), e->modifiers(), e->text());
        // Still pass non-text keys to ImGui for scrolling etc., but swallow text
        // so ImGui text input widgets don't capture note keys.
        ImGuiKey k = qtKeyToImGui(e->key());
        if (k != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(k, true);
        return;
    }

    ImGuiKey k = qtKeyToImGui(e->key());
    if (k != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(k, true);
    if (!e->text().isEmpty())
        ImGui::GetIO().AddInputCharactersUTF8(e->text().toUtf8().constData());
}
void TrackerView::keyReleaseEvent(QKeyEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGuiKey k = qtKeyToImGui(e->key());
    if (k != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(k, false);
}

// ── Pattern editor ─────────────────────────────────────────────────────────────

void TrackerView::clampCursor() {
    if (!m_mod) return;
    int32_t pat   = m_state.current_pattern.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    int32_t nch   = m_state.num_channels.load();
    m_cur.row     = std::clamp(m_cur.row,     0, std::max(0, nrows-1));
    m_cur.channel = std::clamp(m_cur.channel, 0, std::max(0, nch-1));
}

void TrackerView::advanceCursor() {
    if (!m_mod || m_editStep == 0) return;
    int32_t pat   = m_state.current_pattern.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    m_cur.row = std::min(m_cur.row + m_editStep, nrows - 1);
}

void TrackerView::handleEditKey(int qtKey, Qt::KeyboardModifiers mods, const QString& text) {
    int32_t pat   = m_state.current_pattern.load();
    int32_t nrows = pe_num_rows(m_mod, pat);
    int32_t nch   = m_state.num_channels.load();

    // ── Navigation (always active) ───────────────────────────────────────────
    switch (qtKey) {
    case Qt::Key_Up:
        m_cur.row = std::max(0, m_cur.row - 1); return;
    case Qt::Key_Down:
        m_cur.row = std::min(nrows-1, m_cur.row + 1); return;
    case Qt::Key_Left:
        if (m_cur.col == EditCol::Note) {
            if (m_cur.channel > 0) { m_cur.channel--; m_cur.col = EditCol::FxParam; }
        } else {
            m_cur.col = EditCol(int(m_cur.col) - 1);
        }
        return;
    case Qt::Key_Right:
        if (m_cur.col == EditCol::FxParam) {
            if (m_cur.channel < nch-1) { m_cur.channel++; m_cur.col = EditCol::Note; }
        } else {
            m_cur.col = EditCol(int(m_cur.col) + 1);
        }
        return;
    case Qt::Key_Tab:
        if (mods & Qt::ShiftModifier)
            m_cur.channel = (m_cur.channel + nch - 1) % nch;
        else
            m_cur.channel = (m_cur.channel + 1) % nch;
        m_cur.col = EditCol::Note;
        return;
    case Qt::Key_PageUp:
        m_cur.row = std::max(0, m_cur.row - 16); return;
    case Qt::Key_PageDown:
        m_cur.row = std::min(nrows-1, m_cur.row + 16); return;
    case Qt::Key_Home:
        m_cur.row = 0; return;
    case Qt::Key_End:
        m_cur.row = nrows - 1; return;
    case Qt::Key_F1: m_baseOctave = std::max(0, m_baseOctave - 1); return;
    case Qt::Key_F2: m_baseOctave = std::min(8, m_baseOctave + 1); return;
    case Qt::Key_Minus:
        m_editStep = std::max(0, m_editStep - 1); return;
    case Qt::Key_Plus: case Qt::Key_Equal:
        m_editStep = std::min(16, m_editStep + 1); return;
    case Qt::Key_Return: case Qt::Key_Enter:
        m_editMode = !m_editMode; return;
    default: break;
    }

    if (!m_editMode) return;

    PatternCell cell;
    pe_get_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);

    // ── Note column ──────────────────────────────────────────────────────────
    if (m_cur.col == EditCol::Note) {
        if (qtKey == Qt::Key_1) {
            cell.note = NOTE_NOTECUT; pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        if (qtKey == Qt::Key_QuoteLeft) {
            cell.note = NOTE_KEYOFF; pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        if (qtKey == Qt::Key_Delete) {
            pe_clear_cell(m_mod, pat, m_cur.row, m_cur.channel);
            advanceCursor(); return;
        }
        int semi = noteFromQtKey(qtKey);
        if (semi >= 0) {
            int note = (m_baseOctave + semi/12) * 12 + (semi % 12) + 1;
            if (note >= NOTE_MIN && note <= NOTE_MAX) {
                cell.note = (uint8_t)note;
                pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
                advanceCursor();
            }
            return;
        }
        return;
    }

    // ── Instrument column ────────────────────────────────────────────────────
    if (m_cur.col == EditCol::Instr) {
        if (qtKey == Qt::Key_Delete) {
            cell.instr = 0; pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        int d = hexDigitFromKey(qtKey);
        if (d >= 0) {
            cell.instr = (uint8_t)(((cell.instr % 10) * 10 + d) % 100);
            pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        return;
    }

    // ── Volume column ─────────────────────────────────────────────────────────
    if (m_cur.col == EditCol::Vol) {
        if (qtKey == Qt::Key_Delete) {
            cell.volcmd = VOLCMD_NONE; cell.vol = 0;
            pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        int d = hexDigitFromKey(qtKey);
        if (d >= 0 && d <= 9) {
            if (cell.volcmd == VOLCMD_NONE) cell.volcmd = VOLCMD_VOLUME;
            cell.vol = (uint8_t)std::min((int)(cell.vol % 10) * 10 + d, 64);
            pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        return;
    }

    // ── Effect command column ─────────────────────────────────────────────────
    if (m_cur.col == EditCol::FxCmd) {
        if (qtKey == Qt::Key_Delete) {
            cell.command = CMD_NONE; cell.param = 0;
            pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        if (!text.isEmpty()) {
            QChar c = text[0].toUpper();
            if (c >= 'A' && c <= 'Z') {
                cell.command = (uint8_t)(c.toLatin1() - 'A' + 1);
                pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
                advanceCursor(); return;
            }
        }
        return;
    }

    // ── Effect parameter column ───────────────────────────────────────────────
    if (m_cur.col == EditCol::FxParam) {
        if (qtKey == Qt::Key_Delete) {
            cell.param = 0; pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        int d = hexDigitFromKey(qtKey);
        if (d >= 0) {
            cell.param = (uint8_t)(((cell.param & 0x0F) << 4) | d);
            pe_set_cell(m_mod, pat, m_cur.row, m_cur.channel, cell);
            advanceCursor(); return;
        }
        return;
    }
}
