#include "TrackerView.h"
#include <libopenmpt/libopenmpt.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QCoreApplication>

// Colour palette
static const ImVec4 kColorBg         {0.08f, 0.08f, 0.10f, 1.0f};
static const ImVec4 kColorRowCurrent {0.20f, 0.55f, 0.20f, 0.85f};
static const ImVec4 kColorRowBeat    {0.18f, 0.18f, 0.24f, 1.0f};
static const ImVec4 kColorNote       {0.80f, 0.90f, 0.80f, 1.0f};
static const ImVec4 kColorInstr      {0.70f, 0.75f, 1.00f, 1.0f};
static const ImVec4 kColorEffect     {1.00f, 0.80f, 0.40f, 1.0f};
static const ImVec4 kColorDim        {0.40f, 0.40f, 0.40f, 1.0f};
static const ImVec4 kColorVUGreen    {0.20f, 0.85f, 0.20f, 1.0f};
static const ImVec4 kColorVUYellow   {0.90f, 0.85f, 0.10f, 1.0f};
static const ImVec4 kColorVURed      {0.95f, 0.20f, 0.10f, 1.0f};

TrackerView::TrackerView(ModuleState& state, QWidget* parent)
    : QOpenGLWidget(parent), m_state(state) {
    setFocusPolicy(Qt::StrongFocus);
    connect(&m_timer, &QTimer::timeout, this, [this]{ update(); });
    m_timer.start(16); // ~60fps
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

    // Load Menlo (macOS system monospace) scaled for Retina.
    // Physical font size = logical size * dpr for sharp rendering.
    float dpr      = float(devicePixelRatio());
    float fontSize = 13.0f;
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    const char* menlo = "/System/Library/Fonts/Menlo.ttc";
    if (io.Fonts->AddFontFromFileTTF(menlo, fontSize * dpr, &cfg) == nullptr)
        io.Fonts->AddFontDefault(); // fallback if font missing
    io.FontGlobalScale = 1.0f / dpr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 2.0f;
    style.FrameRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.WindowBorderSize  = 0.0f;
    style.CellPadding       = {4.0f, 1.0f};

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
    float dt = float(now - m_lastTime);
    if (dt > 0.1f) dt = 0.1f;
    m_lastTime = now;

    float dpr = float(devicePixelRatio());
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize             = ImVec2(float(width()), float(height()));
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);
    io.DeltaTime               = (dt > 0.0f) ? dt : 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    renderUI();

    ImGui::Render();

    glViewport(0, 0, int(width() * dpr), int(height() * dpr));
    const ImVec4& bg = kColorBg;
    glClearColor(bg.x, bg.y, bg.z, bg.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void TrackerView::renderUI() {
    const float W = float(width());
    const float H = float(height());

    // Info bar at top (fixed height)
    const float INFO_H  = 52.0f;
    // VU meters on the right
    const float VU_W    = 180.0f;
    // Pattern panel fills the rest
    const float PAT_W   = W - VU_W;
    const float PAT_H   = H - INFO_H;

    drawInfoBar();

    // Pattern panel
    ImGui::SetNextWindowPos ({0,      INFO_H});
    ImGui::SetNextWindowSize({PAT_W,  PAT_H});
    drawPatternPanel();

    // VU meters
    ImGui::SetNextWindowPos ({PAT_W,  INFO_H});
    ImGui::SetNextWindowSize({VU_W,   PAT_H});
    drawVUMeters();
}

void TrackerView::drawInfoBar() {
    ImGui::SetNextWindowPos ({0, 0});
    ImGui::SetNextWindowSize({float(width()), 52.0f});
    ImGui::Begin("##info", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    std::string title, typeStr;
    double bpm, pos, dur;
    int32_t speed, order, pattern, row, norders;
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        title   = m_state.title.empty() ? "(no file)" : m_state.title;
        typeStr = m_state.type_long;
    }
    bpm     = m_state.bpm.load();
    pos     = m_state.position_seconds.load();
    dur     = m_state.duration_seconds.load();
    speed   = m_state.current_speed.load();
    order   = m_state.current_order.load();
    pattern = m_state.current_pattern.load();
    row     = m_state.current_row.load();
    norders = m_state.num_orders.load();

    int posM = int(pos) / 60, posS = int(pos) % 60;
    int durM = int(dur) / 60, durS = int(dur) % 60;

    ImGui::TextColored({0.9f,0.9f,0.5f,1.0f}, "%s", title.c_str());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(kColorDim, "%s", typeStr.c_str());

    ImGui::Text("Ord %d/%d  Pat %d  Row %d    BPM %.1f  Spd %d    %02d:%02d / %02d:%02d",
        order, norders, pattern, row,
        bpm, speed,
        posM, posS, durM, durS);

    ImGui::End();
}

void TrackerView::drawPatternPanel() {
    ImGui::Begin("##pattern", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!m_mod || !m_state.is_playing.load()) {
        ImGui::TextColored(kColorDim, "Open a module file to start playback.");
        ImGui::End();
        return;
    }

    int32_t curPattern  = m_state.current_pattern.load();
    int32_t curRow      = m_state.current_row.load();
    int32_t nch         = m_state.num_channels.load();
    int32_t nrows       = m_mod->get_pattern_num_rows(curPattern);
    int     rowsPerBeat = m_mod->get_pattern_rows_per_beat(curPattern);
    if (rowsPerBeat <= 0) rowsPerBeat = 4;

    // Cache channel names once
    std::vector<std::string> chNames(nch);
    {
        std::lock_guard<std::mutex> lk(m_state.meta_mutex);
        for (int32_t ch = 0; ch < nch; ++ch) {
            if (ch < (int32_t)m_state.channel_names.size() && !m_state.channel_names[ch].empty())
                chNames[ch] = m_state.channel_names[ch];
            else
                chNames[ch] = std::to_string(ch + 1);
        }
    }

    // Table: 1 row-number column + 4 sub-columns per channel (note/inst/vol/fx)
    // Using a single ImGui column per channel keeps horizontal scroll clean.
    float charW   = ImGui::CalcTextSize("A").x;
    float rowNumW = charW * 4.0f;
    float cellW   = charW * 14.0f; // note(3)+inst(2)+vol(3)+fx(3)+padding

    int totalCols = 1 + nch; // row# + one col per channel
    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_NoHostExtendX;

    ImVec2 tableSize = ImGui::GetContentRegionAvail();
    if (!ImGui::BeginTable("##pat_table", totalCols, tableFlags, tableSize))
    {
        ImGui::End();
        return;
    }

    // Set up columns
    ImGui::TableSetupScrollFreeze(1, 1); // freeze row# col and header row
    ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthFixed, rowNumW);
    for (int32_t ch = 0; ch < nch; ++ch)
        ImGui::TableSetupColumn(chNames[ch].c_str(), ImGuiTableColumnFlags_WidthFixed, cellW);
    ImGui::TableHeadersRow();

    // Estimate visible rows to avoid rendering off-screen rows
    float rowH    = ImGui::GetTextLineHeightWithSpacing();
    int   visRows = std::max(1, int(tableSize.y / rowH));
    int   startRow = std::max(0, curRow - visRows / 2);
    int   endRow   = std::min(nrows, startRow + visRows + 4);

    // Scroll table to keep current row centred
    ImGui::SetScrollY(float(startRow) * rowH);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int32_t r = startRow; r < endRow; ++r) {
        ImGui::TableNextRow();

        bool isCurrent = (r == curRow);
        bool isBeat    = (r % rowsPerBeat == 0);
        bool isMeasure = (r % (rowsPerBeat * 4) == 0);

        // Highlight current row across the full table width
        if (isCurrent) {
            ImVec2 rMin = ImGui::GetItemRectMin();
            // Draw after all cells via TableNextRow rect
            ImU32 col = ImGui::ColorConvertFloat4ToU32(kColorRowCurrent);
            ImVec2 p0 = {ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y};
            ImVec2 p1 = {p0.x + ImGui::GetWindowWidth(), p0.y + rowH};
            dl->AddRectFilled(p0, p1, col);
            (void)rMin;
        } else if (isMeasure) {
            ImVec2 p0 = {ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y};
            ImVec2 p1 = {p0.x + ImGui::GetWindowWidth(), p0.y + rowH};
            dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32({0.16f,0.16f,0.22f,1.0f}));
        } else if (isBeat) {
            ImVec2 p0 = {ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y};
            ImVec2 p1 = {p0.x + ImGui::GetWindowWidth(), p0.y + rowH};
            dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(kColorRowBeat));
        }

        // Row number
        ImGui::TableNextColumn();
        ImGui::TextColored(isCurrent ? ImVec4{1,1,1,1} : (isBeat ? ImVec4{0.7f,0.7f,0.7f,1} : kColorDim),
                           "%03d", r);

        // Channel cells
        for (int32_t ch = 0; ch < nch; ++ch) {
            ImGui::TableNextColumn();

            std::string note = m_mod->format_pattern_row_channel_command(curPattern, r, ch, openmpt::module::command_note);
            std::string inst = m_mod->format_pattern_row_channel_command(curPattern, r, ch, openmpt::module::command_instrument);
            std::string vol  = m_mod->format_pattern_row_channel_command(curPattern, r, ch, openmpt::module::command_volumeffect);
            std::string fx   = m_mod->format_pattern_row_channel_command(curPattern, r, ch, openmpt::module::command_effect);
            std::string fxp  = m_mod->format_pattern_row_channel_command(curPattern, r, ch, openmpt::module::command_parameter);

            bool hasNote = !note.empty() && note != "..." && note != "---";
            bool hasInst = !inst.empty() && inst != ".." && inst != "00";
            bool hasFx   = (!fx.empty()  && fx  != ".") ||
                           (!vol.empty() && vol != "." && vol != "v..");

            // Note
            ImGui::TextColored(hasNote ? kColorNote : kColorDim, "%-3s", note.empty() ? "..." : note.c_str());
            ImGui::SameLine(0, 3);
            // Instrument
            ImGui::TextColored(hasInst ? kColorInstr : kColorDim, "%2s", inst.empty() ? ".." : inst.c_str());
            ImGui::SameLine(0, 3);
            // Volume effect
            ImGui::TextColored(hasFx ? kColorEffect : kColorDim, "%s", vol.empty() ? "..." : vol.c_str());
            ImGui::SameLine(0, 1);
            // Effect + param
            ImGui::TextColored(hasFx ? kColorEffect : kColorDim, "%s%s",
                fx.empty() ? "." : fx.c_str(),
                fxp.empty() ? ".." : fxp.c_str());
        }
    }

    ImGui::EndTable();
    ImGui::End();
}

void TrackerView::drawVUMeters() {
    ImGui::Begin("##vu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    int32_t nch = m_state.num_channels.load();
    if (nch <= 0) {
        ImGui::End();
        return;
    }

    std::array<ChannelVU, MAX_CHANNELS> vu;
    {
        std::lock_guard<std::mutex> lk(m_state.vu_mutex);
        vu = m_state.vu;
    }

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    float barW      = ImGui::GetContentRegionAvail().x - 8.0f;
    float rowH      = ImGui::GetTextLineHeightWithSpacing();
    float barH      = std::max(4.0f, rowH - 4.0f);

    auto vuColor = [](float v) -> ImVec4 {
        if (v > 0.9f) return kColorVURed;
        if (v > 0.6f) return kColorVUYellow;
        return kColorVUGreen;
    };

    for (int32_t ch = 0; ch < nch && ch < MAX_CHANNELS; ++ch) {
        float level = std::max(vu[ch].left, vu[ch].right);
        level = std::min(level, 1.2f);

        ImVec2 p = ImGui::GetCursorScreenPos();
        // Background
        dl->AddRectFilled(p, {p.x + barW, p.y + barH},
                          IM_COL32(30, 30, 30, 255));
        // Fill
        float fill = (level / 1.2f) * barW;
        ImVec4 c   = vuColor(level);
        dl->AddRectFilled(p, {p.x + fill, p.y + barH},
                          ImGui::ColorConvertFloat4ToU32(c));
        // Channel number overlay
        dl->AddText(p, IM_COL32(200,200,200,180),
                    std::to_string(ch + 1).c_str());

        ImGui::Dummy({barW, barH + 2.0f});
    }

    ImGui::End();
}

// Qt → ImGui input forwarding

static ImGuiKey qtKeyToImGui(int key) {
    switch(key) {
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
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(float(e->pos().x()), float(e->pos().y()));
    if (e->button() == Qt::LeftButton)  io.AddMouseButtonEvent(0, true);
    if (e->button() == Qt::RightButton) io.AddMouseButtonEvent(1, true);
}

void TrackerView::mouseReleaseEvent(QMouseEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGuiIO& io = ImGui::GetIO();
    if (e->button() == Qt::LeftButton)  io.AddMouseButtonEvent(0, false);
    if (e->button() == Qt::RightButton) io.AddMouseButtonEvent(1, false);
}

void TrackerView::mouseMoveEvent(QMouseEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGui::GetIO().AddMousePosEvent(float(e->pos().x()), float(e->pos().y()));
}

void TrackerView::wheelEvent(QWheelEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGui::GetIO().AddMouseWheelEvent(0.0f, e->angleDelta().y() / 120.0f);
}

void TrackerView::keyPressEvent(QKeyEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey k = qtKeyToImGui(e->key());
    if (k != ImGuiKey_None) io.AddKeyEvent(k, true);
    if (!e->text().isEmpty())
        io.AddInputCharactersUTF8(e->text().toUtf8().constData());
}

void TrackerView::keyReleaseEvent(QKeyEvent* e) {
    ImGui::SetCurrentContext(m_imgui);
    ImGuiKey k = qtKeyToImGui(e->key());
    if (k != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(k, false);
}
