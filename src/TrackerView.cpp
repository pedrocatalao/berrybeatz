#include "TrackerView.h"
#include <libopenmpt/libopenmpt.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDateTime>

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
static const ImVec4 kOrderBg     { 20/255.f,  20/255.f,  45/255.f, 1.f};
static const ImVec4 kOrderCur    { 64/255.f,  64/255.f, 160/255.f, 1.f}; // #4040A0

// VU LED colours (exact from ColorSchemes.h)
static const ImU32 kVuLoLit    = IM_COL32(  0, 200,   0, 255); // #00C800
static const ImU32 kVuLoUnlit  = IM_COL32(  0,  28,   0, 255);
static const ImU32 kVuMedLit   = IM_COL32(255, 200,   0, 255); // #FFC800
static const ImU32 kVuMedUnlit = IM_COL32( 40,  30,   0, 255);
static const ImU32 kVuHiLit    = IM_COL32(225,   0,   0, 255); // #E10000
static const ImU32 kVuHiUnlit  = IM_COL32( 35,   0,   0, 255);

// ── Layout constants (match OpenMPT Draw_pat.cpp) ─────────────────────────────
static constexpr float ORDER_H   = 26.0f; // order list strip height
static constexpr float CHANNAME_H= 18.0f; // channel name row
static constexpr float VU_H      = 13.0f; // LED VU row  (VUMETERS_HEIGHT)
static constexpr float CHAN_H    = CHANNAME_H + VU_H;

// ── TrackerView ───────────────────────────────────────────────────────────────

TrackerView::TrackerView(ModuleState& state, QWidget* parent)
    : QOpenGLWidget(parent), m_state(state) {
    setFocusPolicy(Qt::StrongFocus);
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
    m_rowNumW = m_charW * 5.0f;          // "  001 "
    m_cellW   = m_charW * 16.0f;         // note+inst+vol+fx

    ImGuiWindowFlags noDecoFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    // ── Order list ──────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, 0});
    ImGui::SetNextWindowSize({W, ORDER_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kOrderBg);
    ImGui::Begin("##orders", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawOrderList();
    ImGui::End();

    // ── Channel strip ────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos ({0, ORDER_H});
    ImGui::SetNextWindowSize({W, CHAN_H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kChHeader);
    ImGui::Begin("##chanstrip", nullptr, noDecoFlags |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    drawChannelStrip();
    ImGui::End();

    // ── Pattern panel ────────────────────────────────────────────────────────
    float patY = ORDER_H + CHAN_H;
    ImGui::SetNextWindowPos ({0, patY});
    ImGui::SetNextWindowSize({W, H - patY});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kBgNormal);
    ImGui::Begin("##pattern", nullptr, noDecoFlags);
    ImGui::PopStyleColor();
    drawPatternPanel();
    ImGui::End();
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

        char label[12];
        snprintf(label, sizeof(label), "%02d##o%d", i, i);
        if (ImGui::Button(label, {boxW, boxH}))
            emit orderSeekRequested(i);

        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 2);
    }

    ImGui::PopStyleVar(3);
}

// ── LED VU bar ────────────────────────────────────────────────────────────────
// 8 LEDs per channel: 0-3 green, 4-5 yellow, 6-7 red (matches OpenMPT)

void TrackerView::drawLEDs(ImDrawList* dl, float x, float y, float w, float h, float level) {
    constexpr int N = 8;
    float ledW = (w - float(N - 1)) / float(N);
    if (ledW < 1.0f) ledW = 1.0f;
    for (int i = 0; i < N; i++) {
        bool lit = level > float(i) / float(N);
        ImU32 col;
        if      (i >= 6) col = lit ? kVuHiLit  : kVuHiUnlit;
        else if (i >= 4) col = lit ? kVuMedLit : kVuMedUnlit;
        else             col = lit ? kVuLoLit  : kVuLoUnlit;
        float px = x + float(i) * (ledW + 1.0f);
        dl->AddRectFilled({px, y}, {px + ledW, y + h - 1.0f}, col);
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
                chNames[ch] = m_state.channel_names[ch];
            else
                chNames[ch] = std::to_string(ch + 1);
        }
    }

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      wPos = ImGui::GetWindowPos();
    float       wW   = ImGui::GetWindowWidth();

    // "Row" label in the row-number column
    dl->AddText({wPos.x + 6, wPos.y + 2},
                ImGui::ColorConvertFloat4ToU32(kDim), "Row");

    for (int ch = 0; ch < nch && ch < MAX_CHANNELS; ch++) {
        float chX = wPos.x + m_rowNumW + float(ch) * m_cellW - m_hscroll;
        if (chX + m_cellW < wPos.x || chX > wPos.x + wW) continue;

        bool muted = (m_muteState >> ch) & 1;

        // Channel background (slightly lighter than window bg)
        ImU32 hdrCol = muted
            ? IM_COL32(25, 15, 15, 255)
            : ImGui::ColorConvertFloat4ToU32(kChHeader);
        dl->AddRectFilled({chX,             wPos.y},
                          {chX + m_cellW-2, wPos.y + CHAN_H},
                          hdrCol);

        // Separator line
        dl->AddLine({chX + m_cellW - 2, wPos.y},
                    {chX + m_cellW - 2, wPos.y + CHAN_H},
                    ImGui::ColorConvertFloat4ToU32(kChBorder));

        // Channel name (truncated)
        std::string name = chNames[ch].size() > 9 ? chNames[ch].substr(0,9) : chNames[ch];
        ImU32 nameCol = muted
            ? ImGui::ColorConvertFloat4ToU32(kDim)
            : ImGui::ColorConvertFloat4ToU32(kTextNormal);
        dl->AddText({chX + 4, wPos.y + 2}, nameCol, name.c_str());

        // Mute button ("M") — top-right of channel header
        float mBtnX = chX + m_cellW - 14;
        float mBtnY = wPos.y + 2;
        ImU32 mBgCol = muted ? IM_COL32(160, 40, 40, 255) : IM_COL32(50, 50, 80, 255);
        dl->AddRectFilled({mBtnX, mBtnY}, {mBtnX+11, mBtnY+13}, mBgCol);
        dl->AddRect      ({mBtnX, mBtnY}, {mBtnX+11, mBtnY+13},
                          IM_COL32(120, 120, 160, 255));
        dl->AddText({mBtnX+2, mBtnY+1}, IM_COL32(220,220,220,255), "M");

        // Invisible hit area for mute button
        ImGui::SetCursorScreenPos({mBtnX, mBtnY});
        ImGui::PushID(ch);
        if (ImGui::InvisibleButton("##m", {11, 13}))
            m_muteState ^= (1ULL << ch);
        ImGui::PopID();

        // LED VU bar
        float level = std::min(std::max(vu[ch].left, vu[ch].right), 1.2f) / 1.2f;
        drawLEDs(dl, chX + 2, wPos.y + CHANNAME_H + 1, m_cellW - 4, VU_H - 2, level);
    }
}

// ── Pattern panel ─────────────────────────────────────────────────────────────

void TrackerView::drawPatternPanel() {
    if (!m_mod || !m_state.is_playing.load()) {
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

    if (!ImGui::BeginTable("##pat", totalCols, tFlags,
                           ImGui::GetContentRegionAvail()))
        return;

    ImGui::TableSetupScrollFreeze(1, 0);
    ImGui::TableSetupColumn("##row", ImGuiTableColumnFlags_WidthFixed, m_rowNumW);
    for (int ch = 0; ch < nch; ch++) {
        char id[12]; snprintf(id, sizeof(id), "##c%d", ch);
        ImGui::TableSetupColumn(id, ImGuiTableColumnFlags_WidthFixed, m_cellW);
    }

    // Capture horizontal scroll for channel strip sync (1-frame lag, imperceptible)
    m_hscroll = ImGui::GetScrollX();

    float rowH = ImGui::GetTextLineHeightWithSpacing();
    float visH = ImGui::GetContentRegionAvail().y;

    // Smooth scroll: lerp toward target that centres curRow
    float targetY = float(curRow) * rowH - visH * 0.5f + rowH * 0.5f;
    targetY = std::max(0.f, std::min(targetY, float(nrows) * rowH));
    if (curPat != m_lastPattern) {
        m_scrollY = targetY; // snap on pattern change, no flash
        m_lastPattern = curPat;
    } else {
        float dt = ImGui::GetIO().DeltaTime;
        m_scrollY += (targetY - m_scrollY) * std::min(1.0f, 18.0f * dt);
    }
    ImGui::SetScrollY(m_scrollY);

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
            if ((m_muteState >> ch) & 1) {
                ImGui::TextColored(kDim, "--- -- ... ...");
                continue;
            }

            std::string note = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_note);
            std::string inst = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_instrument);
            std::string vol  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_volumeffect);
            std::string fx   = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_effect);
            std::string fxp  = m_mod->format_pattern_row_channel_command(curPat, r, ch, openmpt::module::command_parameter);

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = !inst.empty() && inst != ".." && inst != "00";
            bool hasVol  = !vol.empty()  && vol.find_first_not_of("v.") != std::string::npos;
            bool hasFx   = !fx.empty()   && fx  != ".";

            ImGui::TextColored(hasNote ? kColNote : kDim,
                               "%-3s", note.empty() ? "..." : note.c_str());
            ImGui::SameLine(0, 3);
            ImGui::TextColored(hasInst ? kColInstr : kDim,
                               "%2s", inst.empty() ? ".." : inst.c_str());
            ImGui::SameLine(0, 3);
            ImGui::TextColored(hasVol ? kColVolume : kDim,
                               "%-3s", vol.empty() ? "..." : vol.c_str());
            ImGui::SameLine(0, 2);
            ImGui::TextColored(hasFx ? kColGlobal : kDim,
                               "%s%-2s", fx.empty()?"."  : fx.c_str(),
                                         fxp.empty()? ".." : fxp.c_str());
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
    ImGui::SetCurrentContext(m_imgui);
    ImGui::GetIO().AddMousePosEvent(float(e->pos().x()), float(e->pos().y()));
    if (e->button() == Qt::LeftButton)  ImGui::GetIO().AddMouseButtonEvent(0, true);
    if (e->button() == Qt::RightButton) ImGui::GetIO().AddMouseButtonEvent(1, true);
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
    ImGui::GetIO().AddMouseWheelEvent(0.f, e->angleDelta().y() / 120.0f);
}
void TrackerView::keyPressEvent(QKeyEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
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
